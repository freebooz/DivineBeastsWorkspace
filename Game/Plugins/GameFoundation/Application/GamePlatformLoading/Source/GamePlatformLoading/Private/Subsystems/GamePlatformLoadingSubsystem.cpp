#include "Subsystems/GamePlatformLoadingSubsystem.h"
#include "Operations/LoadingPolicy.h"
#include "Tasks/LoadingBuiltinTasks.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "UObject/StrongObjectPtr.h"

namespace Policy = GamePlatformLoadingPolicy;
struct FLoadingFactoryRecord
{
    FGamePlatformLoadingRegistration Handle;
    FGamePlatformLoadingTaskFactory Factory;
};
struct FLoadingSubscription
{
    FGamePlatformLoadingRegistration Handle;
    FGamePlatformLoadingHandle Operation;
    TWeakObjectPtr<UObject> Owner;
    TFunction<void(const FGamePlatformLoadingSnapshot&)> Callback;
    bool bTerminalDelivered = false;
};
struct FLoadingTaskExecution
{
    Policy::ExecutionToken Token;
    TUniquePtr<IGamePlatformLoadingTask> Task;
};
struct FGamePlatformLoadingScope
{
    FGuid Id = FGuid::NewGuid();
    uint64 Generation = 0;
    Policy::Operation Operation;
    FGamePlatformLoadingHandle Handle;
    FGamePlatformLoadingOperationSpec Spec;
    TWeakObjectPtr<UObject> Owner;
    TMap<FName,TSharedPtr<FLoadingFactoryRecord>> Factories;
    TMap<FGuid,TSharedPtr<FLoadingSubscription>> Subscriptions;
    TMap<FName,TSharedPtr<FLoadingTaskExecution>> Executions;
    TArray<FGamePlatformLoadingSnapshot> PendingSnapshots;
    // 冻结规格不参与反射扫描；等待依赖/回退期间也必须保活定义类，不能只靠Data开始后持有。
    TArray<TStrongObjectPtr<UClass>> DefinitionClasses;
    TSharedPtr<FLoadingWorldEvidence> World;
    double StartSeconds = 0;
    bool bResourcesHeld = false;
    bool bDispatchingTask = false;
    bool bDirty = false;
    bool bHasWorldTask = false;
};

static FGamePlatformResult LoadingError(FName Code)
{ return FGamePlatformResult::Failure(Code,TEXT("加载请求被拒绝或失败；请按错误码与任务快照排查")); }
static bool BelongsToInstance(TWeakObjectPtr<UObject> Owner, UGameInstance* Instance)
{ return Owner.IsValid() && (Owner.Get() == Instance || Owner->GetTypedOuter<UGameInstance>() == Instance); }

