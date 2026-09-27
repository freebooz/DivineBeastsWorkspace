#if WITH_DEV_AUTOMATION_TESTS

#include "HUD/DivineBeastsArenaHUD.h"
#include "HUD/DivineBeastsHUDWidget.h"
#include "HUD/DivineBeastsOpenWorldHUD.h"
#include "HUD/DivineBeastsTrainingHUD.h"
#include "HUD/DivineBeastsTutorialHUD.h"
#include "HUD/DivineBeastsVillageHUD.h"
#include "Layers/DivineBeastsRootLayout.h"
#include "Misc/AutomationTest.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "Screens/Login/DivineBeastsLoginScreen.h"
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"
#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"

/**
 * 验证神兽联盟项目 UI 基类和首批页面/HUD 均沿平台基础类单向继承。
 * 该测试不依赖真实 UMG 资产，可在美术资源尚未创建时提供架构回归。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIBaseClassHierarchyTest,
    "DivineBeasts.UI.Architecture.BaseClassHierarchy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIBaseClassHierarchyTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("项目 Screen 必须继承 GamePlatform Screen"),
        UDivineBeastsUIScreen::StaticClass()->IsChildOf(
            UGamePlatformUIScreen::StaticClass()));

    TestTrue(
        TEXT("登录页必须继承项目 Screen"),
        UDivineBeastsLoginScreen::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()));

    TestTrue(
        TEXT("加载页必须继承项目 Loading Screen"),
        UDivineBeastsLoadingTravelScreen::StaticClass()->IsChildOf(
            UDivineBeastsLoadingScreen::StaticClass()));

    TestTrue(
        TEXT("项目 HUD 必须继承平台 HUD"),
        UDivineBeastsHUDWidget::StaticClass()->IsChildOf(
            UGamePlatformHUDWidget::StaticClass()));

    TestTrue(
        TEXT("五类 HUD 均必须继承项目 HUD"),
        UDivineBeastsOpenWorldHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsVillageHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsTutorialHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsTrainingHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsArenaHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()));

    TestTrue(
        TEXT("项目 RootLayout 必须继承平台 RootLayout"),
        UDivineBeastsRootLayout::StaticClass()->IsChildOf(
            UGamePlatformRootLayout::StaticClass()));

    TestTrue(
        TEXT("项目 ViewModel 基类必须继承平台 ViewModel"),
        UDivineBeastsViewModelBase::StaticClass()->IsChildOf(
            UGamePlatformViewModelBase::StaticClass()));

    TestTrue(
        TEXT("登录和加载 ViewModel 必须继承项目通用 ViewModel"),
        UDivineBeastsLoginViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()) &&
        UDivineBeastsLoadingViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()));

    return true;
}

#endif
