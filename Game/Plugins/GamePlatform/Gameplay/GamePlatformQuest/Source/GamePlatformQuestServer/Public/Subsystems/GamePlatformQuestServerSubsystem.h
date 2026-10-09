#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Types/GamePlatformQuestTypes.h"
#include "GamePlatformQuestServerSubsystem.generated.h"

class IGamePlatformQuestPersistencePort;
class UGamePlatformQuestDefinition;
class UGamePlatformQuestStateComponent;

UCLASS()
class GAMEPLATFORMQUESTSERVER_API UGamePlatformQuestServerSubsystem final
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    bool RegisterDefinition(UGamePlatformQuestDefinition* Definition);
    bool ValidateDefinitionGraph(FText& OutReason) const;

    /** 异步加载PlayerData任务快照；true只表示请求已启动。 */
    bool RegisterPlayer(
        const FString& PlayerId,
        const FString& CharacterId,
        const FGuid& PlayerRuntimeId,
        UGamePlatformQuestStateComponent* StateComponent,
        TSharedPtr<IGamePlatformQuestPersistencePort, ESPMode::ThreadSafe> Persistence,
        EGamePlatformQuestError& OutError);

    void UnregisterPlayer(const FString& PlayerId);

    EGamePlatformQuestError AcceptQuest(
        const FString& PlayerId,
        FName QuestId);

    EGamePlatformQuestError AbandonQuest(
        const FString& PlayerId,
        FName QuestId);

    EGamePlatformQuestError HandleQuestEvent(
        const FGamePlatformQuestEvent& Event);

    /**
     * 迁服/登出前触发关键进度刷新。
     * 如果仍有异步持久化在途，返回false+PersistenceOutcomeUnknown。
     */
    bool FlushPlayerProgressNow(
        const FString& PlayerId,
        EGamePlatformQuestError& OutError);

    /** 游戏线程读取最近对账/持久化状态；队列满时为PersistenceOutcomeUnknown，已接纳事件仍由本实例保留。 */
    EGamePlatformQuestError GetPlayerPersistenceError(const FString& PlayerId) const;

    UFUNCTION(BlueprintPure, Category="Quest")
    bool IsPlayerReady(const FString& PlayerId) const;

