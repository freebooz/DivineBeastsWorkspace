#include "Core/GamePlatformWidgetBase.h"

#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ViewModels/GamePlatformViewModelBase.h"

void UGamePlatformWidgetBase::InitializeWidgetViewModel(
    UGamePlatformViewModelBase* InViewModel)
{
    if (ViewModel == InViewModel)
    {
        return;
    }

    // 控件已处于显示生命周期时，只替换 ViewModel 委托，不重复绑定 Adaptive 事件。
    if (bPlatformEventsBound && IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.RemoveDynamic(
            this,
            &UGamePlatformWidgetBase::HandleViewModelStateChanged);
    }

    ViewModel = InViewModel;

    if (bPlatformEventsBound && IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.AddUniqueDynamic(
            this,
            &UGamePlatformWidgetBase::HandleViewModelStateChanged);
        RefreshInitialState();
    }
}

FGamePlatformUIAdaptiveContext UGamePlatformWidgetBase::GetAdaptiveContext() const
{
    return IsValid(AdaptiveSubsystem)
        ? AdaptiveSubsystem->GetAdaptiveContext()
        : FGamePlatformUIAdaptiveContext();
}

void UGamePlatformWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    if (bPlatformEventsBound)
    {
        return;
    }

    BindPlatformEvents();
    BindUIEvents();
    bPlatformEventsBound = true;

    // 初始刷新只执行一次，不引入持续 Tick 或轮询。
    RefreshInitialState();
}

void UGamePlatformWidgetBase::NativeDestruct()
{
    if (bPlatformEventsBound)
    {
        UnbindUIEvents();
        UnbindPlatformEvents();
        bPlatformEventsBound = false;
    }

    Super::NativeDestruct();
}

void UGamePlatformWidgetBase::HandleViewModelStateChanged(
    int32 Revision,
    int32 PageGeneration)
{
    OnViewModelStateChanged(Revision, PageGeneration);
    BP_OnViewModelStateChanged(Revision, PageGeneration);
}

void UGamePlatformWidgetBase::HandleAdaptiveContextChanged(
    FGamePlatformUIAdaptiveContext Context)
{
    OnAdaptiveContextChanged(Context);
    BP_OnAdaptiveContextChanged(Context);
}

void UGamePlatformWidgetBase::BindPlatformEvents()
{
    if (IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.AddUniqueDynamic(
            this,
            &UGamePlatformWidgetBase::HandleViewModelStateChanged);
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
            &UGamePlatformWidgetBase::HandleAdaptiveContextChanged);
    }
}

void UGamePlatformWidgetBase::UnbindPlatformEvents()
{
    if (IsValid(ViewModel))
    {
        ViewModel->OnViewStateChanged.RemoveDynamic(
            this,
            &UGamePlatformWidgetBase::HandleViewModelStateChanged);
    }

    if (IsValid(AdaptiveSubsystem))
    {
        AdaptiveSubsystem->OnAdaptiveContextChanged.RemoveDynamic(
            this,
            &UGamePlatformWidgetBase::HandleAdaptiveContextChanged);
    }

    AdaptiveSubsystem = nullptr;
}
