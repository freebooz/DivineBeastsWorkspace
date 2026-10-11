// 项目客户端UI继承、命名控件及密码生命周期回归；由UE自动化调用，只消费真实蓝图和瞬态夹具，不连后端。
#if WITH_DEV_AUTOMATION_TESTS

#include "HUD/DivineBeastsHUDWidget.h"
#include "HUD/DivineBeastsOpenWorldHUD.h"
#include "HUD/DivineBeastsTrainingHUD.h"
#include "HUD/DivineBeastsTutorialHUD.h"
#include "HUD/DivineBeastsVillageHUD.h"
#include "Layers/DivineBeastsRootLayout.h"
#include "Misc/AutomationTest.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "Screens/Boot/DivineBeastsBootScreen.h"
#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"
#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"
#include "Screens/Login/DivineBeastsLoginScreen.h"
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "ViewModels/Boot/DivineBeastsBootViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"
#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/ScopeExit.h"

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
        TEXT("启动封面必须继承项目 Loading Screen"),
        UDivineBeastsBootScreen::StaticClass()->IsChildOf(
            UDivineBeastsLoadingScreen::StaticClass()));

    TestTrue(
        TEXT("登录页必须继承项目 Screen"),
        UDivineBeastsLoginScreen::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()));

    TestTrue(
        TEXT("持久角色选择/创建页必须继承项目 Screen"),
        UDivineBeastsCharacterSelectScreen::StaticClass()->IsChildOf(
            UDivineBeastsUIScreen::StaticClass()) &&
        UDivineBeastsCharacterCreateScreen::StaticClass()->IsChildOf(
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
        TEXT("公共非竞技四类HUD均必须继承项目HUD"),
        UDivineBeastsOpenWorldHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsVillageHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsTutorialHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()) &&
        UDivineBeastsTrainingHUD::StaticClass()->IsChildOf(UDivineBeastsHUDWidget::StaticClass()));

    TestTrue(
        TEXT("项目 RootLayout 必须继承平台 RootLayout"),
        UDivineBeastsRootLayout::StaticClass()->IsChildOf(
            UGamePlatformRootLayout::StaticClass()));

    TestTrue(
        TEXT("项目 ViewModel 基类必须继承平台 ViewModel"),
        UDivineBeastsViewModelBase::StaticClass()->IsChildOf(
            UGamePlatformViewModelBase::StaticClass()));

    TestTrue(
        TEXT("启动、登录、角色和加载 ViewModel 必须沿项目 ViewModel 继承链复用"),
        UDivineBeastsBootViewModel::StaticClass()->IsChildOf(
            UDivineBeastsLoadingViewModel::StaticClass()) &&
        UDivineBeastsLoginViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()) &&
        UDivineBeastsCharacterSelectViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()) &&
        UDivineBeastsCharacterCreateViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()) &&
        UDivineBeastsLoadingViewModel::StaticClass()->IsChildOf(
            UDivineBeastsUIViewModel::StaticClass()));

    return true;
}

/**
 * 验证登录Widget Blueprint必须提供的命名控件契约。
 * 这些BindWidget字段让Monolith生成资产在编译时暴露缺失控件，而不是运行时静默失效。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsLoginWidgetContractTest,
    "DivineBeasts.UI.Login.NamedWidgetContract",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsLoginWidgetContractTest::RunTest(const FString& Parameters)
{
    UClass* LoginClass = UDivineBeastsLoginScreen::StaticClass();
    for (const FName PropertyName : {
        FName(TEXT("AccountInput")),
        FName(TEXT("PasswordInput")),
        FName(TEXT("LoginButton")),
        FName(TEXT("ErrorText")),
        FName(TEXT("BusyIndicator")),
        FName(TEXT("MaintenanceText"))})
    {
        TestNotNull(
            *FString::Printf(
                TEXT("登录页面必须公开BindWidget字段：%s"),
                *PropertyName.ToString()),
            FindFProperty<FObjectProperty>(LoginClass, PropertyName));
    }
    return true;
}

// 真实保存的登录蓝图不连后端；只用测试口令验证按住揭示、松开/移出/离页遮蔽及清空，不记录口令值。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLoginPasswordRevealTest,
    "DivineBeasts.UI.Login.PasswordRevealLifecycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsLoginPasswordRevealTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsLoginScreen>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login.WBP_DBA_UI_Login_C"));
    if (!TestNotNull(TEXT("真实登录蓝图类"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsLoginScreen> Screen(NewObject<UDivineBeastsLoginScreen>(GetTransientPackage(), Class));
    Screen->Initialize();
    auto* Password = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("PasswordInput")));
    auto* Account = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AccountInput")));
    auto* Reveal = Cast<UButton>(Screen->GetWidgetFromName(TEXT("PasswordRevealButton")));
    if (!TestNotNull(TEXT("密码输入"), Password) || !TestNotNull(TEXT("账号输入"), Account) || !TestNotNull(TEXT("眼睛按钮"), Reveal)) return false;
    const auto PasswordFont = Password->GetWidgetStyle().TextStyle.Font.Size;
    const auto AccountFont = Account->GetWidgetStyle().TextStyle.Font.Size;
    for (auto* Input : {Password, Account})
    {
        auto* Slot = Cast<UCanvasPanelSlot>(Input->Slot);
        if (!TestNotNull(TEXT("输入Canvas槽"), Slot)) return false;
        TestEqual(TEXT("输入固定284×42"), Slot->GetSize(), FVector2D(284, 42));
    }
    Screen->ActivateWidget();
    ON_SCOPE_EXIT { Screen->DeactivateWidget(); };
    Password->SetText(FText::FromString(TEXT("fixture-only-secret")));
    TestTrue(TEXT("默认遮蔽"), Password->GetIsPassword());
    Reveal->OnPressed.Broadcast();
    TestFalse(TEXT("按住时显示当前输入"), Password->GetIsPassword());
    Reveal->OnReleased.Broadcast();
    TestTrue(TEXT("松开立即遮蔽"), Password->GetIsPassword());
    Reveal->OnPressed.Broadcast();
    Reveal->OnUnhovered.Broadcast();
    TestTrue(TEXT("移出按钮立即遮蔽"), Password->GetIsPassword());
    Reveal->OnPressed.Broadcast();
    Screen->DeactivateWidget();
    TestTrue(TEXT("离页遮蔽"), Password->GetIsPassword());
    TestTrue(TEXT("离页清空瞬时口令"), Password->GetText().IsEmpty());
    Reveal->OnPressed.Broadcast();
    TestTrue(TEXT("失活按钮无残留揭示订阅"), Password->GetIsPassword());
    TestEqual(TEXT("密码字体未缩放"), Password->GetWidgetStyle().TextStyle.Font.Size, PasswordFont);
    TestEqual(TEXT("账号字体未缩放"), Account->GetWidgetStyle().TextStyle.Font.Size, AccountFont);
    return true;
}

#endif