private:
    struct FObjectiveBinding
    {
        FName QuestId = NAME_None;
        int32 ObjectiveIndex = INDEX_NONE;
    };

    /** 已接纳未提交的事件归属：同一EventId可命中多个任务，重放不能再次修改已经提交的任务。 */
    struct FQuestReplayEvent
    {
        FName QuestId;
        FGuid QuestInstanceId;
        FGamePlatformQuestEvent Event;
    };

    struct FPlayerRuntime
    {
        FString CharacterId;
        FGuid PlayerRuntimeId;
        TWeakObjectPtr<UGamePlatformQuestStateComponent> StateComponent;
        TSharedPtr<IGamePlatformQuestPersistencePort, ESPMode::ThreadSafe> Persistence;
        TMap<FName, FGamePlatformQuestSnapshot> Quests;
        TMap<FName, TArray<FObjectiveBinding>> EventIndex;
        TMap<FName, TSet<FGuid>> RecentEventIdsByQuest;
        TMap<FName, TArray<FGuid>> RecentEventOrderByQuest;
        TMap<FName, TArray<FGuid>> PendingEventIds;
        TMap<FGuid, FGamePlatformQuestEvent> PendingEventPayloads;
        TArray<FGamePlatformQuestEvent> DeferredEvents;
        /** 已接纳但须在新权威快照上重放的事件；与新事件限长队列分离，所有权转移后才清旧账本。 */
        TArray<FQuestReplayEvent> PendingReplayEvents;
        EGamePlatformQuestError LastPersistenceError = EGamePlatformQuestError::None;
        FTimerHandle ProgressFlushTimer;
        bool bReady = false;
        bool bPersistenceInFlight = false;
        bool bReconcileRequired = false;
        bool bUnregisterWhenIdle = false;
    };

    TMap<FName, TWeakObjectPtr<UGamePlatformQuestDefinition>> Definitions;
    TMap<FString, FPlayerRuntime> Players;

    void HandleInitialLoadCompleted(
        FString PlayerId,
        FGuid ExpectedRuntimeId,
        TArray<FGamePlatformQuestSnapshot> Loaded,
        EGamePlatformQuestError Error);

    /** 完整校验持久快照；Completed旧版本仅保留自含历史事实，不为旧版本重建当前目标。 */
    bool ValidatePersistedSnapshot(const FGamePlatformQuestSnapshot& Snapshot, EGamePlatformQuestError& OutError) const;
    /** 广播可能注销/重注册玩家；后续只能重新取得同一运行代次，不能继续使用广播前裸引用。 */
    FPlayerRuntime* FindPlayerRuntime(const FString& PlayerId, const FGuid& ExpectedRuntimeId);

    bool ApplyLoadedSnapshots(
        FPlayerRuntime& Runtime,
        const TArray<FGamePlatformQuestSnapshot>& Loaded,
        EGamePlatformQuestError& OutError);

    void BeginReconcilePlayer(
        const FString& PlayerId,
        FPlayerRuntime& Runtime);

    void HandleReconcileCompleted(
        FString PlayerId,
        FGuid ExpectedRuntimeId,
        TArray<FGamePlatformQuestSnapshot> Loaded,
        EGamePlatformQuestError Error);

    bool BuildEventIndex(FPlayerRuntime& Runtime);
    void PublishSnapshot(FPlayerRuntime& Runtime);

    bool ArePrerequisitesMet(
        const FPlayerRuntime& Runtime,
        const UGamePlatformQuestDefinition& Definition) const;

    bool WouldCreatePrerequisiteCycle(
        FName QuestId,
        TSet<FName>& Visiting,
        TSet<FName>& Visited) const;

    bool MatchesObjective(
        const FGamePlatformQuestEvent& Event,
        const struct FGamePlatformQuestObjectiveDefinition& Objective) const;

    bool AllRequiredObjectivesComplete(
        const FGamePlatformQuestSnapshot& Snapshot,
        const UGamePlatformQuestDefinition& Definition) const;

    EGamePlatformQuestError ProcessQuestEventNow(
        const FGamePlatformQuestEvent& Event,
        FPlayerRuntime& Runtime,
        FName ReplayQuestId = NAME_None,
        FGuid ReplayInstanceId = FGuid());

    bool EnqueueDeferredEvent(
        FPlayerRuntime& Runtime,
        const FGamePlatformQuestEvent& Event);

    void ProcessDeferredEvents(
        const FString& PlayerId,
        FGuid ExpectedRuntimeId);

    void ScheduleProgressFlush(
        const FString& PlayerId,
        FPlayerRuntime& Runtime);

    void FlushPlayerProgress(FString PlayerId);

    bool BeginPersistQuest(
        const FString& PlayerId,
        FPlayerRuntime& Runtime,
        FName QuestId);

    void HandlePersistCompleted(
        FString PlayerId,
        FGuid ExpectedRuntimeId,
        FName QuestId,
        TArray<FGuid> EventIds,
        FGamePlatformQuestSnapshot Persisted,
        EGamePlatformQuestError Error);

    void HandleAcceptCompleted(
        FString PlayerId,
        FGuid ExpectedRuntimeId,
        FName QuestId,
        FGamePlatformQuestSnapshot Persisted,
        EGamePlatformQuestError Error);

    void HandleAbandonCompleted(
        FString PlayerId,
        FGuid ExpectedRuntimeId,
        FName QuestId,
        FGamePlatformQuestSnapshot Persisted,
        EGamePlatformQuestError Error);

    void FinishPersistenceOperation(
        const FString& PlayerId,
        FGuid ExpectedRuntimeId);

    void RemovePersistedEventPayloads(
        FPlayerRuntime& Runtime,
        const TArray<FGuid>& EventIds);

    void RememberEvent(
        FPlayerRuntime& Runtime,
        FName QuestId,
        const FGuid& EventId);

    bool HasSeenEvent(
        const FPlayerRuntime& Runtime,
        FName QuestId,
        const FGuid& EventId) const;

    TArray<FGuid> CollectPendingEventIds(
        FPlayerRuntime& Runtime,
        FName QuestId,
        const FGuid& CurrentEventId) const;

    FGamePlatformQuestSnapshot* FindActiveQuest(
        FPlayerRuntime& Runtime,
        FName QuestId);
};
