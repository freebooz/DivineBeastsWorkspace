#include "Subsystems/GamePlatformPCGWorldSubsystem.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Services/GamePlatformPCGInspection.h"
#include "Validation/PCGPolicy.h"
#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"

namespace Policy = GamePlatformPCGPolicy;
struct FPCGOwnedRequest
{
    FGamePlatformPCGRequest Input;
    FGamePlatformPCGSnapshot Snapshot;
    Policy::Lifecycle Lifecycle;
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UPCGComponent> Component;
    FDelegateHandle GeneratedDelegate,CancelledDelegate;
    double GenerateStarted = 0;
    double CleanupStarted = 0;
    bool bNativeGenerated = false;
    bool bNativeCancelled = false;
    bool bCleanupSubmitted = false;
    bool bSourceCreated = false;
    bool bDirty = true;
};
struct FPCGSubscriber
{
    FGamePlatformPCGHandle Handle;
    TWeakObjectPtr<UObject> Owner;
    TFunction<void(const FGamePlatformPCGSnapshot&)> Callback;
};
struct FGamePlatformPCGWorldScope
{
    FGuid Id = FGuid::NewGuid();
    uint64 Generation = 0;
    TMap<FGuid,TSharedPtr<FPCGOwnedRequest>> Requests;
    TMap<FGuid,TSharedPtr<FPCGSubscriber>> Subscribers;
    bool bStopping = false;
    bool bProcessing = false;
};
static FGamePlatformResult PCGFailure(FName Code)
{ return FGamePlatformResult::Failure(Code,TEXT("PCG请求失败，检查用途、世界基础输入和请求快照；不视为已清理")); }
static bool HasBaseInputs(const FGamePlatformWorldReadinessSnapshot& State)
{
    return State.bWorldObjectValid && State.bDefinitionLoaded && State.bMapIdentityMatched && State.bSessionContextMatched &&
        State.bWorldNotTearingDown && State.Context.WorldId.IsValid() && State.Context.ContextGeneration.IsValid() &&
        State.Context.ReadinessState != EGamePlatformWorldReadiness::Invalidated && State.Context.ReadinessState != EGamePlatformWorldReadiness::Failed;
}
UGamePlatformPCGWorldSubsystem::UGamePlatformPCGWorldSubsystem() = default;
UGamePlatformPCGWorldSubsystem::UGamePlatformPCGWorldSubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformPCGWorldSubsystem::~UGamePlatformPCGWorldSubsystem() = default;
IGamePlatformPCGService* IGamePlatformPCGService::Get(UWorld& World)
{ check(IsInGameThread()); return World.GetSubsystem<UGamePlatformPCGWorldSubsystem>(); }
bool UGamePlatformPCGWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game || Type == EWorldType::PIE; }
void UGamePlatformPCGWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Scope = MakeUnique<FGamePlatformPCGWorldScope>();
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Tick),0.05f);
}
FGamePlatformResult UGamePlatformPCGWorldSubsystem::ValidateProfile(const UGamePlatformPCGProfileDefinition& Profile) const
{
    check(IsInGameThread()); const auto Result = Profile.ValidateDefinition(); if (!Result.IsSuccess()) { return Result; }
    if (!Scope || Scope->bStopping || !GetWorld() || GetWorld()->bIsTearingDown) { return PCGFailure(TEXT("WorldUnavailable")); }
    if (GetWorld()->GetNetMode() == NM_DedicatedServer || GetWorld()->GetNetMode() == NM_ListenServer)
    { return FGamePlatformResult::Unsupported(TEXT("ServerCosmeticForbidden"),TEXT("首版仅客户端和明确本地单机装饰，服务器不执行装饰")); }
    if (Profile.ExecutionPolicy != EGamePlatformPCGExecutionPolicy::RuntimeCosmetic)
    { return FGamePlatformResult::Unsupported(TEXT("StaticGenerationEditorOnly"),TEXT("静态碰撞仅编辑器生成并保存，运行期禁止重生成")); }
    auto* WorldService = IGamePlatformWorldService::Get(*GetWorld());
    if (!WorldService || !HasBaseInputs(WorldService->GetReadiness())) { return PCGFailure(TEXT("BaseWorldInputsUnavailable")); }
    return FGamePlatformResult::Success();
}
FGamePlatformPCGHandle UGamePlatformPCGWorldSubsystem::RequestGeneration(const FGamePlatformPCGRequest& Input,FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    auto* World = GetWorld(); auto* Instance = World ? World->GetGameInstance() : nullptr;
    if (!Scope || Scope->bStopping || Scope->bProcessing || !World || !Instance || World->bIsTearingDown || IsRunningCommandlet())
    { OutResult = PCGFailure(TEXT("WorldUnavailable")); return {}; }
    if (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone)
    { OutResult = FGamePlatformResult::Unsupported(TEXT("ServerCosmeticForbidden"),TEXT("服务器不执行运行时装饰")); return {}; }
    if (!Input.ProfileId.IsValid() || !Input.RegionId.IsValid() || !Input.Owner.IsValid() || Input.Owner->GetWorld() != World || Input.CenterCm.ContainsNaN())
    { OutResult = PCGFailure(TEXT("InvalidRequest")); return {}; }
    auto* WorldService = IGamePlatformWorldService::Get(*World);
    auto* Data = IGamePlatformDataService::Get(*Instance);
    if (!WorldService || !Data) { OutResult = PCGFailure(TEXT("PrerequisiteUnavailable")); return {}; }
    const auto Facts = WorldService->GetReadiness(); FGamePlatformId Region;
    if (!HasBaseInputs(Facts) || Facts.Context.ContextGeneration != Input.ContextGeneration ||
        !WorldService->QueryRegion(Input.CenterCm,Region).IsSuccess() || Region != Input.RegionId)
    { OutResult = PCGFailure(TEXT("WorldOrRegionMismatch")); return {}; }
    int32 Active = 0; for (const auto& Pair : Scope->Requests) { Active += Pair.Value->Lifecycle.Phase != Policy::Phase::Cleaned; }
    if (Active >= 4 || Scope->Requests.Num() >= 128)
    { OutResult = PCGFailure(TEXT("RequestBudgetExceeded")); return {}; }
    auto Request = MakeShared<FPCGOwnedRequest>(); Request->Input = Input;
    Request->Snapshot.Handle = {Scope->Id,FGuid::NewGuid(),Input.ContextGeneration,++Scope->Generation};
    Request->Snapshot.ProfileId = Input.ProfileId; Request->Snapshot.RegionId = Input.RegionId;
    Request->Snapshot.StartedSeconds = FPlatformTime::Seconds(); Request->Snapshot.DeadlineSeconds = Request->Snapshot.StartedSeconds + 30;
    Request->Lifecycle.Begin(Request->Snapshot.Handle.ExecutionGeneration);
    Scope->Requests.Add(Request->Snapshot.Handle.OperationId,Request);
    // 实例租约防止World自动释放早于原生任务清理；仅本门面持有并在原生输出归零后归还。
    Request->Lease = Data->AcquireDefinition(Input.ProfileId,UGamePlatformPCGProfileDefinition::StaticClass(),{TEXT("PCGGeneration")},
        EGamePlatformDataLifetime::Instance,Instance,
        [WeakThis = TWeakObjectPtr<UGamePlatformPCGWorldSubsystem>(this),Id = Request->Snapshot.Handle.OperationId](const auto&,const auto& Result)
        {
            auto* Self = WeakThis.Get(); if (!Self || !Self->Scope) { return; }
            auto* Found = Self->Scope->Requests.Find(Id);
            if (Found && (*Found)->Lifecycle.Phase == Policy::Phase::Loading && !Result.IsSuccess())
            { (*Found)->Snapshot.Result = Result; (*Found)->Lifecycle.Stop(Policy::Outcome::Failed); (*Found)->bDirty = true; }
        },OutResult);
    if (!OutResult.IsSuccess())
    { if (Request->Lease.IsValid()) { Data->ReleaseDefinition(Request->Lease); } Scope->Requests.Remove(Request->Snapshot.Handle.OperationId); return {}; }
    return Request->Snapshot.Handle;
}
FGamePlatformResult UGamePlatformPCGWorldSubsystem::CancelGeneration(const FGamePlatformPCGHandle& Handle)
{
    check(IsInGameThread()); auto* Found = Scope ? Scope->Requests.Find(Handle.OperationId) : nullptr;
    if (!Found || !(Handle == (*Found)->Snapshot.Handle)) { return PCGFailure(TEXT("StaleRequest")); }
    if (Scope->bProcessing) { return PCGFailure(TEXT("ReentrantMutation")); }
    if ((*Found)->Lifecycle.Outcome == Policy::Outcome::Pending)
    { (*Found)->Lifecycle.Stop(Policy::Outcome::Cancelled); (*Found)->Snapshot.Result = FGamePlatformResult::Cancelled(TEXT("本地生成取消，等待清理")); BeginCleanup(Handle.OperationId); }
    return FGamePlatformResult::Success();
}
FGamePlatformResult UGamePlatformPCGWorldSubsystem::ReleaseGeneration(const FGamePlatformPCGHandle& Handle)
{
    const auto Result = CancelGeneration(Handle); if (!Result.IsSuccess()) { return Result; }
    auto& Request = *Scope->Requests.FindChecked(Handle.OperationId);
    if (Request.Lifecycle.BeginCleanup()) { BeginCleanup(Handle.OperationId); }
    return FGamePlatformResult::Success();
}
FGamePlatformPCGSnapshot UGamePlatformPCGWorldSubsystem::GetGenerationSnapshot(const FGamePlatformPCGHandle& Handle) const
{
    check(IsInGameThread()); auto* Found = Scope ? Scope->Requests.Find(Handle.OperationId) : nullptr;
    if (!Found || !(Handle == (*Found)->Snapshot.Handle)) { FGamePlatformPCGSnapshot Invalid; Invalid.Result = PCGFailure(TEXT("StaleRequest")); return Invalid; }
    auto Snapshot = (*Found)->Snapshot;
    Snapshot.Phase = static_cast<EGamePlatformPCGPhase>((*Found)->Lifecycle.Phase);
    Snapshot.Outcome = static_cast<EGamePlatformPCGOutcome>((*Found)->Lifecycle.Outcome);
    Snapshot.bIsResultValid = (*Found)->Lifecycle.Phase == Policy::Phase::Retained && (*Found)->Component.IsValid() &&
        (*Found)->Input.Owner.IsValid() && GetWorld() && !GetWorld()->bIsTearingDown;
    return Snapshot;
}
FGamePlatformPCGSubscription UGamePlatformPCGWorldSubsystem::SubscribeGeneration(const FGamePlatformPCGHandle& Handle,
    TWeakObjectPtr<UObject> Owner,TFunction<void(const FGamePlatformPCGSnapshot&)> Callback)
{
    check(IsInGameThread()); auto* Found = Scope ? Scope->Requests.Find(Handle.OperationId) : nullptr;
    if (!Found || !(Handle == (*Found)->Snapshot.Handle) || !Owner.IsValid() || Owner->GetWorld() != GetWorld() || !Callback) { return {}; }
    auto Subscriber = MakeShared<FPCGSubscriber>(); Subscriber->Handle = Handle; Subscriber->Owner = Owner; Subscriber->Callback = MoveTemp(Callback);
    const FGuid Id = FGuid::NewGuid(); Scope->Subscribers.Add(Id,Subscriber); (*Found)->bDirty = true; return {Scope->Id,Id};
}
bool UGamePlatformPCGWorldSubsystem::UnsubscribeGeneration(const FGamePlatformPCGSubscription& Subscription)
{ check(IsInGameThread()); return Scope && Subscription.OwnerScopeId == Scope->Id && Scope->Subscribers.Remove(Subscription.SubscriptionId) != 0; }
void UGamePlatformPCGWorldSubsystem::OnNativeSignal(FGuid Id,TWeakObjectPtr<UPCGComponent> Component,bool bGenerated)
{
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread,[WeakThis = TWeakObjectPtr<UGamePlatformPCGWorldSubsystem>(this),Id,Component,bGenerated]()
        { if (auto* Self = WeakThis.Get()) { Self->OnNativeSignal(Id,Component,bGenerated); } }); return;
    }
    auto* Found = Scope ? Scope->Requests.Find(Id) : nullptr;
    if (!Found || (*Found)->Component != Component || !Component.IsValid()) { return; }
    if (bGenerated) { (*Found)->bNativeGenerated = true; } else { (*Found)->bNativeCancelled = true; }
    // 委托只记录事实；不在原生生成/取消栈中销毁、清理或复用组件。
}
void UGamePlatformPCGWorldSubsystem::BeginCleanup(const FGuid& Id)
{
    auto& Request = *Scope->Requests.FindChecked(Id);
    Request.Lifecycle.BeginCleanup();
    if (Request.CleanupStarted == 0) { Request.CleanupStarted = FPlatformTime::Seconds(); Request.bDirty = true; }
    if (auto* Component = Request.Component.Get())
    {
        Component->CancelGeneration();
        if (!Component->IsGenerating() && !Component->IsCleaningUp() && Component->AreManagedResourcesAccessible() && !Request.bCleanupSubmitted)
        {
            // 有界非分区首版同步移除自己的受管输出，不安排跨世界拆卸期的异步清理任务。
            Component->CleanupLocalImmediate(true); Request.bCleanupSubmitted = true;
        }
    }
}

