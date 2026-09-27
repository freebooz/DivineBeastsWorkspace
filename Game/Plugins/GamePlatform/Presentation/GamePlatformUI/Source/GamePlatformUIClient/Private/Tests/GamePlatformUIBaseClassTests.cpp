#if WITH_DEV_AUTOMATION_TESTS

#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Components/GamePlatformComponentWidget.h"
#include "Core/GamePlatformActivatableWidgetBase.h"
#include "Core/GamePlatformWidgetBase.h"
#include "Layers/GamePlatformRootLayout.h"
#include "Misc/AutomationTest.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "Overlays/GamePlatformOverlayWidget.h"
#include "Panels/GamePlatformPanelWidget.h"
#include "Screens/GamePlatformLoadingScreen.h"
#include "Screens/GamePlatformModalScreen.h"
#include "Screens/GamePlatformUIScreen.h"

/**
 * 验证平台 UI 分类基础类保持既定单向继承关系。
 * 该测试不依赖真实 Widget Blueprint，因此可在没有美术资产时提供编译级架构回归。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIBaseClassHierarchyTest,
    "GamePlatform.UI.Architecture.BaseClassHierarchy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIBaseClassHierarchyTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("Screen 必须继承平台可激活基类"),
        UGamePlatformUIScreen::StaticClass()->IsChildOf(
            UGamePlatformActivatableWidgetBase::StaticClass()));

    TestTrue(
        TEXT("Panel 必须继承普通 UI 基类"),
        UGamePlatformPanelWidget::StaticClass()->IsChildOf(
            UGamePlatformWidgetBase::StaticClass()));

    TestTrue(
        TEXT("Overlay 必须继承普通 UI 基类"),
        UGamePlatformOverlayWidget::StaticClass()->IsChildOf(
            UGamePlatformWidgetBase::StaticClass()));

    TestTrue(
        TEXT("Notification 必须继承普通 UI 基类"),
        UGamePlatformNotificationWidget::StaticClass()->IsChildOf(
            UGamePlatformWidgetBase::StaticClass()));

    TestTrue(
        TEXT("Component 必须继承普通 UI 基类"),
        UGamePlatformComponentWidget::StaticClass()->IsChildOf(
            UGamePlatformWidgetBase::StaticClass()));

    TestTrue(
        TEXT("Loading 页面必须继承 Screen"),
        UGamePlatformLoadingScreen::StaticClass()->IsChildOf(
            UGamePlatformUIScreen::StaticClass()));

    TestTrue(
        TEXT("Modal 页面必须继承 Screen"),
        UGamePlatformModalScreen::StaticClass()->IsChildOf(
            UGamePlatformUIScreen::StaticClass()));

    TestTrue(
        TEXT("RootLayout 必须继承统一层栈"),
        UGamePlatformRootLayout::StaticClass()->IsChildOf(
            UGamePlatformUILayerStack::StaticClass()));

    return true;
}

#endif
