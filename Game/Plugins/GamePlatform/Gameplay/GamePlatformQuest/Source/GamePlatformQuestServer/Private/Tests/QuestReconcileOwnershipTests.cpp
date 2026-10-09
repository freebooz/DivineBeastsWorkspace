#if WITH_DEV_AUTOMATION_TESTS
// 仅测试专用异步端口夹具：手动完成回调模拟Revision冲突，不连接生产HTTP/数据库，不冒充后端实现。
#include "Misc/AutomationTest.h"
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
    { auto Completion = MoveTemp(Load); Completion({Snapshot}, EGamePlatformQuestError::None); }
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
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
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
    Snapshot.Revision = 2; Port->FinishLoad(Snapshot);
    TestEqual(TEXT("满队列对账后全部17条接纳事件仍在权威进度"), State->GetQuestSnapshots()[0].Objectives[0].CurrentValue, 17.0);
    TestFalse(TEXT("尚未落库不能报告Flush成功"), Server->FlushPlayerProgressNow(TEXT("Player"), Error));
    TestEqual(TEXT("实际持久化提案包含全部重放事件"), Port->LastProposed.Objectives[0].CurrentValue, 17.0);
    World->DestroyWorld(false); return true;
}
#endif
