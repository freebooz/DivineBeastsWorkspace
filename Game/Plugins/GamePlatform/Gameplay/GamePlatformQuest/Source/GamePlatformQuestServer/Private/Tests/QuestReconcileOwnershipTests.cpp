#if WITH_DEV_AUTOMATION_TESTS
// 仅测试专用异步端口夹具：手动完成回调模拟Revision冲突，不连接生产HTTP/数据库，不冒充后端实现。
#include "Misc/AutomationTest.h"
#include "Tests/QuestSnapshotReentryFixture.h"
#include "Subsystems/GamePlatformQuestServerSubsystem.h"
#include "Interfaces/GamePlatformQuestPersistencePort.h"
#include "Components/GamePlatformQuestStateComponent.h"
#include "Definitions/GamePlatformQuestDefinition.h"
#include "Settings/GamePlatformQuestSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
class FQuestPersistenceFixture final : public IGamePlatformQuestPersistencePort
{
public:
    FGamePlatformQuestLoadCompletion Load;
    FGamePlatformQuestMutationCompletion Mutation;
    FGamePlatformQuestSnapshot LastProposed;
    bool BeginLoadPlayerQuestSnapshot(const FString&, FGamePlatformQuestLoadCompletion Completion) override
    { Load = MoveTemp(Completion); return true; }
    bool BeginAcceptQuest(const FString&, const FGamePlatformQuestSnapshot&, EGamePlatformQuestRepeatPolicy,
        FGamePlatformQuestMutationCompletion) override { return false; }
    bool BeginPersistQuestEvents(const FString&, const FGamePlatformQuestSnapshot& Proposed, int64,
        const TArray<FGuid>&, FGamePlatformQuestMutationCompletion Completion) override
    { LastProposed = Proposed; Mutation = MoveTemp(Completion); return true; }
    bool BeginCompleteQuest(const FString&, const FString&, const FGamePlatformQuestSnapshot&, int64,
        const TArray<FGuid>&, const FGuid&, FGamePlatformQuestMutationCompletion) override { return false; }
    bool BeginAbandonQuest(const FString&, const FGamePlatformQuestSnapshot&, int64,
        FGamePlatformQuestMutationCompletion) override { return false; }
    void FinishLoad(const FGamePlatformQuestSnapshot& Snapshot)
    { FinishLoadMany({Snapshot}); }
    void FinishLoadMany(TArray<FGamePlatformQuestSnapshot> Snapshots)
    { auto Completion = MoveTemp(Load); Completion(MoveTemp(Snapshots), EGamePlatformQuestError::None); }
    void FinishSnapshot(FGamePlatformQuestSnapshot Snapshot, EGamePlatformQuestError Error)
    { auto Completion = MoveTemp(Mutation); Completion(MoveTemp(Snapshot), Error); }
    void FinishSuccess()
    { auto Completion = MoveTemp(Mutation); auto Snapshot = LastProposed; ++Snapshot.Revision; Completion(Snapshot, EGamePlatformQuestError::None); }
    void FinishDuplicate()
    { auto Completion = MoveTemp(Mutation); auto Snapshot = LastProposed; ++Snapshot.Revision; Completion(Snapshot, EGamePlatformQuestError::DuplicateEvent); }
    void FailMutation()
    { auto Completion = MoveTemp(Mutation); Completion({}, EGamePlatformQuestError::RevisionConflict); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestReconcileOwnershipTest,
    "GamePlatform.Quest.Server.ReconcileRetainsAcceptedEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuestReconcileOwnershipTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TGuardValue<int32> QueueLimit(GetMutableDefault<UGamePlatformQuestSettings>()->MaxDeferredEventsPerPlayer, 16);
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Server = World->GetSubsystem<UGamePlatformQuestServerSubsystem>();
    AActor* Owner = World->SpawnActor<AActor>();
    auto* State = NewObject<UGamePlatformQuestStateComponent>(Owner); Owner->AddInstanceComponent(State); State->RegisterComponent();
    auto* Definition = NewObject<UGamePlatformQuestDefinition>(); Definition->QuestId = TEXT("Quest.Reconcile");
    FGamePlatformQuestObjectiveDefinition Objective; Objective.ObjectiveId = TEXT("Count");
    Objective.EventType = TEXT("Quest.TestEvent"); Objective.RequiredValue = 1000; Definition->Objectives.Add(Objective);
    TestTrue(TEXT("合法任务定义注册"), Server->RegisterDefinition(Definition));
    FGamePlatformQuestSnapshot Snapshot; Snapshot.QuestId = Definition->QuestId; Snapshot.DefinitionVersion = 1;
    Snapshot.QuestInstanceId = FGuid::NewGuid(); Snapshot.State = EGamePlatformQuestState::Active; Snapshot.Revision = 1;
    FGamePlatformQuestObjectiveProgress Progress; Progress.ObjectiveId = Objective.ObjectiveId; Progress.RequiredValue = 1000;
    Snapshot.Objectives.Add(Progress);
    auto Port = MakeShared<FQuestPersistenceFixture, ESPMode::ThreadSafe>();
    const FGuid RuntimeId = FGuid::NewGuid(); EGamePlatformQuestError Error;
    TestTrue(TEXT("玩家加载开始"), Server->RegisterPlayer(TEXT("Player"), TEXT("Character"), RuntimeId, State, Port, Error));
    Port->FinishLoad(Snapshot);
    const auto Event = [&]() { FGamePlatformQuestEvent Value; Value.EventId = FGuid::NewGuid(); Value.PlayerId = TEXT("Player");
        Value.PlayerRuntimeId = RuntimeId; Value.EventType = Objective.EventType; Value.OccurredAtUtc = FDateTime::UtcNow(); return Value; };
    TestEqual(TEXT("第一条事件接纳"), Server->HandleQuestEvent(Event()), EGamePlatformQuestError::None);
    Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    for (int32 Index = 0; Index < 16; ++Index)
    { TestEqual(TEXT("在途期间队列内事件均接纳"), Server->HandleQuestEvent(Event()), EGamePlatformQuestError::None); }
    Port->FailMutation();
    auto InvalidSnapshot = Snapshot; InvalidSnapshot.DefinitionVersion = 99;
    Port->FinishLoad(InvalidSnapshot);
    TestEqual(TEXT("不兼容加载明确拒绝"), Server->GetPlayerPersistenceError(TEXT("Player")), EGamePlatformQuestError::DefinitionVersionMismatch);
    TestEqual(TEXT("加载失败保留已发布进度"), State->GetQuestSnapshots()[0].Objectives[0].CurrentValue, 1.0);
    Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    // 同Version也必须拒绝缺目标/错目标/无效实例，且每次都保留旧进度与全部待重放归属。
    for (int32 Case = 0; Case < 4; ++Case)
    {
        auto Malformed = Snapshot;
        if (Case == 0) { Malformed.Objectives.Reset(); }
        if (Case == 1) { Malformed.Objectives[0].ObjectiveId = TEXT("WrongObjective"); }
        if (Case == 2) { Malformed.QuestInstanceId.Invalidate(); }
        if (Case == 3) { Malformed.QuestInstanceId = FGuid::NewGuid(); }
        Port->FinishLoad(Malformed);
        TestEqual(TEXT("同版本损坏快照明确失败"), Server->GetPlayerPersistenceError(TEXT("Player")), Case == 3 ? EGamePlatformQuestError::PersistenceOutcomeUnknown : EGamePlatformQuestError::InvalidQuestEvent);
        TestEqual(TEXT("损坏快照不替换已接纳进度"), State->GetQuestSnapshots()[0].Objectives[0].CurrentValue, 1.0);
        Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    }
    Snapshot.Revision = 2; Port->FinishLoad(Snapshot);
    TestEqual(TEXT("满队列对账后全部17条接纳事件仍在权威进度"), State->GetQuestSnapshots()[0].Objectives[0].CurrentValue, 17.0);
    TestFalse(TEXT("尚未落库不能报告Flush成功"), Server->FlushPlayerProgressNow(TEXT("Player"), Error));
    TestEqual(TEXT("实际持久化提案包含全部重放事件"), Port->LastProposed.Objectives[0].CurrentValue, 17.0);
    Port->FinishDuplicate();
    TestEqual(TEXT("已提交重复事件直接消费权威快照，不发起循环对账加载"), State->GetQuestSnapshots()[0].Objectives[0].CurrentValue, 17.0);
    TestFalse(TEXT("幂等成功没有再次请求加载"), bool(Port->Load));
    TestTrue(TEXT("持久幂等确认后Flush完成"), Server->FlushPlayerProgressNow(TEXT("Player"), Error));
    World->DestroyWorld(false); return true;
}
// 两个任务共享同一事实：先确认Q1，再令Q2冲突；只重放Q2，Q1的持久快照与去重账本均不能被清空。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPerInstanceReplayTest,
    "GamePlatform.Quest.Server.ReplayOnlyUncommittedQuestInstance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuestPerInstanceReplayTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Server = World->GetSubsystem<UGamePlatformQuestServerSubsystem>(); auto* Owner = World->SpawnActor<AActor>();
    auto* State = NewObject<UGamePlatformQuestStateComponent>(Owner); Owner->AddInstanceComponent(State); State->RegisterComponent();
    TArray<FGamePlatformQuestSnapshot> Snapshots;
    for (const FName QuestId : {FName(TEXT("Quest.Q1")), FName(TEXT("Quest.Q2"))})
    {
        auto* Definition = NewObject<UGamePlatformQuestDefinition>(); Definition->QuestId = QuestId;
        FGamePlatformQuestObjectiveDefinition Objective; Objective.ObjectiveId = TEXT("Count");
        Objective.EventType = TEXT("Quest.SharedEvent"); Objective.RequiredValue = 100; Definition->Objectives.Add(Objective);
        TestTrue(TEXT("任务定义注册"), Server->RegisterDefinition(Definition));
        FGamePlatformQuestSnapshot Snapshot; Snapshot.QuestId = QuestId; Snapshot.QuestInstanceId = FGuid::NewGuid();
        Snapshot.DefinitionVersion = 1; Snapshot.Revision = 1; Snapshot.State = EGamePlatformQuestState::Active;
        FGamePlatformQuestObjectiveProgress Progress; Progress.ObjectiveId = Objective.ObjectiveId; Progress.RequiredValue = 100;
        Snapshot.Objectives.Add(Progress); Snapshots.Add(Snapshot);
    }
    auto Port = MakeShared<FQuestPersistenceFixture, ESPMode::ThreadSafe>(); EGamePlatformQuestError Error; const FGuid RuntimeId = FGuid::NewGuid();
    TestTrue(TEXT("加载玩家"), Server->RegisterPlayer(TEXT("Player"), TEXT("Character"), RuntimeId, State, Port, Error));
    Port->FinishLoadMany(Snapshots);
    FGamePlatformQuestEvent Event; Event.EventId = FGuid::NewGuid(); Event.PlayerId = TEXT("Player"); Event.PlayerRuntimeId = RuntimeId;
    Event.EventType = TEXT("Quest.SharedEvent"); Event.OccurredAtUtc = FDateTime::UtcNow();
    Server->HandleQuestEvent(Event); Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    TestEqual(TEXT("第一笔实际提交Q1"), Port->LastProposed.QuestId, Snapshots[0].QuestId); Port->FinishSuccess();
    Snapshots[0] = Port->LastProposed; ++Snapshots[0].Revision;
    Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    TestEqual(TEXT("第二笔实际提交Q2"), Port->LastProposed.QuestId, Snapshots[1].QuestId); Port->FailMutation();
    Port->FinishLoadMany(Snapshots);
    const auto* Q1 = State->GetQuestSnapshots().FindByPredicate([](const auto& Value) { return Value.QuestId == TEXT("Quest.Q1"); });
    const auto* Q2 = State->GetQuestSnapshots().FindByPredicate([](const auto& Value) { return Value.QuestId == TEXT("Quest.Q2"); });
    TestNotNull(TEXT("保留Q1快照"), Q1); TestNotNull(TEXT("保留Q2快照"), Q2);
    if (Q1 && Q2) { TestEqual(TEXT("已提交Q1不重复累加"), Q1->Objectives[0].CurrentValue, 1.0);
        TestEqual(TEXT("未提交Q2只重放一次"), Q2->Objectives[0].CurrentValue, 1.0); }
    Server->HandleQuestEvent(Event); // 同EventId重复投递仍不能再累加Q1/Q2。
    Server->FlushPlayerProgressNow(TEXT("Player"), Error);
    TestEqual(TEXT("只持久化Q2而不是再次提交Q1"), Port->LastProposed.QuestId, Snapshots[1].QuestId);
    TestEqual(TEXT("重复投递没有重复累计Q2"), Port->LastProposed.Objectives[0].CurrentValue, 1.0);
    auto InvalidPersisted = Port->LastProposed; ++InvalidPersisted.Revision; InvalidPersisted.Objectives.Reset();
    Port->FinishSnapshot(InvalidPersisted, EGamePlatformQuestError::None);
    TestEqual(TEXT("成功回包缺目标不能消费待确认身份"), Server->GetPlayerPersistenceError(TEXT("Player")), EGamePlatformQuestError::PersistenceOutcomeUnknown);
    TestFalse(TEXT("损坏成功回包后仍须以原批次重试"), Server->FlushPlayerProgressNow(TEXT("Player"), Error));
    TestEqual(TEXT("重试仍为Q2原进度"), Port->LastProposed.Objectives[0].CurrentValue, 1.0);
    Port->FinishSuccess(); TestTrue(TEXT("两任务均已提交"), Server->FlushPlayerProgressNow(TEXT("Player"), Error));
    World->DestroyWorld(false); return true;
}

