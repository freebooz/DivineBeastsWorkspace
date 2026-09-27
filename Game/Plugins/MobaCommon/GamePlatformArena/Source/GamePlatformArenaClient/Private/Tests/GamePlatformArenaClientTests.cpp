#if WITH_DEV_AUTOMATION_TESTS

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

#endif
