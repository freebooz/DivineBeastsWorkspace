#include "Subsystems/GamePlatformQuestServerSubsystem.h"

// 服务拥有当前世界的聚合/重试计时器；调用GetTimerManager必须直接包含UWorld完整定义。
#include "Engine/World.h"
#include "Components/GamePlatformQuestStateComponent.h"
#include "Definitions/GamePlatformQuestDefinition.h"
#include "Interfaces/GamePlatformQuestPersistencePort.h"
#include "Settings/GamePlatformQuestSettings.h"
#include "Types/GamePlatformQuestStateMachine.h"
#include "Types/GamePlatformQuestSnapshotRules.h"

namespace
{
bool IsActiveState(EGamePlatformQuestState State)
{
    return State == EGamePlatformQuestState::Accepted ||
           State == EGamePlatformQuestState::Active ||
           State == EGamePlatformQuestState::ObjectivesCompleted ||
           State == EGamePlatformQuestState::CompletionPending;
}

bool IsPersistenceConflict(EGamePlatformQuestError Error)
{
    return Error == EGamePlatformQuestError::DuplicateEvent ||
           Error == EGamePlatformQuestError::RevisionConflict ||
           Error == EGamePlatformQuestError::CompletionAlreadyCommitted;
}
}

void UGamePlatformQuestServerSubsystem::Deinitialize()
{
    // World销毁不是持久化提交点。正常迁服/登出必须先FlushPlayerProgressNow + UnregisterPlayer。
    // 这里只取消世界Timer并释放运行时状态；所有外部异步回调均通过TWeakObjectPtr回到本Subsystem。
    if (UWorld* World = GetWorld())
    {
        for (TPair<FString, FPlayerRuntime>& Pair : Players)
        {
            World->GetTimerManager().ClearTimer(Pair.Value.ProgressFlushTimer);
        }
    }

    Players.Reset();
    Definitions.Reset();
    Super::Deinitialize();
}

bool UGamePlatformQuestServerSubsystem::RegisterDefinition(
    UGamePlatformQuestDefinition* Definition)
{
    if (!IsValid(Definition) ||
        Definition->QuestId.IsNone() ||
        Definitions.Contains(Definition->QuestId))
    {
        return false;
    }

    FText Reason;
    if (!Definition->ValidateDefinition(Reason))
    {
        return false;
    }

    Definitions.Add(Definition->QuestId, Definition);

    if (!ValidateDefinitionGraph(Reason))
    {
        Definitions.Remove(Definition->QuestId);
        return false;
    }

    return true;
}

bool UGamePlatformQuestServerSubsystem::ValidateDefinitionGraph(
    FText& OutReason) const
{
    TSet<FName> Visited;

    for (const TPair<FName, TWeakObjectPtr<UGamePlatformQuestDefinition>>& Pair :
         Definitions)
    {
        TSet<FName> Visiting;
        if (WouldCreatePrerequisiteCycle(
                Pair.Key,
                Visiting,
                Visited))
        {
            OutReason =
                FText::FromString(TEXT("Quest prerequisite存在循环依赖"));
            return false;
        }
    }

    OutReason = FText::GetEmpty();
    return true;
}

