// 平台实例数据作用域：GI拥有请求与签发密钥，统一维护定义/普通软资源租约。
// 所有账本与UObject访问在游戏线程；World/调用者失效撤销自身需求，终态通知延后且至多一次。
#include "Subsystems/GamePlatformDataSubsystem.h"
#include "Loading/GamePlatformAssetManager.h"
#include "Loading/DataNextTick.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Types/GamePlatformDataLimits.h"
#include "Ownership/DataLeaseTerminalPolicy.h"
#include "Hash/Blake3.h"
#include "Serialization/MemoryWriter.h"

namespace
{
struct FDependencyFrame
{
    FPrimaryAssetId Id;
    TArray<FPrimaryAssetId> Children;
    int32 NextChild = 0;
    bool bIsLoaded = false;
};
// 沿用私有历史名；DefinitionId与ResourcePaths互斥，普通资源分支共用同一取消/通知账本。
struct FDefinitionRequest
{
    FGamePlatformDataLease Lease;
    TStrongObjectPtr<UClass> ExpectedClass;
    TWeakObjectPtr<UObject> Caller;
    TWeakObjectPtr<UWorld> World;
    EGamePlatformDataLifetime Lifetime = EGamePlatformDataLifetime::Instance;
    FGamePlatformDataCompletion Completion;
    TArray<FDependencyFrame> Stack;
    TSet<FPrimaryAssetId> Visited;
    TSet<FPrimaryAssetId> Visiting;
    TArray<FPrimaryAssetId> DemandedAssets;
    bool bIsWaiting = false;
    bool bHasPublishedTerminal = false;
    FString Key() const { return Lease.ScopeId.ToString() + TEXT("/") + Lease.LeaseId.ToString() + TEXT("/") + LexToString(Lease.Generation); }
};
// 校验完整句柄快照中身份与分组；RequestState是可过期的观察值，不参与所有权比较。
bool SameLease(const FGamePlatformDataLease& A, const FGamePlatformDataLease& B)
{
    return A.ScopeId == B.ScopeId && A.LeaseId == B.LeaseId && A.Generation == B.Generation && A.DefinitionId == B.DefinitionId && A.Bundles == B.Bundles && A.ResourcePaths == B.ResourcePaths && A.IssuerProof == B.IssuerProof;
}
/** 私有实例密钥与长度明确的完整租约身份生成摘要；状态快照不入摘要，因此Loading副本释放仍有效。 */
FGuid LeaseProof(const FGamePlatformDataLease& Lease, const FGuid& SecretA, const FGuid& SecretB)
{
    TArray<uint8> Payload;
    FMemoryWriter Writer(Payload);
    FGuid ScopeId = Lease.ScopeId, LeaseId = Lease.LeaseId;
    int64 Generation = Lease.Generation;
    Writer << ScopeId << LeaseId << Generation;
    FString DefinitionId = Lease.DefinitionId.ToString().ToLower();
    Writer << DefinitionId;
    int32 BundleCount = Lease.Bundles.Num(), ResourceCount = Lease.ResourcePaths.Num();
    Writer << BundleCount;
    for (FName Bundle : Lease.Bundles) { FString Name = Bundle.ToString().ToLower(); Writer << Name; }
    Writer << ResourceCount;
    for (const FSoftObjectPath& Path : Lease.ResourcePaths)
    {
        FString Asset = Path.GetAssetPathString().ToLower(), SubPath = Path.GetSubPathString();
        Writer << Asset << SubPath;
    }
    FBlake3 Hash;
    Hash.Update(&SecretA, sizeof(SecretA)); Hash.Update(&SecretB, sizeof(SecretB));
    Hash.Update(Payload.GetData(), static_cast<uint64>(Payload.Num()));
    const FBlake3Hash Digest = Hash.Finalize();
    uint32 Words[4]; FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
    return FGuid(Words[0], Words[1], Words[2], Words[3]);
}
bool CallerBelongsTo(UObject* Caller, UGameInstance* Instance)
{
    return Caller && (Caller == Instance || Caller->GetTypedOuter<UGameInstance>() == Instance ||
        (Caller->GetWorld() && Caller->GetWorld()->GetGameInstance() == Instance));
}
bool IsContextAlive(const FDefinitionRequest& Request)
{
    return Request.Caller.IsValid() && (Request.Lifetime == EGamePlatformDataLifetime::Instance ||
        (Request.World.IsValid() && !Request.World->bIsTearingDown));
}
}
struct FGamePlatformDataScope
{
    FGuid Id;
    int64 NextGeneration = 0;
    bool bIsClosing = false;
    TWeakObjectPtr<UGamePlatformAssetManager> Manager;
    TMap<FGuid, TSharedPtr<FDefinitionRequest>> Requests;
    // 作用域私有签发材料永不导出；完整身份摘要支持严格幂等且无需保存每个已释放请求。
    FGuid IssuerSecretA;
    FGuid IssuerSecretB;
    FGamePlatformResult LastResult;
    int64 TotalAcceptedRequests = 0;
    int64 TotalRejectedRequests = 0;
    int64 TotalSucceededRequests = 0;
    int64 TotalFailedRequests = 0;
    int64 TotalCancelledRequests = 0;
};

