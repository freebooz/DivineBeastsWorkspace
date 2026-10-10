// 只读竞技投影回归：离开竞技世界后旧比赛身份、比分和HUD路由全部清空，仍允许新的匹配流程事件驱动。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformArenaViewResetTest,
    "GamePlatform.Arena.Client.ResetReplicatedWorldView",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformArenaViewResetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto* View = NewObject<UGamePlatformArenaViewModel>();
    View->MatchId = TEXT("PreviousMatch"); View->ArenaModeId = TEXT("Arena.1v1");
    View->FlowState = EGamePlatformArenaClientFlowState::InMatch;
    View->MatchPhase = EGamePlatformArenaMatchPhase::InProgress;
    View->Teams.AddDefaulted(); View->Scoreboard.AddDefaulted(); View->RemainingPhaseSeconds = 99.0;
    View->ResetReplicatedArenaState();
    TestTrue(TEXT("旧身份清空"), View->MatchId.IsEmpty() && View->ArenaModeId.IsNone());
    TestTrue(TEXT("旧比分/统计清空"), View->Teams.IsEmpty() && View->Scoreboard.IsEmpty());
    TestEqual(TEXT("离场路由Idle"), View->FlowState, EGamePlatformArenaClientFlowState::Idle);
    TestEqual(TEXT("旧阶段计时清空"), View->RemainingPhaseSeconds, 0.0);
    View->SetObservedClientFlowState(EGamePlatformArenaClientFlowState::Matchmaking);
    TestEqual(TEXT("后续新匹配事件可用"), View->FlowState, EGamePlatformArenaClientFlowState::Matchmaking);
    return true;
}
#endif
