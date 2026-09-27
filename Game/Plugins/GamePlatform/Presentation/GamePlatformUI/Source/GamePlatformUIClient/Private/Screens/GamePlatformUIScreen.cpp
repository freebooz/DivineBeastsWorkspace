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
    ViewModel = InViewModel;
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

void UGamePlatformUIScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (IsValid(ViewModel))
    {
        ViewModel->BeginPage();
    }
}

void UGamePlatformUIScreen::NativeOnDeactivated()
{
    if (IsValid(ViewModel))
    {
        ViewModel->EndPage();
    }
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