UGamePlatformLoadingSubsystem::UGamePlatformLoadingSubsystem() = default;
UGamePlatformLoadingSubsystem::UGamePlatformLoadingSubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformLoadingSubsystem::~UGamePlatformLoadingSubsystem() = default;
IGamePlatformLoadingService* IGamePlatformLoadingService::Get(UGameInstance& Instance)
{ check(IsInGameThread()); return Instance.GetSubsystem<UGamePlatformLoadingSubsystem>(); }
void UGamePlatformLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Scope = MakeUnique<FGamePlatformLoadingScope>();
    // 不在模块启动或Commandlet中启动操作；实例定时采样只负责截止时间和真实世界状态。
    if (!IsRunningCommandlet())
    { TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Tick),0.05f); }
}
void UGamePlatformLoadingSubsystem::Deinitialize()
{
    check(IsInGameThread());
    if (TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); TickerHandle.Reset(); }
    if (Scope)
    {
        Scope->Subscriptions.Reset(); Scope->Operation.Cancel(); ReleaseTasks(); Scope.Reset();
    }
    Super::Deinitialize();
}
FGamePlatformLoadingHandle UGamePlatformLoadingSubsystem::StartLoadingOperation(
    const FGamePlatformLoadingOperationSpec& Spec,TWeakObjectPtr<UObject> Owner,FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Scope || IsRunningCommandlet()) { OutResult = LoadingError(TEXT("ServiceUnavailable")); return {}; }
    if (Scope->bResourcesHeld || Scope->bDispatchingTask) { OutResult = LoadingError(TEXT("Busy")); return {}; }
    if (!BelongsToInstance(Owner,GetGameInstance()) || Spec.Purpose.IsNone()) { OutResult = LoadingError(TEXT("InvalidOwnerOrPurpose")); return {}; }
    std::vector<Policy::TaskSpec> Specs;
    bool HasWorld = false;
    for (const auto& Task : Spec.Tasks)
    {
        if (Task.Requiredness != EGamePlatformLoadingRequirement::Required &&
            Task.Requiredness != EGamePlatformLoadingRequirement::Optional &&
            Task.Requiredness != EGamePlatformLoadingRequirement::Degradable)
        { OutResult = LoadingError(TEXT("InvalidRequirement")); return {}; }
        if ((Task.TaskType == TEXT("WorldPresence") && Task.Requiredness != EGamePlatformLoadingRequirement::Required) ||
            Task.FallbackTaskType == TEXT("WorldPresence"))
        { OutResult = LoadingError(TEXT("WorldMustBeRequired")); return {}; }
        for (const auto Type : {Task.TaskType,Task.FallbackTaskType})
        {
            if (Type.IsNone() && Type == Task.FallbackTaskType) { continue; }
            if (Type != TEXT("Data") && Type != TEXT("WorldPresence") && !Scope->Factories.Contains(Type))
            { OutResult = LoadingError(Type == TEXT("SessionReady") ? TEXT("SessionPrerequisiteMissing") : TEXT("TaskFactoryMissing")); return {}; }
            HasWorld |= Type == TEXT("WorldPresence");
        }
        if (Task.TaskType.IsNone()) { OutResult = LoadingError(TEXT("TaskTypeMissing")); return {}; }
        Policy::TaskSpec Value;
        Value.Id = TCHAR_TO_UTF8(*Task.TaskId.ToString().ToLower());
        if (Task.TaskId.IsNone()) { Value.Id.clear(); }
        Value.Requiredness = static_cast<Policy::Requirement>(Task.Requiredness);
        Value.Weight = Task.Weight; Value.TimeoutSeconds = Task.TimeoutSeconds;
        Value.HasFallback = !Task.FallbackTaskType.IsNone();
        for (const auto Dependency : Task.Dependencies) { Value.Dependencies.emplace_back(TCHAR_TO_UTF8(*Dependency.ToString().ToLower())); }
        Specs.push_back(MoveTemp(Value));
    }
    if (HasWorld && Spec.TargetWorldPackage.IsEmpty()) { OutResult = LoadingError(TEXT("TargetWorldMissing")); return {}; }
    const double Now = FPlatformTime::Seconds();
    const auto Error = Scope->Operation.Start(Specs,Scope->Generation + 1,Now,Spec.TimeoutSeconds);
    if (!Error.empty()) { OutResult = LoadingError(FName(UTF8_TO_TCHAR(Error.c_str()))); return {}; }
    Scope->Handle = {Scope->Id,FGuid::NewGuid(),++Scope->Generation};
    Scope->Spec = Spec; Scope->Owner = Owner; Scope->StartSeconds = Now;
    for (const auto& Task : Spec.Tasks)
    {
        if (Task.Data.ExpectedClass) { Scope->DefinitionClasses.Emplace(Task.Data.ExpectedClass.Get()); }
        if (Task.FallbackData.ExpectedClass) { Scope->DefinitionClasses.Emplace(Task.FallbackData.ExpectedClass.Get()); }
    }
    Scope->bResourcesHeld = true; Scope->bDirty = true; Scope->bHasWorldTask = HasWorld;
    Scope->World = MakeShared<FLoadingWorldEvidence>(); Scope->World->Instance = GetGameInstance(); Scope->World->Package = Spec.TargetWorldPackage;
    OutResult = FGamePlatformResult::Success(); return Scope->Handle;
}
FGamePlatformResult UGamePlatformLoadingSubsystem::CancelLoadingOperation(const FGamePlatformLoadingHandle& Handle)
{
    check(IsInGameThread());
    if (!Scope || !Handle.IsValid() || !(Scope->Handle == Handle)) { return LoadingError(TEXT("StaleOperation")); }
    if (Scope->bDispatchingTask) { return LoadingError(TEXT("ReentrantMutation")); }
    if (Scope->Operation.State == Policy::OperationState::Running)
    { Scope->Operation.Cancel(); ReleaseTasks(); Scope->PendingSnapshots.Add(GetLoadingSnapshot()); Scope->bDirty = true; }
    return FGamePlatformResult::Success();
}
FGamePlatformResult UGamePlatformLoadingSubsystem::ReleaseLoadingOperation(const FGamePlatformLoadingHandle& Handle)
{
    const auto Result = CancelLoadingOperation(Handle);
    if (Result.IsSuccess()) { ReleaseTasks(); Scope->bDirty = true; }
    return Result;
}
void UGamePlatformLoadingSubsystem::ReleaseTasks()
{
    TGuardValue<bool> Guard(Scope->bDispatchingTask,true);
    for (auto& Entry : Scope->Executions) { Entry.Value->Task->Release(); }
    Scope->Executions.Reset(); Scope->bResourcesHeld = false;
    Scope->DefinitionClasses.Reset();
    if (Scope->World) { Scope->World->OperableWorld.Reset(); }
}
FGamePlatformLoadingSnapshot UGamePlatformLoadingSubsystem::GetLoadingSnapshot() const
{
    check(IsInGameThread()); FGamePlatformLoadingSnapshot Snapshot;
    if (!Scope) { return Snapshot; }
    Snapshot.Handle = Scope->Handle; Snapshot.Purpose = Scope->Spec.Purpose; Snapshot.TargetWorldPackage = Scope->Spec.TargetWorldPackage;
    Snapshot.State = static_cast<EGamePlatformLoadingState>(Scope->Operation.State);
    Snapshot.StartTimeSeconds = Scope->StartSeconds; Snapshot.DeadlineSeconds = Scope->StartSeconds + Scope->Spec.TimeoutSeconds;
    Snapshot.OverallProgress01 = Scope->Operation.Progress(); Snapshot.bResourcesHeld = Scope->bResourcesHeld;
    if (Scope->Operation.Ready()) { Snapshot.Result = FGamePlatformResult::Success(); }
    else if (Scope->Operation.State == Policy::OperationState::Cancelled) { Snapshot.Result = FGamePlatformResult::Cancelled(TEXT("加载操作已取消")); }
    else if (Scope->Operation.State == Policy::OperationState::Failed) { Snapshot.Result = LoadingError(TEXT("TaskFailed")); }
    else if (Scope->Operation.State == Policy::OperationState::TimedOut) { Snapshot.Result = LoadingError(TEXT("OperationTimeout")); }
    for (const auto& Task : Scope->Operation.Tasks)
    {
        FGamePlatformLoadingTaskSnapshot Item;
        Item.TaskId = FName(UTF8_TO_TCHAR(Task.Spec.Id.c_str())); Item.State = static_cast<EGamePlatformLoadingTaskState>(Task.State);
        Item.Progress01 = Task.Progress; Item.ExecutionGeneration = Task.Generation; Item.bIsFallback = Task.Fallback;
        Item.Error = FName(UTF8_TO_TCHAR(Task.Error.c_str())); Snapshot.Tasks.Add(Item);
    }
    return Snapshot;
}
FGamePlatformLoadingRegistration UGamePlatformLoadingSubsystem::SubscribeLoadingState(const FGamePlatformLoadingHandle& Handle,
    TWeakObjectPtr<UObject> Owner,TFunction<void(const FGamePlatformLoadingSnapshot&)> Callback)
{
    check(IsInGameThread());
    if (!Scope || !(Scope->Handle == Handle) || !Handle.IsValid() || !Callback || !BelongsToInstance(Owner,GetGameInstance())) { return {}; }
    auto Entry = MakeShared<FLoadingSubscription>(); Entry->Handle = {Scope->Id,FGuid::NewGuid()};
    Entry->Owner = Owner; Entry->Callback = MoveTemp(Callback); Entry->Operation = Handle;
    Scope->Subscriptions.Add(Entry->Handle.RegistrationId,Entry); Scope->bDirty = true; return Entry->Handle;
}
bool UGamePlatformLoadingSubsystem::UnsubscribeLoadingState(const FGamePlatformLoadingRegistration& Registration)
{
    check(IsInGameThread());
    return Scope && Registration.OwnerScopeId == Scope->Id && Scope->Subscriptions.Remove(Registration.RegistrationId) != 0;
}
FGamePlatformLoadingRegistration UGamePlatformLoadingSubsystem::RegisterTaskFactory(FName Type,
    FGamePlatformLoadingTaskFactory Factory,FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Scope || Scope->bResourcesHeld || Scope->bDispatchingTask) { OutResult = LoadingError(TEXT("Busy")); return {}; }
    if (!Factory || Type.IsNone() || Type == TEXT("Data") || Type == TEXT("WorldPresence") || Scope->Factories.Contains(Type))
    { OutResult = LoadingError(TEXT("InvalidOrDuplicateFactory")); return {}; }
    auto Entry = MakeShared<FLoadingFactoryRecord>(); Entry->Handle = {Scope->Id,FGuid::NewGuid()}; Entry->Factory = MoveTemp(Factory);
    Scope->Factories.Add(Type,Entry); OutResult = FGamePlatformResult::Success(); return Entry->Handle;
}
FGamePlatformResult UGamePlatformLoadingSubsystem::UnregisterTaskFactory(const FGamePlatformLoadingRegistration& Registration)
{
    check(IsInGameThread());
    if (!Scope || Scope->bResourcesHeld || Scope->bDispatchingTask) { return LoadingError(TEXT("Busy")); }
    if (Registration.OwnerScopeId != Scope->Id) { return LoadingError(TEXT("ForeignRegistration")); }
    for (auto Iterator = Scope->Factories.CreateIterator(); Iterator; ++Iterator)
    { if (Iterator.Value()->Handle.RegistrationId == Registration.RegistrationId) { Iterator.RemoveCurrent(); return FGamePlatformResult::Success(); } }
    return LoadingError(TEXT("StaleRegistration"));
}
bool UGamePlatformLoadingSubsystem::IsReadyToPlay(const FGamePlatformLoadingHandle& Handle) const
{
    check(IsInGameThread());
    if (!(Scope && Handle.IsValid() && Scope->Handle == Handle && Scope->bResourcesHeld && Scope->Owner.IsValid() &&
        Scope->Operation.Ready() && (!Scope->bHasWorldTask || Scope->World->IsValid()))) { return false; }
    if (Scope->bDispatchingTask) { return false; }
    TGuardValue<bool> Guard(Scope->bDispatchingTask,true);
    for (const auto& Task : Scope->Spec.Tasks)
    {
        if (Task.Requiredness == EGamePlatformLoadingRequirement::Optional) { continue; }
        const auto* Execution = Scope->Executions.Find(Task.TaskId);
        if (!Execution || !(*Execution)->Task || !(*Execution)->Task->IsReadyToUse()) { return false; }
    }
    return true;
}
FGamePlatformResult UGamePlatformLoadingSubsystem::ReportWorldOperable(const FGamePlatformLoadingHandle& Handle,UWorld& World)
{
    check(IsInGameThread());
    if (!Scope || !(Scope->Handle == Handle) || !Scope->bResourcesHeld || Scope->bDispatchingTask ||
        World.GetGameInstance() != GetGameInstance() || GetGameInstance()->GetWorld() != &World)
    { return LoadingError(TEXT("WorldScopeMismatch")); }
    Scope->World->OperableWorld = &World;
    if (!Scope->World->IsValid()) { Scope->World->OperableWorld.Reset(); return LoadingError(TEXT("WorldNotOperable")); }
    Scope->bDirty = true; return FGamePlatformResult::Success();
}
bool UGamePlatformLoadingSubsystem::Tick(float)
{
    check(IsInGameThread()); if (!Scope) { return false; }
    if (Scope->bResourcesHeld && !Scope->Owner.IsValid())
    { Scope->Operation.Cancel(); ReleaseTasks(); Scope->bDirty = true; }
    if (Scope->Operation.State == Policy::OperationState::Running)
    {
        TGuardValue<bool> Guard(Scope->bDispatchingTask,true);
        const auto BeforeState = Scope->Operation.State; const double BeforeProgress = Scope->Operation.Progress();
        Scope->Operation.Expire(FPlatformTime::Seconds());
        for (const auto& Token : Scope->Operation.Startable(FPlatformTime::Seconds()))
        {
            if (Scope->Operation.State != Policy::OperationState::Running) { break; }
            const FName Id(UTF8_TO_TCHAR(Token.Id.c_str()));
            if (auto* Previous = Scope->Executions.Find(Id)) { (*Previous)->Task->Release(); Scope->Executions.Remove(Id); }
            auto Spec = *Scope->Spec.Tasks.FindByPredicate([&](const auto& Candidate) { return Candidate.TaskId == Id; });
            if (Token.Generation > 1) { Spec.TaskType = Spec.FallbackTaskType; Spec.Data = Spec.FallbackData; }
            auto Execution = MakeShared<FLoadingTaskExecution>(); Execution->Token = Token;
            if (Spec.TaskType == TEXT("Data")) { Execution->Task = MakeUnique<FLoadingDataTask>(); }
            else if (Spec.TaskType == TEXT("WorldPresence")) { Execution->Task = MakeUnique<FLoadingWorldTask>(Scope->World.ToSharedRef()); }
            else { Execution->Task = Scope->Factories.FindChecked(Spec.TaskType)->Factory(); }
            if (!Execution->Task) { Scope->Operation.Complete(Token,false,"FactoryReturnedNull"); continue; }
            Scope->Executions.Add(Id,Execution);
            const auto Result = Execution->Task->Start(*GetGameInstance(),Spec);
            if (!Result.IsSuccess())
            {
                Scope->Operation.Complete(Token,false,TCHAR_TO_UTF8(*Result.Code.ToString()));
                Execution->Task->Release(); Scope->Executions.Remove(Id);
            }
            Scope->bDirty = true;
        }
        for (auto Iterator = Scope->Executions.CreateIterator(); Iterator; ++Iterator)
        {
            auto& Execution = *Iterator.Value();
            const auto* Record = Scope->Operation.Tasks.data();
            for (const auto& Candidate : Scope->Operation.Tasks) { if (Candidate.Spec.Id == Execution.Token.Id) { Record = &Candidate; break; } }
            if (Record->State == Policy::TaskState::Succeeded || Record->State == Policy::TaskState::Degraded) { continue; }
            if (Scope->Operation.State != Policy::OperationState::Running || Record->State != Policy::TaskState::Running || Record->Generation != Execution.Token.Generation)
            { Execution.Task->Release(); Iterator.RemoveCurrent(); continue; }
            const auto Update = Execution.Task->Poll();
            Scope->Operation.Report(Execution.Token,Update.Progress01);
            if (Update.State != EGamePlatformLoadingTaskUpdate::Pending)
            {
                Scope->Operation.Complete(Execution.Token,Update.State == EGamePlatformLoadingTaskUpdate::Succeeded,TCHAR_TO_UTF8(*Update.Error.ToString()));
                Scope->bDirty = true;
                if (Update.State == EGamePlatformLoadingTaskUpdate::Failed) { Execution.Task->Release(); Iterator.RemoveCurrent(); }
            }
        }
        Scope->bDirty |= BeforeState != Scope->Operation.State || BeforeProgress != Scope->Operation.Progress();
        if (Scope->Operation.State != Policy::OperationState::Running && !Scope->Operation.Ready()) { ReleaseTasks(); }
    }
    if (Scope->bDirty)
    {
        Scope->bDirty = false;
        auto Snapshots = MoveTemp(Scope->PendingSnapshots); Scope->PendingSnapshots.Reset();
        Snapshots.Add(GetLoadingSnapshot());
        TArray<TSharedPtr<FLoadingSubscription>> Subscribers; Scope->Subscriptions.GenerateValueArray(Subscribers);
        for (const auto& Snapshot : Snapshots)
        {
        for (const auto& Entry : Subscribers)
        {
            if (!Scope) { return false; }
            if (!Entry->Owner.IsValid()) { Scope->Subscriptions.Remove(Entry->Handle.RegistrationId); continue; }
            if (Entry->Operation == Snapshot.Handle && !Entry->bTerminalDelivered && Scope->Subscriptions.Contains(Entry->Handle.RegistrationId))
            {
                Entry->bTerminalDelivered = Snapshot.State != EGamePlatformLoadingState::Running && Snapshot.State != EGamePlatformLoadingState::Idle;
                Entry->Callback(Snapshot);
            }
        }
        }
    }
    return true;
}