void UGamePlatformPCGWorldSubsystem::ProcessRequest(const FGuid& Id)
{
    auto& Request = *Scope->Requests.FindChecked(Id);
    if (Request.Lifecycle.Phase == Policy::Phase::Cleaned) { return; }
    auto* World = GetWorld(); auto* Instance = World ? World->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    auto* WorldService = World ? IGamePlatformWorldService::Get(*World) : nullptr;
    const double Now = FPlatformTime::Seconds();
    if (Request.Lifecycle.Phase != Policy::Phase::Cleaning)
    {
        FGamePlatformId Region; const auto Facts = WorldService ? WorldService->GetReadiness() : FGamePlatformWorldReadinessSnapshot{};
        if (!Request.Input.Owner.IsValid() || !HasBaseInputs(Facts) || Facts.Context.ContextGeneration != Request.Input.ContextGeneration ||
            !WorldService->QueryRegion(Request.Input.CenterCm,Region).IsSuccess() || Region != Request.Input.RegionId)
        {
            Request.Snapshot.Result = PCGFailure(TEXT("ContextInvalidated")); Request.Lifecycle.Stop(Policy::Outcome::Failed);
            BeginCleanup(Id);
        }
        else if (Request.Lifecycle.Outcome == Policy::Outcome::Pending && Now >= Request.Snapshot.DeadlineSeconds)
        { Request.Lifecycle.Stop(Policy::Outcome::TimedOut); Request.Snapshot.Result = PCGFailure(TEXT("GenerationTimeout")); BeginCleanup(Id); }
    }
    const auto* Profile = Data ? Cast<UGamePlatformPCGProfileDefinition>(Data->GetLoadedDefinition(Request.Lease)) : nullptr;
    if (Request.Lifecycle.Phase == Policy::Phase::Loading)
    {
        if (!Profile)
        {
            if (!Data || Data->GetLeaseState(Request.Lease) != EGamePlatformDataRequestState::Loading)
            { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = PCGFailure(TEXT("ProfileLoadFailed")); BeginCleanup(Id); }
            return;
        }
        auto Valid = ValidateProfile(*Profile);
        if (Valid.IsSuccess()) { Valid = GamePlatformPCGInspection::ValidateApprovedGraph(*Profile); }
        if (Profile->RegionId != Request.Input.RegionId) { Valid = PCGFailure(TEXT("ProfileRegionMismatch")); }
        // 请求整个轴对齐盒必须位于同一公开World区域；输出还会做实际边界审计。
        for (int32 Corner = 0; Valid.IsSuccess() && Corner < 8; ++Corner)
        {
            const FVector Position = Request.Input.CenterCm + Profile->HalfExtentCm * FVector((Corner & 1) ? 1 : -1,(Corner & 2) ? 1 : -1,(Corner & 4) ? 1 : -1);
            FGamePlatformId Region;
            if (!WorldService->QueryRegion(Position,Region).IsSuccess() || Region != Request.Input.RegionId) { Valid = PCGFailure(TEXT("BoundsOutsideRegion")); }
        }
        if (!Valid.IsSuccess()) { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = Valid; BeginCleanup(Id); return; }
        Request.Snapshot.ProfileRevision = Profile->DataVersion.ContentRevision;
        Request.Snapshot.DeadlineSeconds = Request.Snapshot.StartedSeconds + Profile->TimeoutSeconds;
        if (Now >= Request.Snapshot.DeadlineSeconds)
        { Request.Lifecycle.Stop(Policy::Outcome::TimedOut); Request.Snapshot.Result = PCGFailure(TEXT("GenerationTimeout")); BeginCleanup(Id); return; }
        FActorSpawnParameters Parameters; Parameters.ObjectFlags = RF_Transient;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = World->SpawnActor<AActor>(AActor::StaticClass(),Request.Input.CenterCm,FRotator::ZeroRotator,Parameters);
        if (!Actor) { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = PCGFailure(TEXT("SourceSpawnFailed")); BeginCleanup(Id); return; }
        Request.Actor = Actor; Request.bSourceCreated = true; Actor->SetReplicates(false);
        auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Profile->HalfExtentCm); Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); Box->SetGenerateOverlapEvents(false);
        Box->SetCanEverAffectNavigation(false); Box->RegisterComponent(); Actor->SetActorLocation(Request.Input.CenterCm);
        auto* Component = NewObject<UPCGComponent>(Actor); Request.Component = Component; Actor->AddInstanceComponent(Component);
        Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
        Component->bGenerateOnDropWhenTriggerOnDemand = false;
