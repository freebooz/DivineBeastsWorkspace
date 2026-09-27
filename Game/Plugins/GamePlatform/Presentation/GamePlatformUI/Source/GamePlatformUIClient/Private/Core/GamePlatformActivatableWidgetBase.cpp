#include "Core/GamePlatformActivatableWidgetBase.h"

#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ViewModels/GamePlatformViewModelBase.h"

void UGamePlatformActivatableWidgetBase::InitializeActivatableViewModel(
    UGamePlatformViewModelBase* InViewModel)
{
    if (ViewModel == InViewModel)
    {
        return;
    }

    // 激活页面更换 ViewModel 时，必须先关闭旧页面代次，避免旧异步回调继续有效。
    if (bPlatformEventsBound)
    {
        if (IsValid(ViewModel))
        {
            ViewModel->OnViewStateChanged.RemoveDynamic(
                this,
                &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
            ViewModel->EndPage();
        }

        ViewModel = InViewModel;

        if (IsValid(ViewModel))
        {
            ViewModel->BeginPage();
            ViewModel->OnViewStateChanged.AddUniqueDynamic(
                this,
                &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
        }

        RefreshInitialState();
        return;
    }

    ViewModel = InViewModel;
}

FGamePlatformUIAdaptiveContext
UGamePlatformActivatableWidgetBase::GetAdaptiveContext() const
{
    return IsValid(AdaptiveSubsystem)
        ? AdaptiveSubsystem->GetAdaptiveContext()
        : FGamePlatformUIAdaptiveContext();
}

void UGamePlatformActivatableWidgetBase::NativeOnActivated()
{
    Super::NativeOnActivated();

    if (bPlatformEventsBound)
    {
        return;
    }

    if (IsValid(ViewModel))
    {
        ViewModel->BeginPage();
    }

    BindPlatformEvents();
    BindUIEvents();
    bPlatformEventsBound = true;
    RefreshInitialState();
}

void UGamePlatformActivatableWidgetBase::NativeOnDeactivated()
{
    if (bPlatformEventsBound)
    {
        UnbindUIEvents();
        UnbindPlatformEvents();
        bPlatformEventsBound = false;
    }

    if (IsValid(ViewModel))
    {
        ViewModel->EndPage();
    }

    Super::NativeOnDeactivated();
}

void UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged(
    int32 Revision,
    int32 PageGeneration)
{
    OnViewModelStateChanged(Revision, PageGeneration);
    BP_OnViewModelStateChanged(Revision, PageGeneration);
}

void UGamePlatformActivatableWidgetBase::HandleAdaptiveContextChanged(
    FGamePlatformUIAdaptiveContext Context)
{
    OnAdaptiveContextChanged(Context);
    BP_OnAdaptiveContextChanged(Context);
}

void UGamePlatformActivatableWidgetBase::BindPlatformEvents()
{
    if (IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.AddUniqueDynamic(
            this,
            &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
    }

    if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
    {
        AdaptiveSubsystem =
            LocalPlayer->GetSubsystem<UGamePlatformUIAdaptiveSubsystem>();
    }

    if (IsValid(AdaptiveSubsystem))
    {
        AdaptiveSubsystem->OnAdaptiveContextChanged.AddUniqueDynamic(
            this,
            &UGamePlatformActivatableWidgetBase::HandleAdaptiveContextChanged);
    }
}

void UGamePlatformActivatableWidgetBase::UnbindPlatformEvents()
{
    if (IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.RemoveDynamic(
            this,
            &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
    }

    if (IsValid(AdaptiveSubsystem))
    {
        AdaptiveSubsystem->OnAdaptiveContextChanged.RemoveDynamic(
            this,
            &UGamePlatformActivatableWidgetBase::HandleAdaptiveContextChanged);
    }

    AdaptiveSubsystem = nullptr;
}