IGamePlatformDataService* IGamePlatformDataService::Get(UGameInstance& GameInstance)
{
    check(IsInGameThread());
    return GameInstance.GetSubsystem<UGamePlatformDataSubsystem>();
}
UGamePlatformDataSubsystem::UGamePlatformDataSubsystem() : Scope(MakeUnique<FGamePlatformDataScope>()) {}
UGamePlatformDataSubsystem::UGamePlatformDataSubsystem(FVTableHelper& Helper) : Super(Helper), Scope(MakeUnique<FGamePlatformDataScope>()) {}
UGamePlatformDataSubsystem::~UGamePlatformDataSubsystem() = default;
void UGamePlatformDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Scope->Id = FGuid::NewGuid();
    Scope->IssuerSecretA = FGuid::NewGuid();
    Scope->IssuerSecretB = FGuid::NewGuid();
    Scope->bIsClosing = false;
    Scope->Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::CleanupWorld);
    // 弱对象销毁没有统一销毁事件；只做作用域租约存活检查，不执行同步加载或业务逐帧逻辑。
    OwnerWatchHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::WatchOwners));
}
void UGamePlatformDataSubsystem::Deinitialize()
{
    Scope->bIsClosing = true;
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(OwnerWatchHandle);
    TArray<FGamePlatformDataLease> Leases;
    for (const auto& Pair : Scope->Requests) Leases.Add(Pair.Value->Lease);
    for (const auto& Lease : Leases) ReleaseRequest(Lease, TEXT("游戏实例数据作用域结束。"));
    Scope->Manager.Reset();
    Super::Deinitialize();
}

