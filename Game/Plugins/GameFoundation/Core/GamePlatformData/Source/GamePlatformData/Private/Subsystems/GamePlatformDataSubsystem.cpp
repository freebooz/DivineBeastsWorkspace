#include "Subsystems/GamePlatformDataSubsystem.h"
#include "Loading/GamePlatformAssetManager.h"
#include "Loading/DataNextTick.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
struct FDependencyFrame
{
    FPrimaryAssetId Id;
    TArray<FPrimaryAssetId> Children;
    int32 NextChild = 0;
    bool bIsLoaded = false;
};
struct FDefinitionRequest
{
    FGamePlatformDataLease Lease;
    TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass;
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
    return A.ScopeId == B.ScopeId && A.LeaseId == B.LeaseId && A.Generation == B.Generation && A.DefinitionId == B.DefinitionId && A.Bundles == B.Bundles;
}
bool CallerBelongsTo(UObject* Caller, UGameInstance* Instance)
{
    return Caller && (Caller == Instance || Caller->GetTypedOuter<UGameInstance>() == Instance ||
        (Caller->GetWorld() && Caller->GetWorld()->GetGameInstance() == Instance));
}
bool IsContextAlive(const FDefinitionRequest& Request)
{
    return Request.Caller.IsValid() && (Request.Lifetime == EGamePlatformDataLifetime::Instance || Request.World.IsValid());
}
}
struct FGamePlatformDataScope
{
    FGuid Id;
    int64 NextGeneration = 0;
    bool bIsClosing = false;
    TWeakObjectPtr<UGamePlatformAssetManager> Manager;
    TMap<FGuid, TSharedPtr<FDefinitionRequest>> Requests;
    // 幂等释放仍核对真实签发记录，不能把伪造的未知LeaseId判为已经释放。
    TMap<FGuid, FGamePlatformDataLease> ReleasedLeases;
    FGamePlatformResult LastResult;
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
    if (!OutResult.IsSuccess())
    {
        Scope->LastResult = OutResult;
        GamePlatform::Data::NextTick([WeakCaller, Callback = MoveTemp(Completion), Result = OutResult]() mutable
        { if (WeakCaller.IsValid() && Callback) Callback(FGamePlatformDataLease(), Result); });
        return {};
    }
    auto Request = MakeShared<FDefinitionRequest>();
    Request->Lease.ScopeId = Scope->Id;
    Request->Lease.LeaseId = FGuid::NewGuid();
    Request->Lease.Generation = ++Scope->NextGeneration;
    Request->Lease.DefinitionId = DefinitionId;
    for (FName Bundle : Bundles) Request->Lease.Bundles.AddUnique(Bundle);
    Request->Lease.Bundles.Sort(FNameLexicalLess());
    Request->Lease.RequestState = EGamePlatformDataRequestState::Loading;
    Request->ExpectedClass = ExpectedClass;
    Request->Caller = WeakCaller;
    Request->Lifetime = Lifetime;
    if (Lifetime == EGamePlatformDataLifetime::World) Request->World = WeakCaller->GetWorld();
    Request->Completion = MoveTemp(Completion);
    FDependencyFrame Root;
    Root.Id = DefinitionId;
    Request->Stack.Add(MoveTemp(Root));
    Request->Visiting.Add(DefinitionId);
    // 租约必须在引擎可能同步完成前发布到作用域。
    Scope->Requests.Add(Request->Lease.LeaseId, Request);
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
            if (!Result.IsSuccess()) { Request->bIsWaiting = false; Finish(Lease, Result); }
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
        if (Request->Stack.Num() >= 128 || Request->DemandedAssets.Num() >= 4096)
        { Finish(Lease, FGamePlatformResult::Failure(TEXT("DependencyGraphLimit"), TEXT("定义依赖超过128层或4096个唯一节点的安全上限。"))); return; }
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
        (AssetId == Lease.DefinitionId && !Definition->IsA(Request->ExpectedClass)))
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
        Request->Stack.Empty(); Request->Visiting.Empty(); Request->Visited.Empty();
        if (!Result.IsSuccess())
        {
            Self->Scope->LastResult = Result;
            if (auto* Manager = Self->Scope->Manager.Get())
                for (const auto& Id : Request->DemandedAssets) Manager->RemoveDemand(Id, Request->Key());
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
    Scope->ReleasedLeases.Add(Lease.LeaseId, Lease);
    if (auto* Manager = Scope->Manager.Get())
        for (const auto& Id : Request->DemandedAssets) Manager->RemoveDemand(Id, Request->Key());
    if (!Request->bHasPublishedTerminal)
    {
        Request->bHasPublishedTerminal = true;
        Request->Lease.RequestState = EGamePlatformDataRequestState::Cancelled;
        const FGamePlatformResult Result = FGamePlatformResult::Cancelled(Reason);
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
    if (const auto* Released = Scope->ReleasedLeases.Find(Lease.LeaseId); Released && SameLease(*Released, Lease)) return FGamePlatformResult::Success();
    const auto* Found = Scope->Requests.Find(Lease.LeaseId);
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
    return Definition && Definition->IsA((*Found)->ExpectedClass) ? Definition : nullptr;
}
EGamePlatformDataRequestState UGamePlatformDataSubsystem::GetLeaseState(const FGamePlatformDataLease& Lease) const
{
    check(IsInGameThread());
    if (const auto* Found = Scope->Requests.Find(Lease.LeaseId); Found && SameLease((*Found)->Lease, Lease)) return (*Found)->Lease.RequestState;
    if (const auto* Released = Scope->ReleasedLeases.Find(Lease.LeaseId); Released && SameLease(*Released, Lease)) return EGamePlatformDataRequestState::Released;
    return EGamePlatformDataRequestState::Invalid;
}
FGamePlatformDataDiagnostics UGamePlatformDataSubsystem::GetDiagnostics() const
{
    check(IsInGameThread());
    FGamePlatformDataDiagnostics Result;
    Result.ScopeId = Scope->Id; Result.LastResult = Scope->LastResult;
    for (const auto& Pair : Scope->Requests)
    {
        if (Pair.Value->Lease.RequestState == EGamePlatformDataRequestState::Loading) ++Result.PendingRequests;
        else if (Pair.Value->Lease.RequestState == EGamePlatformDataRequestState::Succeeded) ++Result.ActiveLeases;
        else ++Result.TerminalRequests;
    }
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