bool UGamePlatformQuestServerSubsystem::RegisterPlayer(
    const FString& PlayerId,
    const FString& CharacterId,
    const FGuid& PlayerRuntimeId,
    UGamePlatformQuestStateComponent* StateComponent,
    TSharedPtr<IGamePlatformQuestPersistencePort, ESPMode::ThreadSafe> Persistence,
    EGamePlatformQuestError& OutError)
{
    OutError = EGamePlatformQuestError::None;

    if (PlayerId.IsEmpty() ||
        CharacterId.IsEmpty() ||
        !PlayerRuntimeId.IsValid() ||
        !IsValid(StateComponent) ||
        !Persistence.IsValid())
    {
        OutError = EGamePlatformQuestError::PersistenceUnavailable;
        return false;
    }

    if (Players.Contains(PlayerId))
    {
        OutError = EGamePlatformQuestError::AlreadyAccepted;
        return false;
    }

    FPlayerRuntime Runtime;
    Runtime.CharacterId = CharacterId;
    Runtime.PlayerRuntimeId = PlayerRuntimeId;
    Runtime.StateComponent = StateComponent;
    Runtime.Persistence = MoveTemp(Persistence);
    Runtime.bPersistenceInFlight = true;

    Players.Add(PlayerId, MoveTemp(Runtime));

    FPlayerRuntime& Stored = Players.FindChecked(PlayerId);
    const FGuid ExpectedRuntimeId = Stored.PlayerRuntimeId;
    TWeakObjectPtr<UGamePlatformQuestServerSubsystem> WeakThis(this);

    const bool bStarted =
        Stored.Persistence->BeginLoadPlayerQuestSnapshot(
            PlayerId,
            [WeakThis, PlayerId, ExpectedRuntimeId](
                TArray<FGamePlatformQuestSnapshot> Loaded,
                EGamePlatformQuestError Error)
            {
                if (UGamePlatformQuestServerSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleInitialLoadCompleted(
                        PlayerId,
                        ExpectedRuntimeId,
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted)
    {
        Players.Remove(PlayerId);
        OutError = EGamePlatformQuestError::PersistenceUnavailable;
        return false;
    }

    return true;
}

void UGamePlatformQuestServerSubsystem::HandleInitialLoadCompleted(
    FString PlayerId,
    FGuid ExpectedRuntimeId,
    TArray<FGamePlatformQuestSnapshot> Loaded,
    EGamePlatformQuestError Error)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime || Runtime->PlayerRuntimeId != ExpectedRuntimeId)
    {
        return;
    }

    Runtime->bPersistenceInFlight = false;

    if (Error != EGamePlatformQuestError::None)
    {
        // 加载期间可能已接纳延迟事件；保留玩家与载荷，安排重试，不把后端失败变成事件丢失。
        Runtime->LastPersistenceError = Error; Runtime->bReconcileRequired = true;
        ScheduleProgressFlush(PlayerId, *Runtime);
        return;
    }

    EGamePlatformQuestError ApplyError =
        EGamePlatformQuestError::None;

    if (!ApplyLoadedSnapshots(
            *Runtime,
            Loaded,
            ApplyError))
    {
        Runtime->LastPersistenceError = ApplyError; Runtime->bReconcileRequired = true;
        ScheduleProgressFlush(PlayerId, *Runtime);
        return;
    }

    Runtime->bReady = true;
    BuildEventIndex(*Runtime);
    PublishSnapshot(*Runtime);
    ProcessDeferredEvents(PlayerId, *Runtime);

    if (FPlayerRuntime* Current = Players.Find(PlayerId))
    {
        FinishPersistenceOperation(PlayerId, *Current);
    }
}

bool UGamePlatformQuestServerSubsystem::ApplyLoadedSnapshots(
    FPlayerRuntime& Runtime,
    const TArray<FGamePlatformQuestSnapshot>& Loaded,
    EGamePlatformQuestError& OutError)
{
    OutError = EGamePlatformQuestError::None;

    // 先完整校验到临时值；任何定义/版本/重复键失败都不改变已接纳状态和事件所有权。
    TMap<FName, FGamePlatformQuestSnapshot> ValidatedQuests;

    for (const FGamePlatformQuestSnapshot& Snapshot : Loaded)
    {
        if (Snapshot.QuestId.IsNone())
        {
            OutError = EGamePlatformQuestError::InvalidQuestEvent;
            return false;
        }

        const UGamePlatformQuestDefinition* Definition =
            Definitions.FindRef(Snapshot.QuestId).Get();

        if (Snapshot.State != EGamePlatformQuestState::Completed)
        {
            if (!IsValid(Definition))
            {
                OutError = EGamePlatformQuestError::DefinitionMissing;
                return false;
            }

            if (Snapshot.DefinitionVersion != Definition->Version)
            {
                OutError =
                    EGamePlatformQuestError::DefinitionVersionMismatch;
                return false;
            }
        }

        // 已完成历史事实不因当前Definition版本变化而倒退。
        if (ValidatedQuests.Contains(Snapshot.QuestId))
        { OutError = EGamePlatformQuestError::RevisionConflict; return false; }
        ValidatedQuests.Add(Snapshot.QuestId, Snapshot);
    }

    Runtime.Quests = MoveTemp(ValidatedQuests);
    return true;
}

void UGamePlatformQuestServerSubsystem::UnregisterPlayer(
    const FString& PlayerId)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime)
    {
        return;
    }

    Runtime->bUnregisterWhenIdle = true;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(
            Runtime->ProgressFlushTimer);
    }

    if (!Runtime->bPersistenceInFlight &&
        Runtime->bReady)
    {
        ProcessDeferredEvents(PlayerId, *Runtime);
    }

    Runtime = Players.Find(PlayerId);
    if (!Runtime)
    {
        return;
    }

    if (!Runtime->bPersistenceInFlight &&
        (Runtime->bReconcileRequired ||
         !Runtime->PendingEventIds.IsEmpty()))
    {
        FlushPlayerProgress(PlayerId);
    }

    Runtime = Players.Find(PlayerId);
    if (Runtime &&
        !Runtime->bPersistenceInFlight &&
        Runtime->PendingEventIds.IsEmpty() &&
        Runtime->DeferredEvents.IsEmpty() && Runtime->PendingReplayEvents.IsEmpty())
    {
        Players.Remove(PlayerId);
    }
}