FGamePlatformDataLease UGamePlatformDataSubsystem::AcquireDefinition(const FPrimaryAssetId& DefinitionId,
    TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass, const TArray<FName>& Bundles,
    EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
    FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    FGamePlatformId LogicalId;
    // 正常UE启动在实例初始化前创建管理器；测试/特殊宿主若早建实例，允许重试取得同一个引擎对象。
    // 这里只查GetIfInitialized，绝不创建替代管理器或替换已配置的进程实例。
    if (!Scope->bIsClosing && !Scope->Manager.IsValid())
        Scope->Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
    OutResult = FGamePlatformResult::Success();
    if (Scope->bIsClosing || !Scope->Id.IsValid())
        OutResult = FGamePlatformResult::Failure(TEXT("ScopeClosed"), TEXT("数据作用域未初始化或正在关闭。"));
    else if (!Scope->Manager.IsValid())
        OutResult = FGamePlatformResult::Failure(TEXT("AssetManagerNotConfigured"), TEXT("引擎未配置GamePlatformAssetManager，不能创建第二个管理器。"));
    else if (!ExpectedClass || ExpectedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidDefinitionClass"), TEXT("预期定义类为空、抽象、废弃或已被替换。"));
    else if (DefinitionId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() || !FGamePlatformId::TryParse(DefinitionId.PrimaryAssetName.ToString(), LogicalId))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidDefinitionId"), TEXT("定义主资产身份非法。"));
    else if (!CallerBelongsTo(WeakCaller.Get(), GetGameInstance()) || !Completion)
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidCaller"), TEXT("弱调用者必须属于本实例，完成回调必须非空。"));
    else if (Lifetime != EGamePlatformDataLifetime::Instance && Lifetime != EGamePlatformDataLifetime::World)
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidLifetime"), TEXT("数据租约期限非法。"));
    else if (Lifetime == EGamePlatformDataLifetime::World && (!WeakCaller->GetWorld() || WeakCaller->GetWorld()->GetGameInstance() != GetGameInstance() || WeakCaller->GetWorld()->bIsTearingDown))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidWorld"), TEXT("世界租约要求存活且属于本实例的调用者世界。"));
    else if (Bundles.Contains(NAME_None))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidBundle"), TEXT("分组集合允许为空，但不能包含None名称。"));
    else if (Scope->NextGeneration == MAX_int64)
        OutResult = FGamePlatformResult::Failure(TEXT("GenerationExhausted"), TEXT("作用域申请代次耗尽，拒绝复用旧代次。"));
    else
    {
        TSet<FName> UniqueBundles;
        for (const FName Bundle : Bundles) UniqueBundles.Add(Bundle);
        if (UniqueBundles.Num() > GamePlatform::Data::Limits::MaxBundlesPerLease)
            OutResult = FGamePlatformResult::Failure(TEXT("TooManyBundles"), TEXT("单租约请求的唯一Asset Bundle数量超过平台安全上限。"));
    }
    if (!OutResult.IsSuccess())
    {
        ++Scope->TotalRejectedRequests;
        Scope->LastResult = OutResult;
        GamePlatform::Data::NextTick([WeakCaller, Callback = MoveTemp(Completion), Result = OutResult]() mutable
        { if (WeakCaller.IsValid() && Callback) Callback(FGamePlatformDataLease(), Result); });
        return {};
    }
    auto Request = MakeShared<FDefinitionRequest>();
    Request->Lease.ScopeId = Scope->Id;
    Request->Lease.LeaseId = FGuid::NewGuid();
    Request->Lease.Generation = ++Scope->NextGeneration;
    Request->Lease.DefinitionId = FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*LogicalId.ToString()));
    for (FName Bundle : Bundles) Request->Lease.Bundles.AddUnique(Bundle);
    Request->Lease.Bundles.Sort(FNameLexicalLess());
    Request->Lease.RequestState = EGamePlatformDataRequestState::Loading;
    Request->Lease.IssuerProof = LeaseProof(Request->Lease, Scope->IssuerSecretA, Scope->IssuerSecretB);
    Request->ExpectedClass.Reset(ExpectedClass.Get());
    Request->Caller = WeakCaller;
    Request->Lifetime = Lifetime;
    if (Lifetime == EGamePlatformDataLifetime::World) Request->World = WeakCaller->GetWorld();
    Request->Completion = MoveTemp(Completion);
    FDependencyFrame Root;
    Root.Id = Request->Lease.DefinitionId;
    Request->Stack.Add(MoveTemp(Root));
    Request->Visiting.Add(Request->Lease.DefinitionId);
    // 租约必须在引擎可能同步完成前发布到作用域。
    Scope->Requests.Add(Request->Lease.LeaseId, Request);
    ++Scope->TotalAcceptedRequests;
    const FGamePlatformDataLease Lease = Request->Lease;
    TWeakObjectPtr<UGamePlatformDataSubsystem> WeakThis(this);
    GamePlatform::Data::NextTick([WeakThis, Lease]() { if (auto* Self = WeakThis.Get()) Self->Advance(Lease); });
    return Lease;
}