#if WITH_EDITOR
        Component->bRegenerateInEditor = false;
#endif
        const auto Native = TWeakObjectPtr<UPCGComponent>(Component);
        Request.GeneratedDelegate = Component->OnPCGGraphGeneratedDelegate.AddWeakLambda(this,[this,Id,Native](UPCGComponent* Sender)
        { if (Sender == Native.Get()) { OnNativeSignal(Id,Native,true); } });
        Request.CancelledDelegate = Component->OnPCGGraphCancelledDelegate.AddWeakLambda(this,[this,Id,Native](UPCGComponent* Sender)
        { if (Sender == Native.Get()) { OnNativeSignal(Id,Native,false); } });
        const auto Facts = WorldService->GetReadiness();
        const uint32 Seed = Policy::StableSeed(TCHAR_TO_UTF8(*Facts.Context.WorldId.ToString()),TCHAR_TO_UTF8(*Profile->RegionId.ToString()),
            TCHAR_TO_UTF8(*Profile->LogicalId.ToString()),static_cast<uint32>(Profile->DataVersion.ContentRevision),static_cast<uint32>(Profile->Seed));
        Valid = GamePlatformPCGInspection::ConfigureOwnedComponent(*Component,*Profile,static_cast<int32>(Seed));
        if (!Valid.IsSuccess()) { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = Valid; BeginCleanup(Id); return; }
        Component->RegisterComponent(); Request.Lifecycle.Loaded(Request.Snapshot.Handle.ExecutionGeneration);
        Request.GenerateStarted = Now; Request.bDirty = true;
        const auto NativeTask = Component->GenerateLocalGetTaskId(false);
        if (NativeTask == InvalidPCGTaskId)
        { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = PCGFailure(TEXT("NativeGenerationNotScheduled")); BeginCleanup(Id); }
        return;
    }
    if (Request.Lifecycle.Phase == Policy::Phase::Generating)
    {
        if (!Profile || Profile->DataVersion.ContentRevision != Request.Snapshot.ProfileRevision || Request.bNativeCancelled || !Request.Component.IsValid())
        { Request.Lifecycle.Stop(Policy::Outcome::Failed); Request.Snapshot.Result = PCGFailure(TEXT("NativeGenerationInvalidated")); BeginCleanup(Id); }
        else if (Request.bNativeGenerated)
        {
            const auto Inspection = GamePlatformPCGInspection::InspectOwnedOutput(*Request.Component.Get(),*Profile,Request.Input.CenterCm);
            Request.Lifecycle.Generated(Request.Snapshot.Handle.ExecutionGeneration,Inspection.Result.IsSuccess());
            Request.Snapshot.Result = Inspection.Result; Request.Snapshot.ActualInstanceCount = Inspection.InstanceCount;
            Request.Snapshot.OutputFingerprint = Inspection.Fingerprint; Request.Snapshot.GenerationSeconds = Now - Request.GenerateStarted;
            Request.bDirty = true; if (!Inspection.Result.IsSuccess()) { BeginCleanup(Id); }
        }
    }
    if (Request.Lifecycle.Phase == Policy::Phase::Retained &&
        (!Profile || !Request.Component.IsValid() || Profile->DataVersion.ContentRevision != Request.Snapshot.ProfileRevision))
    { Request.Snapshot.Result = PCGFailure(TEXT("RetainedResultInvalidated")); BeginCleanup(Id); }
    if (Request.Lifecycle.Phase == Policy::Phase::Cleaning)
    {
        BeginCleanup(Id);
        bool Empty = !Request.bSourceCreated;
        if (auto* Component = Request.Component.Get())
        {
            Empty = !Component->IsGenerating() && !Component->IsCleaningUp() && Component->AreManagedResourcesAccessible() &&
                !Component->bGenerated && Component->GetGeneratedGraphOutput().TaggedData.IsEmpty();
            if (Empty) { Component->ForEachConstManagedResource([&](const UPCGManagedResource*) { Empty = false; }); }
            if (Empty)
            {
                Component->OnPCGGraphGeneratedDelegate.Remove(Request.GeneratedDelegate);
                Component->OnPCGGraphCancelledDelegate.Remove(Request.CancelledDelegate);
                Component->DestroyComponent(); Request.Component.Reset();
                Request.bSourceCreated = false;
            }
        }
        if (Empty && Data)
        {
            if (auto* Actor = Request.Actor.Get()) { Actor->Destroy(); Request.Actor.Reset(); }
            const auto Released = Data->ReleaseDefinition(Request.Lease);
            if (Released.IsSuccess())
            {
                Request.Lease = {}; Request.Lifecycle.Cleaned(Request.Snapshot.Handle.ExecutionGeneration,true,true);
                Request.Snapshot.CleaningSeconds = Now - Request.CleanupStarted; Request.bDirty = true;
            }
            else { Request.Snapshot.Result = Released; }
        }
    }
}
bool UGamePlatformPCGWorldSubsystem::Tick(float)
{
    check(IsInGameThread()); if (!Scope || Scope->bStopping) { return false; }
    TArray<FGuid> Ids; Scope->Requests.GenerateKeyArray(Ids);
    {
        TGuardValue<bool> Guard(Scope->bProcessing,true);
        for (const auto Id : Ids) { ProcessRequest(Id); }
    }
    TArray<FGuid> Subscribers; Scope->Subscribers.GenerateKeyArray(Subscribers);
    for (const auto Id : Ids)
    {
        if (!Scope) { return false; }
        auto Request = Scope->Requests.FindChecked(Id); if (!Request->bDirty) { continue; } Request->bDirty = false;
        const auto Snapshot = GetGenerationSnapshot(Request->Snapshot.Handle);
        for (const auto Subscription : Subscribers)
        {
            if (!Scope) { return false; }
            auto* Found = Scope->Subscribers.Find(Subscription); if (!Found) { continue; }
            auto Entry = *Found;
            if (!Entry->Owner.IsValid()) { Scope->Subscribers.Remove(Subscription); continue; }
            if (Entry->Handle == Snapshot.Handle) { Entry->Callback(Snapshot); }
        }
    }
    return true;
}
void UGamePlatformPCGWorldSubsystem::Deinitialize()
{
    check(IsInGameThread());
    if (TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); TickerHandle.Reset(); }
    if (Scope)
    {
        Scope->bStopping = true; Scope->Subscribers.Reset();
        auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
        for (auto& Pair : Scope->Requests)
        {
            auto& Request = *Pair.Value; Request.Lifecycle.Stop(Policy::Outcome::Cancelled);
            bool bOutputsReleased = !Request.bSourceCreated;
            if (auto* Component = Request.Component.Get())
            {
                Component->OnPCGGraphGeneratedDelegate.Remove(Request.GeneratedDelegate); Component->OnPCGGraphCancelledDelegate.Remove(Request.CancelledDelegate);
                Component->CancelGeneration();
                // 世界退出不再调度异步新任务；限定非分区且取消已返回后使用原生同步清理自己的临时输出。
                if (!Component->IsGenerating() && !Component->IsCleaningUp() && Component->AreManagedResourcesAccessible())
                {
                    Component->CleanupLocalImmediate(true);
                    bOutputsReleased = !Component->bGenerated && Component->GetGeneratedGraphOutput().TaggedData.IsEmpty();
                    Component->ForEachConstManagedResource([&](const UPCGManagedResource*) { bOutputsReleased = false; });
                    if (bOutputsReleased) { Component->DestroyComponent(); }
                }
            }
            if (bOutputsReleased)
            {
                if (auto* Actor = Request.Actor.Get()) { Actor->Destroy(); }
                if (Data && Request.Lease.IsValid()) { Data->ReleaseDefinition(Request.Lease); }
            }
            else
            {
                // 不提前释放仍被引擎使用的实例租约；异常路径由GI最终拆卸回收，不能报告Cleaned。
                UE_LOG(LogTemp, Error, TEXT("PCG teardown could not prove native drain for %s; instance lease retained until GI teardown"), *Request.Snapshot.Handle.OperationId.ToString());
            }
        }
        Scope.Reset();
    }
    Super::Deinitialize();
}