EGamePlatformQuestError UGamePlatformQuestServerSubsystem::AcceptQuest(
    const FString& PlayerId,
    FName QuestId)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime ||
        !Runtime->Persistence.IsValid() ||
        !Runtime->bReady)
    {
        return EGamePlatformQuestError::PersistenceUnavailable;
    }

    if (Runtime->bPersistenceInFlight || Runtime->bReconcileRequired ||
        Runtime->bUnregisterWhenIdle)
    {
        return EGamePlatformQuestError::PersistenceOutcomeUnknown;
    }

    const TWeakObjectPtr<UGamePlatformQuestDefinition>* DefinitionPtr =
        Definitions.Find(QuestId);

    UGamePlatformQuestDefinition* Definition =
        DefinitionPtr ? DefinitionPtr->Get() : nullptr;

    if (!IsValid(Definition))
    {
        return EGamePlatformQuestError::DefinitionMissing;
    }

    if (Definition->RepeatPolicy ==
            EGamePlatformQuestRepeatPolicy::OneShot)
    {
        if (const FGamePlatformQuestSnapshot* Existing =
            Runtime->Quests.Find(QuestId))
        {
            if (Existing->State == EGamePlatformQuestState::Completed)
            {
                return EGamePlatformQuestError::AlreadyCompleted;
            }
        }
    }

    if (const FGamePlatformQuestSnapshot* Existing =
        Runtime->Quests.Find(QuestId))
    {
        if (IsActiveState(Existing->State))
        {
            return EGamePlatformQuestError::AlreadyAccepted;
        }
    }

    if (!ArePrerequisitesMet(*Runtime, *Definition))
    {
        return EGamePlatformQuestError::PrerequisiteNotMet;
    }

    int32 ActiveCount = 0;
    for (const TPair<FName, FGamePlatformQuestSnapshot>& Pair :
         Runtime->Quests)
    {
        ActiveCount += IsActiveState(Pair.Value.State) ? 1 : 0;
    }

    if (ActiveCount >=
        FMath::Max(
            1,
            GetDefault<UGamePlatformQuestSettings>()
                ->MaxActiveQuests))
    {
        return EGamePlatformQuestError::QuestNotAvailable;
    }

    FGamePlatformQuestSnapshot Proposed;
    Proposed.QuestId = QuestId;
    Proposed.QuestInstanceId = FGuid::NewGuid();
    Proposed.DefinitionVersion = Definition->Version;
    Proposed.State = EGamePlatformQuestState::Available;

    if (!FGamePlatformQuestStateMachine::TryTransition(
            Proposed.State,
            EGamePlatformQuestState::Active))
    {
        return EGamePlatformQuestError::QuestNotAvailable;
    }

    Proposed.Revision = 0;
    Proposed.AcceptedAtUtc = FDateTime::UtcNow();
    Proposed.RewardStatus =
        Definition->RewardSetId.IsNone()
            ? EGamePlatformQuestRewardStatus::NoReward
            : EGamePlatformQuestRewardStatus::PendingExternalReward;
    Proposed.RewardSetId = Definition->RewardSetId;

    for (const FGamePlatformQuestObjectiveDefinition& Objective :
         Definition->Objectives)
    {
        FGamePlatformQuestObjectiveProgress Progress;
        Progress.ObjectiveId = Objective.ObjectiveId;
        Progress.RequiredValue = Objective.RequiredValue;
        Proposed.Objectives.Add(Progress);
    }

    Runtime->bPersistenceInFlight = true;
    const FGuid ExpectedRuntimeId = Runtime->PlayerRuntimeId;
    TWeakObjectPtr<UGamePlatformQuestServerSubsystem> WeakThis(this);

    const bool bStarted =
        Runtime->Persistence->BeginAcceptQuest(
            PlayerId,
            Proposed,
            Definition->RepeatPolicy,
            [WeakThis, PlayerId, ExpectedRuntimeId, QuestId](
                FGamePlatformQuestSnapshot Persisted,
                EGamePlatformQuestError Error)
            {
                if (UGamePlatformQuestServerSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleAcceptCompleted(
                        PlayerId,
                        ExpectedRuntimeId,
                        QuestId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted)
    {
        Runtime->bPersistenceInFlight = false;
        return EGamePlatformQuestError::PersistenceUnavailable;
    }

    return EGamePlatformQuestError::None;
}

void UGamePlatformQuestServerSubsystem::HandleAcceptCompleted(
    FString PlayerId,
    FGuid ExpectedRuntimeId,
    FName QuestId,
    FGamePlatformQuestSnapshot Persisted,
    EGamePlatformQuestError Error)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime || Runtime->PlayerRuntimeId != ExpectedRuntimeId)
    {
        return;
    }

    Runtime->bPersistenceInFlight = false;

    if (Error == EGamePlatformQuestError::None)
    {
        Runtime->Quests.Add(QuestId, MoveTemp(Persisted));
        BuildEventIndex(*Runtime);
        PublishSnapshot(*Runtime);
    }
    else if (IsPersistenceConflict(Error))
    {
        Runtime->bReconcileRequired = true;
        BeginReconcilePlayer(PlayerId, *Runtime);
        return;
    }

    FinishPersistenceOperation(PlayerId, *Runtime);
}

EGamePlatformQuestError UGamePlatformQuestServerSubsystem::AbandonQuest(
    const FString& PlayerId,
    FName QuestId)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime ||
        !Runtime->Persistence.IsValid() ||
        !Runtime->bReady)
    {
        return EGamePlatformQuestError::PersistenceUnavailable;
    }

    if (Runtime->bPersistenceInFlight || Runtime->bReconcileRequired ||
        Runtime->bUnregisterWhenIdle)
    {
        return EGamePlatformQuestError::PersistenceOutcomeUnknown;
    }

    FGamePlatformQuestSnapshot* Snapshot =
        FindActiveQuest(*Runtime, QuestId);

    if (!Snapshot)
    {
        return EGamePlatformQuestError::QuestNotFound;
    }

    UGamePlatformQuestDefinition* Definition =
        Definitions.FindRef(QuestId).Get();

    if (!IsValid(Definition) || !Definition->bCanAbandon)
    {
        return EGamePlatformQuestError::QuestNotAvailable;
    }

    FGamePlatformQuestSnapshot Proposed = *Snapshot;
    if (!FGamePlatformQuestStateMachine::TryTransition(
            Proposed.State,
            EGamePlatformQuestState::Abandoned))
    {
        return EGamePlatformQuestError::QuestNotAvailable;
    }

    Runtime->bPersistenceInFlight = true;
    const int64 ExpectedRevision = Snapshot->Revision;
    const FGuid ExpectedRuntimeId = Runtime->PlayerRuntimeId;
    TWeakObjectPtr<UGamePlatformQuestServerSubsystem> WeakThis(this);

    const bool bStarted =
        Runtime->Persistence->BeginAbandonQuest(
            PlayerId,
            Proposed,
            ExpectedRevision,
            [WeakThis, PlayerId, ExpectedRuntimeId, QuestId](
                FGamePlatformQuestSnapshot Persisted,
                EGamePlatformQuestError Error)
            {
                if (UGamePlatformQuestServerSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleAbandonCompleted(
                        PlayerId,
                        ExpectedRuntimeId,
                        QuestId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted)
    {
        Runtime->bPersistenceInFlight = false;
        return EGamePlatformQuestError::PersistenceUnavailable;
    }

    return EGamePlatformQuestError::None;
}

void UGamePlatformQuestServerSubsystem::HandleAbandonCompleted(
    FString PlayerId,
    FGuid ExpectedRuntimeId,
    FName QuestId,
    FGamePlatformQuestSnapshot Persisted,
    EGamePlatformQuestError Error)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime || Runtime->PlayerRuntimeId != ExpectedRuntimeId)
    {
        return;
    }

    Runtime->bPersistenceInFlight = false;

    if (Error == EGamePlatformQuestError::None)
    {
        if (FGamePlatformQuestSnapshot* Snapshot =
            Runtime->Quests.Find(QuestId))
        {
            *Snapshot = MoveTemp(Persisted);
        }

        Runtime->PendingEventIds.Remove(QuestId);
        BuildEventIndex(*Runtime);
        PublishSnapshot(*Runtime);
    }
    else if (IsPersistenceConflict(Error))
    {
        Runtime->bReconcileRequired = true;
        BeginReconcilePlayer(PlayerId, *Runtime);
        return;
    }

    FinishPersistenceOperation(PlayerId, *Runtime);
}