void UGamePlatformDataSubsystem::Advance(FGamePlatformDataLease Lease)
{
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found || !SameLease((*Found)->Lease, Lease)) return;
    auto Request = *Found;
    if (Request->bHasPublishedTerminal || Request->bIsWaiting) return;
    if (Scope->bIsClosing || !IsContextAlive(*Request)) { ReleaseRequest(Lease, TEXT("请求上下文已失效。")); return; }
    auto* Manager = Scope->Manager.Get();
    if (!Manager) { Finish(Lease, FGamePlatformResult::Failure(TEXT("AssetManagerUnavailable"), TEXT("资产管理器已失效。"))); return; }
    if (!Lease.ResourcePaths.IsEmpty())
    {
        Request->bIsWaiting = true;
        TWeakObjectPtr<UGamePlatformDataSubsystem> WeakThis(this);
        const FGamePlatformResult Result = Manager->AddResourceDemand(Lease.ResourcePaths, Request->Key(),
            [WeakThis, Lease](FGamePlatformResult LoadedResult)
            { if (auto* Self = WeakThis.Get()) Self->ResourcesReady(Lease, MoveTemp(LoadedResult)); });
        if (!Result.IsSuccess()) { Request->bIsWaiting = false; Finish(Lease, Result); }
        return;
    }
    // 显式栈代替C++递归；每条路径最多128层，单请求最多4096个定义，避免不可信图耗尽栈或内存。
    while (!Request->Stack.IsEmpty())
    {
        auto& Frame = Request->Stack.Last();
        if (!Frame.bIsLoaded)
        {
            const FPrimaryAssetId AssetId = Frame.Id;
            Request->bIsWaiting = true;
            TWeakObjectPtr<UGamePlatformDataSubsystem> WeakThis(this);
            const FGamePlatformResult Result = Manager->AddDemand(AssetId, Request->Key(), Lease.Bundles,
                [WeakThis, Lease, AssetId](FGamePlatformResult LoadedResult)
                { if (auto* Self = WeakThis.Get()) Self->AssetReady(Lease, AssetId, MoveTemp(LoadedResult)); });
            if (!Result.IsSuccess())
            {
                Request->bIsWaiting = false;
                if (Result.Code == TEXT("AssetRegistryNotReady"))
                {
                    // 实例依赖初始化不等于磁盘扫描结束；等下一调度轮再验，不同步阻塞资产发现。
                    GamePlatform::Data::NextTick([WeakThis, Lease]() { if (auto* Self = WeakThis.Get()) Self->Advance(Lease); });
                }
                else Finish(Lease, Result);
            }
            else Request->DemandedAssets.Add(AssetId);
            return;
        }
        if (Frame.NextChild >= Frame.Children.Num())
        {
            Request->Visiting.Remove(Frame.Id);
            Request->Visited.Add(Frame.Id);
            Request->Stack.Pop();
            continue;
        }
        const FPrimaryAssetId Child = Frame.Children[Frame.NextChild++];
        if (Request->Visiting.Contains(Child))
        { Finish(Lease, FGamePlatformResult::Failure(TEXT("DependencyCycle"), FString::Printf(TEXT("必需定义形成循环：%s。"), *Child.ToString()))); return; }
        if (Request->Visited.Contains(Child)) continue;
        if (Request->Stack.Num() >= GamePlatform::Data::Limits::MaxDependencyDepth ||
            Request->DemandedAssets.Num() >= GamePlatform::Data::Limits::MaxDefinitionsPerRequest)
        { Finish(Lease, FGamePlatformResult::Failure(TEXT("DependencyGraphLimit"), TEXT("定义依赖超过平台统一的深度或唯一节点安全上限。"))); return; }
        FDependencyFrame ChildFrame;
        ChildFrame.Id = Child;
        Request->Stack.Add(MoveTemp(ChildFrame));
        Request->Visiting.Add(Child);
    }
    Finish(Lease, FGamePlatformResult::Success());
}

