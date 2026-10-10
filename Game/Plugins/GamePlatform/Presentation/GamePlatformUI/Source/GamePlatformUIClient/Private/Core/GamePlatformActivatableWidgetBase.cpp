// 本文件属于GamePlatform平台层 GamePlatformUI，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Core/GamePlatformActivatableWidgetBase.h"

#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "UObject/StrongObjectPtr.h"

void UGamePlatformActivatableWidgetBase::InitializeActivatableViewModel(
    UGamePlatformViewModelBase* InViewModel)
{
    if (ViewModel == InViewModel)
    {
        return;
    }

    const TStrongObjectPtr<UGamePlatformActivatableWidgetBase> KeepWidget(this);
    const TStrongObjectPtr<UGamePlatformViewModelBase> Previous(ViewModel);
    const TStrongObjectPtr<UGamePlatformViewModelBase> Incoming(InViewModel);
    const uint64 BindingGeneration = ++ViewModelBindingGeneration;
    const uint64 VisibleGeneration = ActivationGeneration;
    const int32 IncomingPageGeneration = Incoming.IsValid() ? Incoming->GetPageGeneration() : 0;
    // 先发布本次VM选择并绑定新状态事件；旧EndPage/新BeginPage任一钩子自闭都能由失活完整解绑。
    ViewModel = Incoming.Get();
    if (bPlatformEventsBound)
    {
        if (Previous.IsValid())
        {
            Previous->OnViewStateChanged.RemoveDynamic(
                this,
                &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
        }
        if (Incoming.IsValid()) Incoming->OnViewStateChanged.AddUniqueDynamic(
            this, &UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged);
        const auto IsSelectionCurrent = [this, BindingGeneration, VisibleGeneration, Selected = Incoming.Get()]()
        {
            return IsActivated() && bPlatformEventsBound && ViewModel == Selected &&
                ActivationGeneration == VisibleGeneration && ViewModelBindingGeneration == BindingGeneration;
        };
        if (Previous.IsValid() && Previous->IsPageActive()) Previous->EndPage();
        if (!IsSelectionCurrent() || (Incoming.IsValid() && Incoming->GetPageGeneration() != IncomingPageGeneration)) return;
        if (Incoming.IsValid()) Incoming->BeginPage();
        if (!IsSelectionCurrent() || (Incoming.IsValid() &&
            (!Incoming->IsPageActive() || Incoming->GetPageGeneration() != IncomingPageGeneration + 1))) return;
        RefreshInitialState();
        return;
    }
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
    const TStrongObjectPtr<UGamePlatformActivatableWidgetBase> KeepWidget(this);
    const uint64 VisibleGeneration = ++ActivationGeneration;
    Super::NativeOnActivated();

    if (bPlatformEventsBound || !IsActivated() || ActivationGeneration != VisibleGeneration)
    {
        return;
    }

    // Super激活事件可以同步自闭；先登记绑定资格，BeginPage广播内失活能完整解绑。
    bPlatformEventsBound = true;
    BindPlatformEvents();
    const TStrongObjectPtr<UGamePlatformViewModelBase> Selected(ViewModel);
    const uint64 BindingGeneration = ViewModelBindingGeneration;
    const int32 ExpectedPageGeneration = Selected.IsValid() ? Selected->GetPageGeneration() + 1 : 0;
    const auto IsActivationCurrent = [this, VisibleGeneration, BindingGeneration, SelectedVM = Selected.Get()]()
    {
        return IsActivated() && bPlatformEventsBound && ActivationGeneration == VisibleGeneration &&
            ViewModelBindingGeneration == BindingGeneration && ViewModel == SelectedVM;
    };
    if (Selected.IsValid())
    {
        Selected->BeginPage();
    }
    if (!IsActivationCurrent() || (Selected.IsValid() &&
        (!Selected->IsPageActive() || Selected->GetPageGeneration() != ExpectedPageGeneration))) return;
    BindUIEvents();
    if (!IsActivationCurrent() || (Selected.IsValid() && Selected->GetPageGeneration() != ExpectedPageGeneration)) return;
    RefreshInitialState();
}

void UGamePlatformActivatableWidgetBase::NativeOnDeactivated()
{
    const TStrongObjectPtr<UGamePlatformActivatableWidgetBase> KeepWidget(this);
    ++ActivationGeneration;
    const TStrongObjectPtr<UGamePlatformViewModelBase> KeepPreviousViewModel(ViewModel);
    UGamePlatformViewModelBase* PreviousViewModel = KeepPreviousViewModel.Get();
    const int32 PreviousPageGeneration = IsValid(PreviousViewModel) ? PreviousViewModel->GetPageGeneration() : 0;
    if (bPlatformEventsBound)
    {
        bPlatformEventsBound = false;
        UnbindPlatformEvents();
        UnbindUIEvents();
    }

    // 派生解绑钩子可能打开同一VM的新页面，只能结束捕获的旧代次。
    if (IsValid(PreviousViewModel) && PreviousViewModel->IsPageActive() &&
        PreviousViewModel->GetPageGeneration() == PreviousPageGeneration)
    {
        PreviousViewModel->EndPage();
    }

    if (!IsActivated()) Super::NativeOnDeactivated();
}

void UGamePlatformActivatableWidgetBase::HandleViewModelStateChanged(
    int32 Revision,
    int32 PageGeneration)
{
    const uint64 VisibleGeneration = ActivationGeneration;
    const uint64 BindingGeneration = ViewModelBindingGeneration;
    const TStrongObjectPtr<UGamePlatformViewModelBase> Selected(ViewModel);
    if (!IsActivated() || !bPlatformEventsBound || !Selected.IsValid() ||
        Selected->GetPageGeneration() != PageGeneration || !Selected->IsPageActive()) return;
    OnViewModelStateChanged(Revision, PageGeneration);
    if (IsActivated() && bPlatformEventsBound && ActivationGeneration == VisibleGeneration &&
        ViewModelBindingGeneration == BindingGeneration && ViewModel == Selected.Get() &&
        Selected->IsPageActive() && Selected->GetPageGeneration() == PageGeneration)
        BP_OnViewModelStateChanged(Revision, PageGeneration);
}

void UGamePlatformActivatableWidgetBase::HandleAdaptiveContextChanged(
    FGamePlatformUIAdaptiveContext Context)
{
    const uint64 VisibleGeneration = ActivationGeneration;
    if (!IsActivated() || !bPlatformEventsBound) return;
    OnAdaptiveContextChanged(Context);
    if (IsActivated() && bPlatformEventsBound && ActivationGeneration == VisibleGeneration)
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