EGamePlatformQuestError UGamePlatformQuestServerSubsystem::HandleQuestEvent(
    const FGamePlatformQuestEvent& Event)
{
    if (!Event.EventId.IsValid() ||
        Event.EventType.IsNone() ||
        Event.PlayerId.IsEmpty())
    {
        return EGamePlatformQuestError::InvalidQuestEvent;
    }

    FPlayerRuntime* Runtime = Players.Find(Event.PlayerId);
    if (!Runtime || !Runtime->Persistence.IsValid())
    {
        return EGamePlatformQuestError::PersistenceUnavailable;
    }

    if (Runtime->PlayerRuntimeId != Event.PlayerRuntimeId ||
        Runtime->bUnregisterWhenIdle)
    {
        return EGamePlatformQuestError::Unauthorized;
    }

    if (!Runtime->bReady || Runtime->bReconcileRequired ||
        Runtime->bPersistenceInFlight)
    {
        return EnqueueDeferredEvent(*Runtime, Event)
            ? EGamePlatformQuestError::None
            : EGamePlatformQuestError::TimedOut;
    }

    return ProcessQuestEventNow(Event, *Runtime);
}

EGamePlatformQuestError
UGamePlatformQuestServerSubsystem::ProcessQuestEventNow(
    const FGamePlatformQuestEvent& Event,
    FPlayerRuntime& Runtime)
{
    const TArray<FObjectiveBinding>* Bindings =
        Runtime.EventIndex.Find(Event.EventType);

    if (!Bindings)
    {
        return EGamePlatformQuestError::None;
    }

    TMap<FName, TArray<int32>> ObjectivesByQuest;
    for (const FObjectiveBinding& Binding : *Bindings)
    {
        ObjectivesByQuest.FindOrAdd(
            Binding.QuestId).AddUnique(Binding.ObjectiveIndex);
    }

    bool bNeedsImmediateFlush = false;
    bool bAnyProgressChanged = false;

    for (const TPair<FName, TArray<int32>>& QuestBindings :
         ObjectivesByQuest)
    {
        const FName QuestId = QuestBindings.Key;

        if (HasSeenEvent(Runtime, QuestId, Event.EventId))
        {
            continue;
        }

        FGamePlatformQuestSnapshot* Snapshot =
            FindActiveQuest(Runtime, QuestId);

        UGamePlatformQuestDefinition* Definition =
            Definitions.FindRef(QuestId).Get();

        if (!Snapshot || !IsValid(Definition))
        {
            continue;
        }

        FGamePlatformQuestSnapshot Proposed = *Snapshot;
        bool bChanged = false;

        for (const int32 ObjectiveIndex : QuestBindings.Value)
        {
            if (!Definition->Objectives.IsValidIndex(ObjectiveIndex) ||
                !Proposed.Objectives.IsValidIndex(ObjectiveIndex))
            {
                continue;
            }

            const FGamePlatformQuestObjectiveDefinition& Objective =
                Definition->Objectives[ObjectiveIndex];

            if (!MatchesObjective(Event, Objective))
            {
                continue;
            }

            FGamePlatformQuestObjectiveProgress& Progress =
                Proposed.Objectives[ObjectiveIndex];

            if (Progress.bCompleted)
            {
                continue;
            }

            const double Delta =
                Objective.AggregationPolicy ==
                        EGamePlatformQuestAggregationPolicy::SetComplete
                    ? Objective.RequiredValue
                    : Objective.AggregationPolicy ==
                              EGamePlatformQuestAggregationPolicy::Sum
                          ? FMath::Max(0.0, Event.NumericValue)
                          : 1.0;

            if (Delta <= 0.0)
            {
                continue;
            }

            Progress.CurrentValue =
                FMath::Min(
                    Progress.RequiredValue,
                    Progress.CurrentValue + Delta);

            Progress.bCompleted =
                Progress.CurrentValue >= Progress.RequiredValue;

            bChanged = true;
        }

        if (!bChanged)
        {
            continue;
        }

        if (AllRequiredObjectivesComplete(Proposed, *Definition))
        {
            if (!FGamePlatformQuestStateMachine::TryTransition(
                    Proposed.State,
                    EGamePlatformQuestState::ObjectivesCompleted) ||
                !FGamePlatformQuestStateMachine::TryTransition(
                    Proposed.State,
                    EGamePlatformQuestState::CompletionPending))
            {
                continue;
            }

            Proposed.CompletionId =
                Proposed.CompletionId.IsValid()
                    ? Proposed.CompletionId
                    : FGuid::NewGuid();

            if (!Proposed.RewardSetId.IsNone() &&
                !Proposed.RewardClaimId.IsValid())
            {
                Proposed.RewardClaimId = FGuid::NewGuid();
            }

            bNeedsImmediateFlush = true;
        }

        *Snapshot = Proposed;

        TArray<FGuid>& Pending =
            Runtime.PendingEventIds.FindOrAdd(QuestId);

        Pending.AddUnique(Event.EventId);
        Runtime.PendingEventPayloads.Add(Event.EventId, Event);
        RememberEvent(Runtime, QuestId, Event.EventId);
        bAnyProgressChanged = true;
    }

    if (!bAnyProgressChanged)
    {
        return EGamePlatformQuestError::None;
    }

    BuildEventIndex(Runtime);
    PublishSnapshot(Runtime);

    const FString PlayerId = Event.PlayerId;
    if (bNeedsImmediateFlush)
    {
        FlushPlayerProgress(PlayerId);
    }
    else
    {
        ScheduleProgressFlush(PlayerId, Runtime);
    }

    return EGamePlatformQuestError::None;
}

