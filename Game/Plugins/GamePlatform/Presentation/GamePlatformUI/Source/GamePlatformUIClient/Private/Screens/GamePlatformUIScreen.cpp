// 本文件属于GamePlatform平台层 GamePlatformUI，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Screens/GamePlatformUIScreen.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Input/GamePlatformUIInputPolicy.h"

void UGamePlatformUIScreen::InitializeScreen(
    FName InScreenId,
    UGamePlatformViewModelBase* InViewModel,
    FName InDesiredFocusWidgetName,
    EGamePlatformUIInputMode InInputMode,
    EGamePlatformUIPausePolicy InPausePolicy)
{
    ScreenId = InScreenId;

    // ViewModel 生命周期统一交给可激活基类管理，避免 Screen 与子类重复绑定事件。
    InitializeActivatableViewModel(InViewModel);

    DesiredFocusWidgetName = InDesiredFocusWidgetName;
    InputMode = InInputMode;
    PausePolicy = InPausePolicy;
}

void UGamePlatformUIScreen::CloseScreen()
{
    DeactivateWidget();
}

TOptional<FUIInputConfig> UGamePlatformUIScreen::GetDesiredInputConfig() const
{
    return UGamePlatformUIInputPolicy::BuildInputConfig(InputMode);
}

void UGamePlatformUIScreen::NativeDestruct()
{
    // 池中UObject可复用；Slate释放后交由Manager核对是否真的离开栈。
    PlatformReleased.Broadcast(this);
    Super::NativeDestruct();
}

void UGamePlatformUIScreen::NativeOnActivated()
{
    // ViewModel BeginPage、事件绑定和初始刷新由平台可激活基类统一完成。
    Super::NativeOnActivated();
}

void UGamePlatformUIScreen::NativeOnDeactivated()
{
    // 先解绑并结束旧页面代次，再开放关闭观察者重入；返回后不能结束观察者的新页面。
    Super::NativeOnDeactivated();
    if (!IsActivated()) PlatformDeactivated.Broadcast(this);
}

bool UGamePlatformUIScreen::NativeOnHandleBackAction()
{
    if (!bAllowBack)
    {
        return false;
    }

    DeactivateWidget();
    return true;
}

UWidget* UGamePlatformUIScreen::NativeGetDesiredFocusTarget() const
{
    if (!DesiredFocusWidgetName.IsNone() && WidgetTree)
    {
        if (UWidget* Desired = WidgetTree->FindWidget(DesiredFocusWidgetName))
        {
            return Desired;
        }
    }

    return Super::NativeGetDesiredFocusTarget();
}
