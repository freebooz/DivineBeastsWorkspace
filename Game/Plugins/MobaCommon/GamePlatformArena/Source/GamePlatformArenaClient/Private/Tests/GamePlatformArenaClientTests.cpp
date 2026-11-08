#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Misc/AutomationTest.h"
#include "Client/GamePlatformArenaClientTypes.h"
#include "Screens/GamePlatformHUDWidget.h"
#include "Screens/GamePlatformModalScreen.h"
#include "Screens/GamePlatformUIScreen.h"
#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "UI/GamePlatformMobaArenaModalScreenBase.h"
#include "UI/GamePlatformMobaArenaScreenBase.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
#include "ViewModels/GamePlatformViewModelBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaClientRequestTest,
    "GamePlatform.Arena.Client.MatchmakingRequestAuthorityBoundary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaClientRequestTest::RunTest(const FString& Parameters)
{
    FGamePlatformArenaMatchmakingRequest Request;
    Request.ArenaModeId = TEXT("Arena.Mode.Team5v5");
    Request.PartyId = TEXT("party-1");
    Request.PreferredRegion = TEXT("auto");
    Request.ClientRequestId = TEXT("req-1");
    FString Error;
    TestTrue(TEXT("允许字段请求合法"), Request.IsValid(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaClientUIHierarchyTest,
    "GamePlatform.Arena.Client.UIHierarchy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaClientUIHierarchyTest::RunTest(const FString&)
{
    TestTrue(
        TEXT("ArenaViewModel必须复用平台ViewModel生命周期"),
        UGamePlatformArenaViewModel::StaticClass()->IsChildOf(
            UGamePlatformViewModelBase::StaticClass()));

    TestTrue(
        TEXT("MOBA竞技HUD必须继承平台HUD"),
        UGamePlatformMobaArenaHUDBase::StaticClass()->IsChildOf(
            UGamePlatformHUDWidget::StaticClass()));

    TestTrue(
        TEXT("MOBA竞技普通页面必须继承平台Screen"),
        UGamePlatformMobaArenaScreenBase::StaticClass()->IsChildOf(
            UGamePlatformUIScreen::StaticClass()));

    TestTrue(
        TEXT("MOBA竞技准备页基类必须继承平台Modal"),
        UGamePlatformMobaArenaModalScreenBase::StaticClass()->IsChildOf(
            UGamePlatformModalScreen::StaticClass()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaClientPlayerNameTest,
    "GamePlatform.Arena.Client.ScoreboardFollowsPlayerNameReplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaClientPlayerNameTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("竞技复制测试世界"), World)) { return false; }

    auto* GameState = World->SpawnActor<AGamePlatformArenaGameState>();
    auto* PlayerState = World->SpawnActor<AGamePlatformArenaPlayerState>();
    if (!TestNotNull(TEXT("竞技比赛状态"), GameState) || !TestNotNull(TEXT("竞技玩家状态"), PlayerState))
    {
        World->DestroyWorld(false);
        return false;
    }

    PlayerState->SetRole(ROLE_SimulatedProxy);
    PlayerState->PlayerIdPublic = TEXT("internal-player-id");
    PlayerState->TeamId = TEXT("Team.A");
    auto* View = NewObject<UGamePlatformArenaViewModel>();
    const auto Refresh = [View, GameState, PlayerState](AGamePlatformArenaPlayerState*)
    {
        View->RefreshFromReplicatedState(GameState, {PlayerState});
    };
    const FDelegateHandle StatsChanged = PlayerState->OnArenaStatsChanged.AddLambda(Refresh);
    PlayerState->OnRep_StatsRevision();
    if (TestEqual(TEXT("先收到统计时已显示该参赛者"), View->Scoreboard.Num(), 1))
    {
        TestEqual(TEXT("姓名未到达时显示通用名称"), View->Scoreboard[0].PlayerName, FString(TEXT("玩家")));
        PlayerState->SetPlayerNameInternal(TEXT("白君"));
        PlayerState->OnRep_PlayerName();
        TestEqual(TEXT("迟到的姓名通过事件更新结算投影"), View->Scoreboard[0].PlayerName, FString(TEXT("白君")));
    }
    PlayerState->OnArenaStatsChanged.Remove(StatsChanged);
    PlayerState->SetRole(ROLE_Authority);
    World->DestroyWorld(false);
    return true;
}

#endif