// 三条持久回调发布时都允许监听者同步注销/清理；旧完成回调不得访问已删除Runtime或驱动新玩家代次。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestSnapshotReentryTest,
    "GamePlatform.Quest.Server.SnapshotListenerRetiresRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuestSnapshotReentryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (int32 Stage = 0; Stage < 3; ++Stage)
    {
        // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
        const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &WorldInitializationValues);
        auto* Server = World->GetSubsystem<UGamePlatformQuestServerSubsystem>(); auto* Owner = World->SpawnActor<AActor>();
        auto* State = NewObject<UGamePlatformQuestStateComponent>(Owner); Owner->AddInstanceComponent(State); State->RegisterComponent();
        auto* Definition = NewObject<UGamePlatformQuestDefinition>(); Definition->QuestId = TEXT("Quest.Reentry");
        FGamePlatformQuestObjectiveDefinition Objective; Objective.ObjectiveId = TEXT("Count"); Objective.EventType = TEXT("Quest.Event");
        Objective.RequiredValue = 100; Definition->Objectives.Add(Objective); Server->RegisterDefinition(Definition);
        FGamePlatformQuestSnapshot Snapshot; Snapshot.QuestId = Definition->QuestId; Snapshot.QuestInstanceId = FGuid::NewGuid();
        Snapshot.DefinitionVersion = 1; Snapshot.Revision = 1; Snapshot.State = EGamePlatformQuestState::Active;
        FGamePlatformQuestObjectiveProgress Progress; Progress.ObjectiveId = Objective.ObjectiveId; Progress.RequiredValue = 100; Snapshot.Objectives.Add(Progress);
        auto Port = MakeShared<FQuestPersistenceFixture, ESPMode::ThreadSafe>(); EGamePlatformQuestError Error; const FGuid RuntimeId = FGuid::NewGuid();
        Server->RegisterPlayer(TEXT("Player"), TEXT("Character"), RuntimeId, State, Port, Error);
        auto* Listener = NewObject<UQuestSnapshotReentryFixture>(Owner);
        State->OnQuestSnapshotsChanged.AddDynamic(Listener, &UQuestSnapshotReentryFixture::HandleSnapshot);
        if (Stage != 0)
        {
            Port->FinishLoad(Snapshot);
            FGamePlatformQuestEvent Event; Event.EventId = FGuid::NewGuid(); Event.PlayerId = TEXT("Player");
            Event.PlayerRuntimeId = RuntimeId; Event.EventType = Objective.EventType; Server->HandleQuestEvent(Event);
            Server->FlushPlayerProgressNow(TEXT("Player"), Error);
            if (Stage == 1) { Port->FailMutation(); }
        }
        auto ReplacementPort = MakeShared<FQuestPersistenceFixture, ESPMode::ThreadSafe>();
        // 初载监听者真实注销再重注册同PlayerId；对账/persist监听者Deinitialize模拟世界服务立即退出。
        Listener->Handler = [&, Stage]()
        {
            if (Stage == 0) { Server->UnregisterPlayer(TEXT("Player")); Server->RegisterPlayer(TEXT("Player"), TEXT("Character"),
                FGuid::NewGuid(), State, ReplacementPort, Error); }
            else { Server->Deinitialize(); }
        };
        if (Stage == 2) { Port->FinishSuccess(); } else { Port->FinishLoad(Snapshot); }
        TestFalse(TEXT("已退休运行代次不再Ready"), Server->IsPlayerReady(TEXT("Player")));
        if (Stage == 0) { TestTrue(TEXT("新代次仍保留自己的初载操作"), bool(ReplacementPort->Load)); }
        State->OnQuestSnapshotsChanged.RemoveAll(Listener); World->DestroyWorld(false);
    }
    return true;
}
#endif