bool UGamePlatformQuestServerSubsystem::EnqueueDeferredEvent(
    FPlayerRuntime& Runtime,
    const FGamePlatformQuestEvent& Event)
{
    const int32 Limit =
        FMath::Max(
            16,
            GetDefault<UGamePlatformQuestSettings>()
                ->MaxDeferredEventsPerPlayer);

    if (Runtime.DeferredEvents.ContainsByPredicate([&Event](const auto& Existing) { return Existing.EventId == Event.EventId; }))
    { return true; }
    if (Runtime.DeferredEvents.Num() >= Limit) { Runtime.LastPersistenceError = EGamePlatformQuestError::PersistenceOutcomeUnknown; return false; }

    Runtime.DeferredEvents.Add(Event);
    return true;
}

void UGamePlatformQuestServerSubsystem::ProcessDeferredEvents(
    const FString& PlayerId,
    FPlayerRuntime& Runtime)
{
    while (Runtime.bReady &&
           !Runtime.bPersistenceInFlight &&
           !Runtime.bReconcileRequired &&
           (!Runtime.PendingReplayEvents.IsEmpty() || !Runtime.DeferredEvents.IsEmpty()))
    {
        // 已接纳的冲突事件先重放；队列满只延后新事件，不能永久阻塞或丢掉旧权威事实。
        auto& Queue = Runtime.PendingReplayEvents.IsEmpty() ? Runtime.DeferredEvents : Runtime.PendingReplayEvents;
        FGamePlatformQuestEvent Event = Queue[0];
        Queue.RemoveAt(
            0,
            1,
            EAllowShrinking::No);

        ProcessQuestEventNow(Event, Runtime);

        if (Runtime.bPersistenceInFlight)
        {
            return;
        }
    }

    if (!Runtime.bPersistenceInFlight &&
        !Runtime.PendingEventIds.IsEmpty())
    {
        ScheduleProgressFlush(PlayerId, Runtime);
    }
}

