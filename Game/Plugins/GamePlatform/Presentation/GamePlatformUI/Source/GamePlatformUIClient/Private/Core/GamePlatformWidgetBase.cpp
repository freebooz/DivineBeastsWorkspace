#include "Core/GamePlatformWidgetBase.h"

#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "Styling/GamePlatformUIThemeBinding.h"
#include "Input/GamePlatformUIWindowDrag.h"

int32 UGamePlatformWidgetBase::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 Result=Super::NativePaint(Args,Geometry,CullingRect,Elements,Layer,Style,bParentEnabled);
    if(bPlatformEventsBound && WindowDragState) WindowDragState->LayoutReady(Geometry);
    return Result;
}

FReply UGamePlatformWidgetBase::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if(bPlatformEventsBound && WindowDrag.bEnabled && WindowDragState)
    { const FReply Reply=WindowDragState->MouseDown(Geometry,Event); if(Reply.IsEventHandled()) return Reply; }
    return Super::NativeOnMouseButtonDown(Geometry,Event);
}
FReply UGamePlatformWidgetBase::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if(WindowDragState) { const FReply Reply=WindowDragState->MouseMove(Geometry,Event); if(Reply.IsEventHandled()) return Reply; }
    return Super::NativeOnMouseMove(Geometry,Event);
}
FReply UGamePlatformWidgetBase::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if(WindowDragState) { const FReply Reply=WindowDragState->MouseUp(Event); if(Reply.IsEventHandled()) return Reply; }
    return Super::NativeOnMouseButtonUp(Geometry,Event);
}
void UGamePlatformWidgetBase::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{ if(WindowDragState) WindowDragState->CaptureLost(Event); Super::NativeOnMouseCaptureLost(Event); }
void UGamePlatformWidgetBase::NativeOnFocusLost(const FFocusEvent& Event)
{ if(WindowDragState) WindowDragState->Cancel(); Super::NativeOnFocusLost(Event); }

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

    if (bPlatformEventsBound)
    {
        if (IsValid(ViewModel)) ViewModel->OnViewStateChanged.AddUniqueDynamic(
            this, &UGamePlatformWidgetBase::HandleViewModelStateChanged);
        // 清空模型同样是状态变化；派生必须撤旧来源并显示未知，而非保留上个世界/账号的文本。
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
    // 样式应用可能调用CommonUI蓝图回调，必须放在事件登记完成之后；销毁可立即解绑。
    if (bPlatformEventsBound && !ThemeBindings.IsEmpty())
    {
        if (!ThemeBinding) ThemeBinding = NewObject<UGamePlatformUIThemeBinding>(this);
        ThemeBinding->Initialize(this, ThemeBindings);
    }
    if(bPlatformEventsBound && WindowDrag.bEnabled)
    {
        if(!WindowDragState) WindowDragState=NewObject<UGamePlatformUIWindowDrag>(this);
        WindowDragState->Initialize(this,WindowDrag);
        WindowDragState->Adapt(GetAdaptiveContext());
    }
}

void UGamePlatformWidgetBase::NativeDestruct()
{
    if(WindowDragState) WindowDragState->Shutdown();
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
    if(WindowDragState) WindowDragState->Adapt(Context);
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
    if (ThemeBinding) ThemeBinding->Unbind();
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
