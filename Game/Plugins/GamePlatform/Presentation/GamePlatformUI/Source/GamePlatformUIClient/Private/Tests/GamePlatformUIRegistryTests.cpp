#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/LocalPlayer.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Routing/GamePlatformUIRouteDefinition.h"
#include "Screens/GamePlatformUIScreen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIRegistryTest,
    "GamePlatform.UI.Registry.DuplicateAndRouteCycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIRegistryTest::RunTest(const FString& Parameters)
{
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>();
    UGamePlatformUIManagerSubsystem* Manager =
        NewObject<UGamePlatformUIManagerSubsystem>(LocalPlayer);

    UGamePlatformUIScreenDefinition* Screen =
        NewObject<UGamePlatformUIScreenDefinition>();
    Screen->ScreenId = TEXT("Test.Screen");
    Screen->WidgetClass = TSoftClassPtr<UGamePlatformUIScreen>(
        FSoftObjectPath(TEXT("/Game/GamePlatformUI/Tests/WBP_TestScreen.WBP_TestScreen_C")));
    Screen->DefaultFocusWidgetName = TEXT("DefaultFocus");

    FText ValidationReason;
    TestTrue(TEXT("合法Screen Definition通过结构校验"), Screen->ValidateDefinition(ValidationReason));

    UGamePlatformUIScreenDefinition* AbstractScreen =
        NewObject<UGamePlatformUIScreenDefinition>();
    AbstractScreen->ScreenId = TEXT("Test.AbstractScreen");
    AbstractScreen->WidgetClass = UGamePlatformUIScreen::StaticClass();
    AbstractScreen->DefaultFocusWidgetName = TEXT("DefaultFocus");
    TestFalse(
        TEXT("已加载的抽象Screen类必须在注册前被拒绝"),
        AbstractScreen->ValidateDefinition(ValidationReason));
    TestTrue(TEXT("首次Screen注册成功"), Manager->RegisterScreenDefinition(Screen));
    TestFalse(TEXT("重复Screen拒绝"), Manager->RegisterScreenDefinition(Screen));

    UGamePlatformUIScreenDefinition* MissingFocus =
        NewObject<UGamePlatformUIScreenDefinition>();
    MissingFocus->ScreenId = TEXT("Test.MissingFocus");
    MissingFocus->WidgetClass = UGamePlatformUIScreen::StaticClass();
    TestFalse(TEXT("可交互页面缺少Focus必须拒绝"), MissingFocus->ValidateDefinition(ValidationReason));

    UGamePlatformUIScreenDefinition* InvalidLayer =
        NewObject<UGamePlatformUIScreenDefinition>();
    InvalidLayer->ScreenId = TEXT("Test.InvalidLayer");
    InvalidLayer->WidgetClass = UGamePlatformUIScreen::StaticClass();
    InvalidLayer->Layer = EGamePlatformUILayer::HUD;
    TestFalse(TEXT("HUD不能作为Activatable Screen Definition"), InvalidLayer->ValidateDefinition(ValidationReason));

    UGamePlatformUIScreenDefinition* UnsafePath =
        NewObject<UGamePlatformUIScreenDefinition>();
    UnsafePath->ScreenId = TEXT("Test.UnsafePath");
    UnsafePath->WidgetClass = TSoftClassPtr<UGamePlatformUIScreen>(
        FSoftObjectPath(TEXT("https://invalid.example/WBP_UI.WBP_UI_C")));
    UnsafePath->DefaultFocusWidgetName = TEXT("DefaultFocus");
    TestFalse(TEXT("外部WidgetClass路径必须拒绝"), UnsafePath->ValidateDefinition(ValidationReason));

    UGamePlatformUIRouteDefinition* UnknownTarget =
        NewObject<UGamePlatformUIRouteDefinition>();
    UnknownTarget->RouteId = TEXT("Route.UnknownTarget");
    UnknownTarget->ScreenId = TEXT("Missing.Screen");
    TestFalse(TEXT("Route目标Screen未注册时拒绝"), Manager->RegisterRouteDefinition(UnknownTarget));

    UGamePlatformUIRouteDefinition* RouteA =
        NewObject<UGamePlatformUIRouteDefinition>();
    RouteA->RouteId = TEXT("Route.A");
    RouteA->ScreenId = Screen->ScreenId;
    RouteA->FallbackRouteId = TEXT("Route.B");

    UGamePlatformUIRouteDefinition* RouteB =
        NewObject<UGamePlatformUIRouteDefinition>();
    RouteB->RouteId = TEXT("Route.B");
    RouteB->ScreenId = Screen->ScreenId;
    RouteB->FallbackRouteId = TEXT("Route.A");

    TestTrue(TEXT("未闭环Route A可注册"), Manager->RegisterRouteDefinition(RouteA));
    TestFalse(TEXT("形成循环的Route B被拒绝"), Manager->RegisterRouteDefinition(RouteB));

    UGamePlatformUIRouteDefinition* SelfBack =
        NewObject<UGamePlatformUIRouteDefinition>();
    SelfBack->RouteId = TEXT("Route.SelfBack");
    SelfBack->ScreenId = Screen->ScreenId;
    SelfBack->BackRouteId = SelfBack->RouteId;
    TestFalse(TEXT("BackRoute不能指向自身"), Manager->RegisterRouteDefinition(SelfBack));

    UGamePlatformUIRouteDefinition* RouteC =
        NewObject<UGamePlatformUIRouteDefinition>();
    RouteC->RouteId = TEXT("Route.C");
    RouteC->ScreenId = Screen->ScreenId;
    RouteC->BackRouteId = RouteA->RouteId;
    TestTrue(TEXT("显式BackRoute可注册"), Manager->RegisterRouteDefinition(RouteC));
    TestEqual(TEXT("BackRoute可查询"), Manager->GetBackRouteId(RouteC->RouteId), RouteA->RouteId);

    TestFalse(TEXT("存在Route引用时不能注销Screen"), Manager->UnregisterScreenDefinition(Screen->ScreenId));
    TestFalse(TEXT("存在BackRoute引用时不能注销Route A"), Manager->UnregisterRouteDefinition(RouteA->RouteId));
    TestTrue(TEXT("注销Route C"), Manager->UnregisterRouteDefinition(RouteC->RouteId));
    TestTrue(TEXT("注销Route A"), Manager->UnregisterRouteDefinition(RouteA->RouteId));
    TestTrue(TEXT("解除Route后可注销Screen"), Manager->UnregisterScreenDefinition(Screen->ScreenId));
    return true;
}

#endif