void UGamePlatformQuestServerSubsystem::BeginReconcilePlayer(
    const FString& PlayerId,
    FPlayerRuntime& Runtime)
{
    if (!Runtime.Persistence.IsValid() ||
        Runtime.bPersistenceInFlight)
    {
        return;
    }

    Runtime.bReconcileRequired = true;

    Runtime.bPersistenceInFlight = true;
    const FGuid ExpectedRuntimeId = Runtime.PlayerRuntimeId;
    TWeakObjectPtr<UGamePlatformQuestServerSubsystem> WeakThis(this);

    const bool bStarted =
        Runtime.Persistence->BeginLoadPlayerQuestSnapshot(
            PlayerId,
            [WeakThis, PlayerId, ExpectedRuntimeId](
                TArray<FGamePlatformQuestSnapshot> Loaded,
                EGamePlatformQuestError Error)
            {
                if (UGamePlatformQuestServerSubsystem* Self = WeakThis.Get())
                {
                    Self->HandleReconcileCompleted(
                        PlayerId,
                        ExpectedRuntimeId,
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted)
    {
        Runtime.bPersistenceInFlight = false;
        ScheduleProgressFlush(PlayerId, Runtime);
    }
}

void UGamePlatformQuestServerSubsystem::HandleReconcileCompleted(
    FString PlayerId,
    FGuid ExpectedRuntimeId,
    TArray<FGamePlatformQuestSnapshot> Loaded,
    EGamePlatformQuestError Error)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime || Runtime->PlayerRuntimeId != ExpectedRuntimeId)
    {
        return;
    }

    Runtime->bPersistenceInFlight = false;

    Runtime->LastPersistenceError = Error;
    if (Error == EGamePlatformQuestError::None)
    {
        // Apply只在完整校验后发布新Quests；失败时PendingEventIds/载荷/去重账本全部原样保留。
        EGamePlatformQuestError ApplyError = EGamePlatformQuestError::None;
        TArray<FGamePlatformQuestEvent> EventsToReplay;
        Runtime->PendingEventPayloads.GenerateValueArray(EventsToReplay);
        if (ApplyLoadedSnapshots(*Runtime, Loaded, ApplyError))
        {
            EventsToReplay.Sort([](const auto& A, const auto& B) { return A.OccurredAtUtc < B.OccurredAtUtc; });
            const int32 Limit = FMath::Max(16, GetDefault<UGamePlatformQuestSettings>()->MaxDeferredEventsPerPlayer);
            TArray<FGamePlatformQuestEvent> UniqueReplay;
            for (const auto& Event : EventsToReplay)
            {
                if (!Runtime->DeferredEvents.ContainsByPredicate([&Event](const auto& Item) { return Item.EventId == Event.EventId; }) &&
                    !Runtime->PendingReplayEvents.ContainsByPredicate([&Event](const auto& Item) { return Item.EventId == Event.EventId; }))
                { UniqueReplay.Add(Event); }
            }
            if (GamePlatformQuestSnapshotPolicy::CanTransferReplay(Runtime->DeferredEvents.Num(), UniqueReplay.Num(), Limit))
            { Runtime->DeferredEvents.Insert(UniqueReplay, 0); }
            else
            {
                // 不部分入队：有界新事件队列满时，已接受载荷原子转移到待重放所有权，Flush明确报告未完成。
                Runtime->PendingReplayEvents.Append(UniqueReplay);
                Runtime->LastPersistenceError = EGamePlatformQuestError::PersistenceOutcomeUnknown;
            }
            Runtime->PendingEventPayloads.Reset(); Runtime->PendingEventIds.Reset();
            Runtime->RecentEventIdsByQuest.Reset(); Runtime->RecentEventOrderByQuest.Reset();
            Runtime->bReady = true; Runtime->bReconcileRequired = false;
            BuildEventIndex(*Runtime); PublishSnapshot(*Runtime);
        }
        else { Runtime->LastPersistenceError = ApplyError; }
    }

    FinishPersistenceOperation(PlayerId, *Runtime);
}

bool UGamePlatformQuestServerSubsystem::BuildEventIndex(
    FPlayerRuntime& Runtime)
{
    Runtime.EventIndex.Reset();

    for (const TPair<FName, FGamePlatformQuestSnapshot>& Pair :
         Runtime.Quests)
    {
        if (!IsActiveState(Pair.Value.State))
        {
            continue;
        }

        UGamePlatformQuestDefinition* Definition =
            Definitions.FindRef(Pair.Key).Get();

        if (!IsValid(Definition))
        {
            continue;
        }

        for (int32 Index = 0;
             Index < Definition->Objectives.Num();
             ++Index)
        {
            const FGamePlatformQuestObjectiveDefinition& Objective =
                Definition->Objectives[Index];

            FObjectiveBinding Binding;
            Binding.QuestId = Definition->QuestId;
            Binding.ObjectiveIndex = Index;

            Runtime.EventIndex.FindOrAdd(
                Objective.EventType).Add(Binding);
        }
    }

    return true;
}

void UGamePlatformQuestServerSubsystem::PublishSnapshot(
    FPlayerRuntime& Runtime)
{
    UGamePlatformQuestStateComponent* StateComponent =
        Runtime.StateComponent.Get();

    if (!IsValid(StateComponent))
    {
        return;
    }

    TArray<FGamePlatformQuestSnapshot> Snapshots;
    Runtime.Quests.GenerateValueArray(Snapshots);

    Snapshots.Sort(
        [](const FGamePlatformQuestSnapshot& A,
           const FGamePlatformQuestSnapshot& B)
        {
            return A.QuestId.LexicalLess(B.QuestId);
        });

    StateComponent->SetServerQuestSnapshots(Snapshots);
}

bool UGamePlatformQuestServerSubsystem::ArePrerequisitesMet(
    const FPlayerRuntime& Runtime,
    const UGamePlatformQuestDefinition& Definition) const
{
    for (const FGamePlatformQuestPrerequisite& Prerequisite :
         Definition.Prerequisites)
    {
        const FGamePlatformQuestSnapshot* Snapshot =
            Runtime.Quests.Find(Prerequisite.CompletedQuestId);

        if (!Snapshot ||
            Snapshot->State != EGamePlatformQuestState::Completed)
        {
            return false;
        }
    }

    return true;
}

bool UGamePlatformQuestServerSubsystem::WouldCreatePrerequisiteCycle(
    FName QuestId,
    TSet<FName>& Visiting,
    TSet<FName>& Visited) const
{
    if (Visited.Contains(QuestId))
    {
        return false;
    }

    if (Visiting.Contains(QuestId))
    {
        return true;
    }

    Visiting.Add(QuestId);

    const UGamePlatformQuestDefinition* Definition =
        Definitions.FindRef(QuestId).Get();

    if (IsValid(Definition))
    {
        for (const FGamePlatformQuestPrerequisite& Prerequisite :
             Definition->Prerequisites)
        {
            if (Definitions.Contains(Prerequisite.CompletedQuestId) &&
                WouldCreatePrerequisiteCycle(
                    Prerequisite.CompletedQuestId,
                    Visiting,
                    Visited))
            {
                return true;
            }
        }
    }

    Visiting.Remove(QuestId);
    Visited.Add(QuestId);
    return false;
}

bool UGamePlatformQuestServerSubsystem::MatchesObjective(
    const FGamePlatformQuestEvent& Event,
    const FGamePlatformQuestObjectiveDefinition& Objective) const
{
    if (Event.EventType != Objective.EventType)
    {
        return false;
    }

    if (!Event.SemanticTags.HasAll(
            Objective.RequiredSemanticTags))
    {
        return false;
    }

    if (Objective.ObjectiveType ==
            EGamePlatformQuestObjectiveType::Region &&
        Event.RegionId != Objective.RequiredRegionId)
    {
        return false;
    }

    return true;
}

bool UGamePlatformQuestServerSubsystem::AllRequiredObjectivesComplete(
    const FGamePlatformQuestSnapshot& Snapshot,
    const UGamePlatformQuestDefinition& Definition) const
{
    for (int32 Index = 0;
         Index < Definition.Objectives.Num();
         ++Index)
    {
        if (Definition.Objectives[Index].bOptional)
        {
            continue;
        }

        if (!Snapshot.Objectives.IsValidIndex(Index) ||
            !Snapshot.Objectives[Index].bCompleted)
        {
            return false;
        }
    }

    return true;
}

void UGamePlatformQuestServerSubsystem::ScheduleProgressFlush(
    const FString& PlayerId,
    FPlayerRuntime& Runtime)
{
    if (!GetWorld() ||
        Runtime.bPersistenceInFlight ||
        (!Runtime.bReconcileRequired &&
         Runtime.PendingEventIds.IsEmpty()) ||
        GetWorld()->GetTimerManager().IsTimerActive(
            Runtime.ProgressFlushTimer))
    {
        return;
    }

    const float Delay =
        FMath::Max(
            0.1f,
            GetDefault<UGamePlatformQuestSettings>()
                ->ProgressFlushDelaySeconds);

    GetWorld()->GetTimerManager().SetTimer(
        Runtime.ProgressFlushTimer,
        FTimerDelegate::CreateUObject(
            this,
            &UGamePlatformQuestServerSubsystem::FlushPlayerProgress,
            PlayerId),
        Delay,
        false);
}

void UGamePlatformQuestServerSubsystem::FlushPlayerProgress(
    FString PlayerId)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime ||
        !Runtime->Persistence.IsValid() ||
        Runtime->bPersistenceInFlight)
    {
        return;
    }

    if (Runtime->bReconcileRequired)
    {
        BeginReconcilePlayer(PlayerId, *Runtime);
        return;
    }
    if (!Runtime->bReady) { return; }

    TArray<FName> QuestIds;
    Runtime->PendingEventIds.GetKeys(QuestIds);

    QuestIds.Sort(
        [](const FName A, const FName B)
        {
            return A.LexicalLess(B);
        });

    for (const FName QuestId : QuestIds)
    {
        if (BeginPersistQuest(
                PlayerId,
                *Runtime,
                QuestId))
        {
            return;
        }
    }

    FinishPersistenceOperation(PlayerId, *Runtime);
}