void UGamePlatformDataSubsystem::AssetReady(FGamePlatformDataLease Lease, FPrimaryAssetId AssetId, FGamePlatformResult Result)
{
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found || !SameLease((*Found)->Lease, Lease)) return;
    auto Request = *Found;
    if (Request->bHasPublishedTerminal || !Request->bIsWaiting || Request->Stack.IsEmpty() || Request->Stack.Last().Id != AssetId) return;
    Request->bIsWaiting = false;
    if (!IsContextAlive(*Request) || Scope->bIsClosing) { ReleaseRequest(Lease, TEXT("加载返回时请求上下文已失效。")); return; }
    if (!Result.IsSuccess()) { Finish(Lease, Result); return; }
    auto* Manager = Scope->Manager.Get();
    const auto* Definition = Manager ? Cast<UGamePlatformDefinitionBase>(Manager->GetPrimaryAssetObject(AssetId)) : nullptr;
    if (!Definition || Definition->GetPrimaryAssetId() != AssetId ||
        (AssetId == Lease.DefinitionId && !Definition->IsA(Request->ExpectedClass.Get())))
    { Finish(Lease, FGamePlatformResult::Failure(TEXT("DefinitionTypeMismatch"), TEXT("实际加载对象的定义类型或稳定身份不匹配。"))); return; }
    Result = Definition->ValidateDefinition();
    if (!Result.IsSuccess()) { Finish(Lease, Result); return; }
    Request->Stack.Last().Children = Definition->RequiredDefinitions;
    Request->Stack.Last().bIsLoaded = true;
    Advance(Lease);
}

void UGamePlatformDataSubsystem::Finish(FGamePlatformDataLease Lease, FGamePlatformResult Result)
{
    // 终态在下一轮提交，期间Release可先赢得取消竞争。缓存与加载完成没有同步重入差异。
    TWeakObjectPtr<UGamePlatformDataSubsystem> WeakThis(this);
    GamePlatform::Data::NextTick([WeakThis, Lease, Result]() mutable
    {
        auto* Self = WeakThis.Get();
        if (!Self) return;
        const auto* Found = Self->Scope->Requests.Find(Lease.LeaseId);
        if (!Found || !SameLease((*Found)->Lease, Lease)) return;
        auto Request = *Found;
        if (Request->bHasPublishedTerminal) return;
        if (!IsContextAlive(*Request) || Self->Scope->bIsClosing) { Self->ReleaseRequest(Lease, TEXT("终态发布前上下文失效。")); return; }
        Request->bHasPublishedTerminal = true;
        Request->Lease.RequestState = Result.IsSuccess() ? EGamePlatformDataRequestState::Succeeded : EGamePlatformDataRequestState::Failed;
        if (Result.IsSuccess()) ++Self->Scope->TotalSucceededRequests;
        else ++Self->Scope->TotalFailedRequests;
        Request->Stack.Empty(); Request->Visiting.Empty(); Request->Visited.Empty();
        if (!Result.IsSuccess())
        {
            Self->Scope->LastResult = Result;
            if (auto* Manager = Self->Scope->Manager.Get())
            {
                for (const auto& Id : Request->DemandedAssets) Manager->RemoveDemand(Id, Request->Key());
                if (!Request->Lease.ResourcePaths.IsEmpty()) Manager->RemoveResourceDemand(Request->Key());
            }
            Request->DemandedAssets.Empty();
        }
        auto Callback = MoveTemp(Request->Completion);
        if (Callback && Request->Caller.IsValid()) Callback(Request->Lease, Result);
    });
}

