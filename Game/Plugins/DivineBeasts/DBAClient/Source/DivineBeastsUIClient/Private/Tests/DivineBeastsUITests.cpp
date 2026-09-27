#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Localization/DivineBeastsUILocalization.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIScreenInventoryTest,
    "DivineBeasts.UI.ScreenInventory",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIScreenInventoryTest::RunTest(const FString&)
{
    const TArray<FDivineBeastsUISurfaceDescriptor>& Surfaces =
        FDivineBeastsUIScreenCatalog::GetSurfaces();

    TestEqual(TEXT("一期UI表面数量"), Surfaces.Num(), 19);

    TSet<FName> Unique;
    for (const FDivineBeastsUISurfaceDescriptor& Surface : Surfaces)
    {
        TestTrue(TEXT("SurfaceId有效"), !Surface.SurfaceId.IsNone());
        TestFalse(TEXT("SurfaceId唯一"), Unique.Contains(Surface.SurfaceId));
        Unique.Add(Surface.SurfaceId);
    }

    for (const FName Required : {
        FName(TEXT("UI.Screen.Login")),
        FName(TEXT("UI.Screen.CharacterRoster")),
        FName(TEXT("UI.Screen.CharacterCreate")),
        FName(TEXT("UI.Screen.CharacterSelect")),
        FName(TEXT("UI.Screen.LoadingTravel")),
        FName(TEXT("UI.Screen.Matchmaking")),
        FName(TEXT("UI.Screen.MatchFoundReady")),
        FName(TEXT("UI.Screen.ArenaHeroSelection")),
        FName(TEXT("UI.Screen.Scoreboard")),
        FName(TEXT("UI.Screen.PostMatchResult")),
        FName(TEXT("UI.HUD.OpenWorld")),
        FName(TEXT("UI.HUD.VillageMain")),
        FName(TEXT("UI.HUD.TutorialGuidance")),
        FName(TEXT("UI.HUD.TrainingControls")),
        FName(TEXT("UI.HUD.Arena")),
        FName(TEXT("UI.Notification.Toast"))
    })
    {
        TestNotNull(
            *FString::Printf(TEXT("Required surface %s"), *Required.ToString()),
            FDivineBeastsUIScreenCatalog::Find(Required));
    }

    TestNull(
        TEXT("一期未批准Settings页面不应被擅自创建"),
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.Settings")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUICommandContractTest,
    "DivineBeasts.UI.CommandContract",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUICommandContractTest::RunTest(const FString&)
{
    FDivineBeastsUICommand Login;
    Login.RequestId = FGuid::NewGuid();
    Login.Type = EDivineBeastsUICommandType::Login;

    FString Error;
    TestFalse(TEXT("空登录命令拒绝"), Login.IsValid(Error));

    Login.LoginName = TEXT("test-user");
    Login.Password = TEXT("transient-only");
    Error.Reset();
    TestTrue(TEXT("瞬时登录参数有效"), Login.IsValid(Error));

    FDivineBeastsUICommand Create;
    Create.RequestId = FGuid::NewGuid();
    Create.Type = EDivineBeastsUICommandType::CreateCharacter;
    Create.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Create.CharacterName = TEXT("TestCharacter");
    Error.Reset();
    TestTrue(TEXT("角色创建意图有效"), Create.IsValid(Error));

    FDivineBeastsUICommand Select;
    Select.RequestId = FGuid::NewGuid();
    Select.Type =
        EDivineBeastsUICommandType::SelectPersistentCharacter;
    Error.Reset();
    TestFalse(TEXT("持久角色选择必须携带CharacterId"), Select.IsValid(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIRoutingPolicyTest,
    "DivineBeasts.UI.RoutingPolicy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIRoutingPolicyTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    State.CurrentStep = TEXT("DBA.Flow.Authentication");
    TestEqual(
        TEXT("认证页面路由到Login"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.Login")));

    State.PageState = EDivineBeastsUIPageState::Error;
    TestEqual(
        TEXT("错误优先进入重连/错误页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.ErrorReconnect")));

    State.PageState = EDivineBeastsUIPageState::Ready;
    State.Loading.bIsLoading = true;
    TestEqual(
        TEXT("Loading优先进入真实加载页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.LoadingTravel")));

    State.Loading.bIsLoading = false;
    State.Arena.ResultCommitState =
        EDivineBeastsUIResultCommitState::Committed;
    TestEqual(
        TEXT("Committed赛后结果进入PostMatch"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.PostMatchResult")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUILocalizationTest,
    "DivineBeasts.UI.Localization",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUILocalizationTest::RunTest(const FString&)
{
    TestFalse(
        TEXT("已知错误必须映射本地化文本"),
        FDivineBeastsUILocalization::ErrorCodeToText(
            TEXT("InvalidCredentials")).IsEmpty());
    TestFalse(
        TEXT("未知错误也必须安全降级"),
        FDivineBeastsUILocalization::ErrorCodeToText(
            TEXT("Internal.Technical.Error")).IsEmpty());
    return true;
}

#endif