bool UGamePlatformQuestServerSubsystem::BeginPersistQuest(
    const FString& PlayerId,
    FPlayerRuntime& Runtime,
    FName QuestId)
{
    TArray<FGuid>* EventIds =
        Runtime.PendingEventIds.Find(QuestId);

    FGamePlatformQuestSnapshot* Snapshot =
        Runtime.Quests.Find(QuestId);

    if (!EventIds ||
        EventIds->IsEmpty() ||
        !Snapshot ||
        !IsActiveState(Snapshot->State))
    {
        Runtime.PendingEventIds.Remove(QuestId);
        return false;
    }

    const TArray<FGuid> EventsToPersist = *EventIds;
    const FGamePlatformQuestSnapshot Proposed = *Snapshot;
    const int64 ExpectedRevision = Snapshot->Revision;
    const FGuid ExpectedRuntimeId = Runtime.PlayerRuntimeId;
    TWeakObjectPtr<UGamePlatformQuestServerSubsystem> WeakThis(this);

    Runtime.bPersistenceInFlight = true;

    FGamePlatformQuestMutationCompletion Completion =
        [WeakThis,
         PlayerId,
         ExpectedRuntimeId,
         QuestId,
         EventsToPersist](
            FGamePlatformQuestSnapshot Persisted,
            EGamePlatformQuestError Error)
        {
            if (UGamePlatformQuestServerSubsystem* Self = WeakThis.Get())
            {
                Self->HandlePersistCompleted(
                    PlayerId,
                    ExpectedRuntimeId,
                    QuestId,
                    EventsToPersist,
                    MoveTemp(Persisted),
                    Error);
            }
        };

    bool bStarted = false;

    if (Snapshot->State ==
        EGamePlatformQuestState::CompletionPending)
    {
        if (!Snapshot->CompletionId.IsValid())
        {
            Runtime.bPersistenceInFlight = false;
            return false;
        }

        bStarted =
            Runtime.Persistence->BeginCompleteQuest(
                PlayerId,
                Runtime.CharacterId,
                Proposed,
                ExpectedRevision,
                EventsToPersist,
                Snapshot->CompletionId,
                MoveTemp(Completion));
    }
    else
    {
        bStarted =
            Runtime.Persistence->BeginPersistQuestEvents(
                PlayerId,
                Proposed,
                ExpectedRevision,
                EventsToPersist,
                MoveTemp(Completion));
    }

    if (!bStarted)
    {
        Runtime.bPersistenceInFlight = false;
        ScheduleProgressFlush(PlayerId, Runtime);
        return false;
    }

    return true;
}

void UGamePlatformQuestServerSubsystem::HandlePersistCompleted(
    FString PlayerId,
    FGuid ExpectedRuntimeId,
    FName QuestId,
    TArray<FGuid> EventIds,
    FGamePlatformQuestSnapshot Persisted,
    EGamePlatformQuestError Error)
{
    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime || Runtime->PlayerRuntimeId != ExpectedRuntimeId)
    {
        return;
    }

    Runtime->bPersistenceInFlight = false;

    if (Error == EGamePlatformQuestError::None)
    {
        if (FGamePlatformQuestSnapshot* Snapshot =
            Runtime->Quests.Find(QuestId))
        {
            *Snapshot = MoveTemp(Persisted);
        }

        if (TArray<FGuid>* Pending =
            Runtime->PendingEventIds.Find(QuestId))
        {
            for (const FGuid& EventId : EventIds)
            {
                Pending->Remove(EventId);
            }

            if (Pending->IsEmpty())
            {
                Runtime->PendingEventIds.Remove(QuestId);
            }

            RemovePersistedEventPayloads(
                *Runtime,
                EventIds);
        }

        BuildEventIndex(*Runtime);
        PublishSnapshot(*Runtime);
    }
    else if (IsPersistenceConflict(Error))
    {
        Runtime->bReconcileRequired = true;
        BeginReconcilePlayer(PlayerId, *Runtime);
        return;
    }
    else
    {
        ScheduleProgressFlush(PlayerId, *Runtime);
    }

    FinishPersistenceOperation(PlayerId, *Runtime);
}