void UGamePlatformDataSubsystem::ReleaseRequest(FGamePlatformDataLease Lease, const FString& Reason)
{
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found || !SameLease((*Found)->Lease, Lease)) return;
    auto Request = *Found;
    Scope->Requests.Remove(Lease.LeaseId); // 先撤销，再调用可能同步取消的引擎操作。
    if (auto* Manager = Scope->Manager.Get())
    {
        for (const auto& Id : Request->DemandedAssets) Manager->RemoveDemand(Id, Request->Key());
        if (!Lease.ResourcePaths.IsEmpty()) Manager->RemoveResourceDemand(Request->Key());
    }
    if (!Request->bHasPublishedTerminal)
    {
        Request->bHasPublishedTerminal = true;
        Request->Lease.RequestState = EGamePlatformDataRequestState::Cancelled;
        const FGamePlatformResult Result = FGamePlatformResult::Cancelled(Reason);
        ++Scope->TotalCancelledRequests;
        Scope->LastResult = Result;
        GamePlatform::Data::NextTick([Request, Result]() mutable
        {
            auto Callback = MoveTemp(Request->Completion);
            if (Request->Caller.IsValid() && Callback) Callback(Request->Lease, Result);
        });
    }
}

FGamePlatformResult UGamePlatformDataSubsystem::ReleaseDefinition(const FGamePlatformDataLease& Lease)
{
    check(IsInGameThread());
    if (!Lease.IsValid() || Lease.ScopeId != Scope->Id)
        return FGamePlatformResult::Failure(TEXT("InvalidLeaseScope"), TEXT("不能释放无效或其他实例的租约。"));
    if (!HasAuthenticLease(Lease))
        return FGamePlatformResult::Failure(TEXT("InvalidLeaseProof"), TEXT("租约不是当前实例签发的完整身份，拒绝伪造或篡改。"));
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found && GamePlatform::Data::CanTreatMissingLeaseAsReleased(true, static_cast<uint64>(Lease.Generation), static_cast<uint64>(Scope->NextGeneration))) return FGamePlatformResult::Success();
    if (!Found || !SameLease((*Found)->Lease, Lease))
        return FGamePlatformResult::Failure(TEXT("InvalidLeaseGeneration"), TEXT("租约未签发或身份、分组、代次被修改。"));
    ReleaseRequest(Lease, TEXT("调用者释放了尚未完成的数据请求。"));
    return FGamePlatformResult::Success();
}
const UGamePlatformDefinitionBase* UGamePlatformDataSubsystem::GetLoadedDefinition(const FGamePlatformDataLease& Lease) const
{
    check(IsInGameThread());
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found || !SameLease((*Found)->Lease, Lease) || (*Found)->Lease.RequestState != EGamePlatformDataRequestState::Succeeded || !IsContextAlive(**Found)) return nullptr;
    auto* Manager = Scope->Manager.Get();
    const auto* Definition = Manager ? Cast<UGamePlatformDefinitionBase>(Manager->GetPrimaryAssetObject(Lease.DefinitionId)) : nullptr;
    return Definition && Definition->IsA((*Found)->ExpectedClass.Get()) ? Definition : nullptr;
}
EGamePlatformDataRequestState UGamePlatformDataSubsystem::GetLeaseState(const FGamePlatformDataLease& Lease) const
{
    check(IsInGameThread());
    if (const auto* Found = Scope->Requests.Find(Lease.LeaseId); Found && SameLease((*Found)->Lease, Lease)) return (*Found)->Lease.RequestState;
    if (HasAuthenticLease(Lease) && GamePlatform::Data::CanTreatMissingLeaseAsReleased(true, static_cast<uint64>(Lease.Generation), static_cast<uint64>(Scope->NextGeneration))) return EGamePlatformDataRequestState::Released;
    return EGamePlatformDataRequestState::Invalid;
}
FGamePlatformDataDiagnostics UGamePlatformDataSubsystem::GetDiagnostics() const
{
    check(IsInGameThread());
    FGamePlatformDataDiagnostics Result;
    Result.ScopeId = Scope->Id;
    Result.LastResult = Scope->LastResult;
    Result.ReleasedLeaseRecords = 0; // 兼容诊断字段；真实签发证明取代无界历史表。
    Result.TotalAcceptedRequests = Scope->TotalAcceptedRequests;
    Result.TotalRejectedRequests = Scope->TotalRejectedRequests;
    Result.TotalSucceededRequests = Scope->TotalSucceededRequests;
    Result.TotalFailedRequests = Scope->TotalFailedRequests;
    Result.TotalCancelledRequests = Scope->TotalCancelledRequests;
    TSet<FPrimaryAssetId> Definitions;
    TSet<FName> Bundles;
    for (const auto& Pair : Scope->Requests)
    {
        const auto& Request = *Pair.Value;
        if (Request.Lease.DefinitionId.IsValid()) Definitions.Add(Request.Lease.DefinitionId);
        for (const FPrimaryAssetId& Id : Request.DemandedAssets) Definitions.Add(Id);
        for (const FName Bundle : Request.Lease.Bundles) Bundles.Add(Bundle);
        if (Request.Lease.RequestState == EGamePlatformDataRequestState::Loading) ++Result.PendingRequests;
        else if (Request.Lease.RequestState == EGamePlatformDataRequestState::Succeeded) ++Result.ActiveLeases;
        else ++Result.TerminalRequests;
    }
    Result.UniqueTrackedDefinitions = Definitions.Num();
    Result.UniqueRequestedBundles = Bundles.Num();
    return Result;
}
void UGamePlatformDataSubsystem::CleanupWorld(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    TArray<FGamePlatformDataLease> Leases;
    for (const auto& Pair : Scope->Requests)
        if (Pair.Value->Lifetime == EGamePlatformDataLifetime::World && Pair.Value->World == World) Leases.Add(Pair.Value->Lease);
    for (const auto& Lease : Leases) ReleaseRequest(Lease, TEXT("租约绑定的世界正在清理。"));
}
bool UGamePlatformDataSubsystem::WatchOwners(float DeltaSeconds)
{
    TArray<FGamePlatformDataLease> Leases;
    for (const auto& Pair : Scope->Requests) if (!IsContextAlive(*Pair.Value)) Leases.Add(Pair.Value->Lease);
    for (const auto& Lease : Leases) ReleaseRequest(Lease, TEXT("租约调用者或世界已销毁。"));
    return !Scope->bIsClosing;
}

