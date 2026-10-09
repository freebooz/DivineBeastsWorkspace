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
    // 先通知Manager页面暂时失活，再由父类解绑事件并结束 ViewModel 页面代次。
    PlatformDeactivated.Broadcast(this);
    Super::NativeOnDeactivated();
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