void UGamePlatformQuestServerSubsystem::FinishPersistenceOperation(
    const FString& PlayerId,
    FPlayerRuntime& Runtime)
{
    if (Runtime.bPersistenceInFlight)
    {
        return;
    }

    if (!Runtime.bReconcileRequired)
    {
        ProcessDeferredEvents(PlayerId, Runtime);
    }

    FPlayerRuntime* Current = Players.Find(PlayerId);
    if (!Current)
    {
        return;
    }

    if (!Current->bPersistenceInFlight &&
        (Current->bReconcileRequired ||
         !Current->PendingEventIds.IsEmpty()))
    {
        ScheduleProgressFlush(PlayerId, *Current);
    }

    if (Current->bUnregisterWhenIdle &&
        !Current->bPersistenceInFlight &&
        Current->PendingEventIds.IsEmpty() &&
        Current->DeferredEvents.IsEmpty() && Current->PendingReplayEvents.IsEmpty())
    {
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(
                Current->ProgressFlushTimer);
        }

        Players.Remove(PlayerId);
    }
}

bool UGamePlatformQuestServerSubsystem::FlushPlayerProgressNow(
    const FString& PlayerId,
    EGamePlatformQuestError& OutError)
{
    OutError = EGamePlatformQuestError::None;

    FPlayerRuntime* Runtime = Players.Find(PlayerId);
    if (!Runtime ||
        !Runtime->Persistence.IsValid())
    {
        OutError = EGamePlatformQuestError::PersistenceUnavailable;
        return false;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(
            Runtime->ProgressFlushTimer);
    }

    if (!Runtime->bPersistenceInFlight &&
        (Runtime->bReconcileRequired ||
         !Runtime->PendingEventIds.IsEmpty()))
    {
        FlushPlayerProgress(PlayerId);
    }

    Runtime = Players.Find(PlayerId);
    if (!Runtime)
    {
        return true;
    }

    if (Runtime->bPersistenceInFlight ||
        !Runtime->PendingEventIds.IsEmpty() ||
        !Runtime->DeferredEvents.IsEmpty() || !Runtime->PendingReplayEvents.IsEmpty() || Runtime->bReconcileRequired)
    {
        OutError =
            EGamePlatformQuestError::PersistenceOutcomeUnknown;
        return false;
    }

    Runtime->LastPersistenceError = EGamePlatformQuestError::None;
    return true;
}

bool UGamePlatformQuestServerSubsystem::IsPlayerReady(
    const FString& PlayerId) const
{
    const FPlayerRuntime* Runtime = Players.Find(PlayerId);
    return Runtime &&
           Runtime->bReady &&
           !Runtime->bReconcileRequired &&
           !Runtime->bUnregisterWhenIdle;
}

void UGamePlatformQuestServerSubsystem::RemovePersistedEventPayloads(
    FPlayerRuntime& Runtime,
    const TArray<FGuid>& EventIds)
{
    for (const FGuid& EventId : EventIds)
    {
        bool bStillPending = false;

        for (const TPair<FName, TArray<FGuid>>& Pair :
             Runtime.PendingEventIds)
        {
            if (Pair.Value.Contains(EventId))
            {
                bStillPending = true;
                break;
            }
        }

        if (!bStillPending)
        {
            Runtime.PendingEventPayloads.Remove(EventId);
        }
    }
}

void UGamePlatformQuestServerSubsystem::RememberEvent(
    FPlayerRuntime& Runtime,
    FName QuestId,
    const FGuid& EventId)
{
    if (QuestId.IsNone() || !EventId.IsValid())
    {
        return;
    }

    TSet<FGuid>& EventIds =
        Runtime.RecentEventIdsByQuest.FindOrAdd(QuestId);

    if (EventIds.Contains(EventId))
    {
        return;
    }

    EventIds.Add(EventId);

    TArray<FGuid>& Order =
        Runtime.RecentEventOrderByQuest.FindOrAdd(QuestId);

    Order.Add(EventId);

    const int32 Limit =
        FMath::Max(
            16,
            GetDefault<UGamePlatformQuestSettings>()
                ->MaxRecentEventIds);

    while (Order.Num() > Limit)
    {
        const FGuid Oldest = Order[0];
        Order.RemoveAt(0, 1, EAllowShrinking::No);
        EventIds.Remove(Oldest);
    }
}

bool UGamePlatformQuestServerSubsystem::HasSeenEvent(
    const FPlayerRuntime& Runtime,
    FName QuestId,
    const FGuid& EventId) const
{
    const TSet<FGuid>* Events =
        Runtime.RecentEventIdsByQuest.Find(QuestId);

    return Events && Events->Contains(EventId);
}

TArray<FGuid>
UGamePlatformQuestServerSubsystem::CollectPendingEventIds(
    FPlayerRuntime& Runtime,
    FName QuestId,
    const FGuid& CurrentEventId) const
{
    TArray<FGuid> Result;

    if (const TArray<FGuid>* Pending =
        Runtime.PendingEventIds.Find(QuestId))
    {
        Result = *Pending;
    }

    Result.AddUnique(CurrentEventId);
    return Result;
}

FGamePlatformQuestSnapshot*
UGamePlatformQuestServerSubsystem::FindActiveQuest(
    FPlayerRuntime& Runtime,
    FName QuestId)
{
    FGamePlatformQuestSnapshot* Snapshot =
        Runtime.Quests.Find(QuestId);

    return Snapshot && IsActiveState(Snapshot->State)
        ? Snapshot
        : nullptr;
}


EGamePlatformQuestError UGamePlatformQuestServerSubsystem::GetPlayerPersistenceError(const FString& PlayerId) const
{
    check(IsInGameThread());
    const FPlayerRuntime* Runtime = Players.Find(PlayerId);
    return Runtime ? Runtime->LastPersistenceError : EGamePlatformQuestError::PersistenceUnavailable;
}