/** 普通资源请求复用同一个实例账本、代次与完成协议；不创建虚假的主资产或并行管理器。 */
FGamePlatformDataLease UGamePlatformDataSubsystem::AcquireResources(const TArray<FSoftObjectPath>& ResourcePaths,
    EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
    FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Scope->bIsClosing && !Scope->Manager.IsValid()) Scope->Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
    OutResult = FGamePlatformResult::Success();
    TArray<FSoftObjectPath> Paths;
    // 先限制原始输入，再做去重和排序；大量重复路径同样不能绕过申请处理成本上限。
    if (ResourcePaths.Num() <= GamePlatform::Data::Limits::MaxDefinitionsPerRequest)
        for (const FSoftObjectPath& Path : ResourcePaths) Paths.AddUnique(Path);
    Paths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
    if (Scope->bIsClosing || !Scope->Id.IsValid()) OutResult = FGamePlatformResult::Failure(TEXT("ScopeClosed"), TEXT("数据作用域关闭，不能申请资源。"));
    else if (!Scope->Manager.IsValid()) OutResult = FGamePlatformResult::Failure(TEXT("AssetManagerNotConfigured"), TEXT("必须使用正式唯一GamePlatformAssetManager。"));
    else if (Paths.IsEmpty() || Paths.Num() > GamePlatform::Data::Limits::MaxDefinitionsPerRequest || Paths.ContainsByPredicate([](const FSoftObjectPath& Path) { return !Path.IsValid(); }))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidResourcePaths"), TEXT("资源路径必须非空、有效且不超过统一请求节点安全上限。"));
    else if (!CallerBelongsTo(WeakCaller.Get(), GetGameInstance()) || !Completion) OutResult = FGamePlatformResult::Failure(TEXT("InvalidCaller"), TEXT("有效弱调用者必须属于本实例且提供完成回调。"));
    else if (Lifetime != EGamePlatformDataLifetime::Instance && Lifetime != EGamePlatformDataLifetime::World) OutResult = FGamePlatformResult::Failure(TEXT("InvalidLifetime"), TEXT("资源期限非法。"));
    else if (Lifetime == EGamePlatformDataLifetime::World && (!WeakCaller->GetWorld() || WeakCaller->GetWorld()->GetGameInstance() != GetGameInstance() || WeakCaller->GetWorld()->bIsTearingDown))
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidWorld"), TEXT("世界资源租约要求同实例的存活世界。"));
    else if (Scope->NextGeneration == MAX_int64) OutResult = FGamePlatformResult::Failure(TEXT("GenerationExhausted"), TEXT("拒绝复用耗尽的申请代次。"));
    if (!OutResult.IsSuccess())
    {
        ++Scope->TotalRejectedRequests; Scope->LastResult = OutResult;
        GamePlatform::Data::NextTick([WeakCaller, Callback = MoveTemp(Completion), Result = OutResult]() mutable
        { if (WeakCaller.IsValid() && Callback) Callback({}, Result); });
        return {};
    }
    auto Request = MakeShared<FDefinitionRequest>();
    Request->Lease.ScopeId = Scope->Id; Request->Lease.LeaseId = FGuid::NewGuid();
    Request->Lease.Generation = ++Scope->NextGeneration; Request->Lease.ResourcePaths = MoveTemp(Paths);
    Request->Lease.RequestState = EGamePlatformDataRequestState::Loading;
    Request->Lease.IssuerProof = LeaseProof(Request->Lease, Scope->IssuerSecretA, Scope->IssuerSecretB);
    Request->Caller = WeakCaller; Request->Lifetime = Lifetime;
    if (Lifetime == EGamePlatformDataLifetime::World) Request->World = WeakCaller->GetWorld();
    Request->Completion = MoveTemp(Completion);
    const FGamePlatformDataLease Lease = Request->Lease;
    Scope->Requests.Add(Lease.LeaseId, Request); ++Scope->TotalAcceptedRequests;
    TWeakObjectPtr<UGamePlatformDataSubsystem> WeakThis(this);
    GamePlatform::Data::NextTick([WeakThis, Lease]() { if (auto* Self = WeakThis.Get()) Self->Advance(Lease); });
    return Lease;
}
void UGamePlatformDataSubsystem::ResourcesReady(FGamePlatformDataLease Lease, FGamePlatformResult Result)
{
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
    if (!Found || !SameLease((*Found)->Lease, Lease) || (*Found)->bHasPublishedTerminal || !(*Found)->bIsWaiting) return;
    (*Found)->bIsWaiting = false;
    if (Scope->bIsClosing || !IsContextAlive(**Found)) { ReleaseRequest(Lease, TEXT("普通资源完成时调用者/世界失效。")); return; }
    Finish(Lease, Result);
}
bool UGamePlatformDataSubsystem::HasAuthenticLease(const FGamePlatformDataLease& Lease) const
{
    // 不可信的复制句柄在序列化签发证明前先受相同结构上限约束。
    return Lease.IsValid() && Lease.Bundles.Num() <= GamePlatform::Data::Limits::MaxBundlesPerLease &&
        Lease.ResourcePaths.Num() <= GamePlatform::Data::Limits::MaxDefinitionsPerRequest &&
        Lease.ScopeId == Scope->Id && Lease.Generation <= Scope->NextGeneration &&
        Lease.IssuerProof == LeaseProof(Lease, Scope->IssuerSecretA, Scope->IssuerSecretB);
}
FGamePlatformResult UGamePlatformDataSubsystem::ReleaseResources(const FGamePlatformDataLease& Lease)
{
    if (Lease.ResourcePaths.IsEmpty()) return FGamePlatformResult::Failure(TEXT("InvalidResourceLease"), TEXT("释放普通资源必须使用普通资源租约。"));
    return ReleaseDefinition(Lease);
}
