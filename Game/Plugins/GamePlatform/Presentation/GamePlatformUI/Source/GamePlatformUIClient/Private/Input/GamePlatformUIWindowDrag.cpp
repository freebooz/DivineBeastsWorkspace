#include "Input/GamePlatformUIWindowDrag.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"

namespace
{
bool Finite(const FVector2D& P) { return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y); }
// 窗口能容纳时整体留在视口；超宽时允许左右查看，超高时仍将标题留在顶部可操作范围。
double ClampAxis(double Delta, double WindowMin, double WindowMax, double ViewMin, double ViewMax, bool bVertical)
{
    const double Minimum = ViewMin - WindowMin;
    const double Maximum = ViewMax - WindowMax;
    if (Minimum <= Maximum) return FMath::Clamp(Delta, Minimum, Maximum);
    return bVertical ? Minimum : FMath::Clamp(Delta, Maximum, Minimum);
}
}

void UGamePlatformUIWindowDrag::Initialize(UUserWidget* InOwner, const FGamePlatformUIWindowDragSettings& InSettings)
{
    const bool bChanged = Owner.Get()!=InOwner || Settings.bEnabled!=InSettings.bEnabled ||
        Settings.BoundsWidgetName!=InSettings.BoundsWidgetName || Settings.MoveTargetWidgetNames!=InSettings.MoveTargetWidgetNames ||
        Settings.HeaderHeight!=InSettings.HeaderHeight || Settings.ViewportInset!=InSettings.ViewportInset;
    if (bChanged) Shutdown();
    Owner=InOwner; Settings=InSettings;
    if(bChanged || Targets.IsEmpty()) bPendingLayoutFit=Settings.bEnabled;
    if (Owner.IsValid() && Settings.bEnabled && !VisibilityHandle.IsValid())
        VisibilityHandle=Owner->OnNativeVisibilityChanged.AddUObject(this,&UGamePlatformUIWindowDrag::VisibilityChanged);
}

bool UGamePlatformUIWindowDrag::ResolveTargets()
{
    if (!Owner.IsValid() || !Owner->WidgetTree) return false;
    if (!Targets.IsEmpty())
        return Bounds.IsValid() && !Targets.ContainsByPredicate([](const FTarget& T){return !T.Widget.IsValid();});
    UWidget* BoundWidget=Owner->GetWidgetFromName(Settings.BoundsWidgetName);
    if (!BoundWidget || BoundWidget->GetTypedOuter<UWidgetTree>()!=Owner->WidgetTree) return false;
    const TArray<FName> Names=Settings.MoveTargetWidgetNames.IsEmpty() ? TArray<FName>{Settings.BoundsWidgetName} : Settings.MoveTargetWidgetNames;
    TArray<FTarget> Resolved;
    for (const FName Name:Names)
    {
        UWidget* Widget=Owner->GetWidgetFromName(Name);
        if (!Widget || Widget->GetTypedOuter<UWidgetTree>()!=Owner->WidgetTree) return false;
        for (const FTarget& Existing:Resolved)
        {
            if (Existing.Widget.Get()==Widget) return false;
            for (UWidget* Parent=Widget->GetParent();Parent;Parent=Parent->GetParent()) if(Parent==Existing.Widget.Get()) return false;
            for (UWidget* Parent=Existing.Widget->GetParent();Parent;Parent=Parent->GetParent()) if(Parent==Widget) return false;
        }
        FTarget Target; Target.Widget=Widget; Target.OriginalTranslation=Widget->GetRenderTransform().Translation;
        Resolved.Add(Target);
    }
    // 边界必须随移动组移动，否则无法保持鼠标与窗口的相对位置。
    bool bBoundsMoves=false;
    for(UWidget* Parent=BoundWidget;Parent;Parent=Parent->GetParent())
        bBoundsMoves |= Resolved.ContainsByPredicate([Parent](const FTarget& T){return T.Widget.Get()==Parent;});
    if (!bBoundsMoves) return false;
    Bounds=BoundWidget; Targets=MoveTemp(Resolved); return true;
}

FReply UGamePlatformUIWindowDrag::MouseDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (bDragging || !Settings.bEnabled || !Owner.IsValid() || !Owner->IsVisible() || !Owner->GetIsEnabled() ||
        Owner->GetVisibility()==ESlateVisibility::HitTestInvisible || Event.IsTouchEvent() ||
        Event.GetEffectingButton()!=EKeys::LeftMouseButton || !Finite(Event.GetScreenSpacePosition()) ||
        !FMath::IsFinite(Settings.HeaderHeight) || Settings.HeaderHeight<=0.f ||
        !FMath::IsFinite(Settings.ViewportInset) || Settings.ViewportInset<0.f || !ResolveTargets()) return FReply::Unhandled();
    const FGeometry& BoundsGeometry=Bounds->GetCachedGeometry();
    const FVector2D Point=BoundsGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    const FVector2D Size=BoundsGeometry.GetLocalSize();
    if (!Finite(Size) || Size.X<=0 || Size.Y<=0 || Point.X<0 || Point.X>Size.X || Point.Y<0 || Point.Y>FMath::Min(double(Settings.HeaderHeight),Size.Y)) return FReply::Unhandled();

    if(!ResolveViewport(Geometry,ViewportBounds)) return FReply::Unhandled();
    for (FTarget& Target:Targets)
    {
        Target.StartTranslation=Target.Widget->GetRenderTransform().Translation;
        Target.LayoutScale=Target.Widget->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale();
        if(!Finite(Target.StartTranslation) || !FMath::IsFinite(Target.LayoutScale) || Target.LayoutScale<=UE_SMALL_NUMBER) return FReply::Unhandled();
    }
    StartPointer=Event.GetScreenSpacePosition(); StartBounds=BoundsGeometry.GetRenderBoundingRect();
    UserIndex=Event.GetUserIndex(); PointerIndex=Event.GetPointerIndex(); bDragging=true;
    CapturedSlate=Owner->TakeWidget();
    if (FSlateApplication::IsInitialized())
        ApplicationHandle=FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&UGamePlatformUIWindowDrag::ApplicationActivationChanged);
    return FReply::Handled().CaptureMouse(CapturedSlate.Pin().ToSharedRef());
}

bool UGamePlatformUIWindowDrag::ResolveViewport(const FGeometry& Geometry, FSlateRect& OutBounds) const
{
    if(!Owner.IsValid()) return false;
    FGeometry Viewport=Geometry;
    if (Owner->GetOwningPlayer())
    {
        const FGeometry PlayerViewport=UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(Owner->GetOwningPlayer());
        if(PlayerViewport.GetLocalSize().X>0 && PlayerViewport.GetLocalSize().Y>0) Viewport=PlayerViewport;
    }
    const FVector2D ViewSize=Viewport.GetLocalSize();
    if(!Finite(ViewSize) || ViewSize.X<=0 || ViewSize.Y<=0) return false;
    OutBounds=Viewport.GetRenderBoundingRect();
    const float ViewScale=Viewport.GetAccumulatedLayoutTransform().GetScale();
    if(!FMath::IsFinite(ViewScale) || ViewScale<=UE_SMALL_NUMBER) return false;
    const float SafeScale=ViewScale/FMath::Max(AdaptiveContext.DPIScale,UE_SMALL_NUMBER);
    const float Inset=Settings.ViewportInset*ViewScale;
    OutBounds.Left+=Inset+AdaptiveContext.SafeZonePadding.Left*SafeScale;
    OutBounds.Top+=Inset+AdaptiveContext.SafeZonePadding.Top*SafeScale;
    OutBounds.Right-=Inset+AdaptiveContext.SafeZonePadding.Right*SafeScale;
    OutBounds.Bottom-=Inset+AdaptiveContext.SafeZonePadding.Bottom*SafeScale;
    return OutBounds.Right>OutBounds.Left && OutBounds.Bottom>OutBounds.Top;
}

void UGamePlatformUIWindowDrag::LayoutReady(const FGeometry& Geometry)
{
    // 由激活/构造或 Adaptive 通知置位；NativePaint 在子树布局完成后消费一次。
    // 不增加 Tick/计时器，也不逐帧读取业务或扫描窗口；未启用时直接退出。
    if(!bPendingLayoutFit) return;
    bPendingLayoutFit=false;
    FSlateRect View;
    if(!Settings.bEnabled || !FMath::IsFinite(Settings.ViewportInset) || Settings.ViewportInset<0 ||
        !ResolveTargets() || !ResolveViewport(Geometry,View)) return;
    const FGeometry& BoundGeometry=Bounds->GetCachedGeometry();
    if(BoundGeometry.GetLocalSize().X<=0 || BoundGeometry.GetLocalSize().Y<=0) return;
    const FSlateRect Rect=BoundGeometry.GetRenderBoundingRect();
    const FVector2D Delta(ClampAxis(0,Rect.Left,Rect.Right,View.Left,View.Right,false),
        ClampAxis(0,Rect.Top,Rect.Bottom,View.Top,View.Bottom,true));
    if(Delta.IsNearlyZero()) return;
    for(FTarget& Target:Targets)
    {
        Target.LayoutScale=Target.Widget->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale();
        if(!FMath::IsFinite(Target.LayoutScale) || Target.LayoutScale<=UE_SMALL_NUMBER) return;
    }
    for(const FTarget& Target:Targets)
        Target.Widget->SetRenderTranslation(Target.Widget->GetRenderTransform().Translation+Delta/Target.LayoutScale);
}

bool UGamePlatformUIWindowDrag::Matches(const FPointerEvent& Event) const
{ return bDragging && Event.GetUserIndex()==UserIndex && Event.GetPointerIndex()==PointerIndex; }

FReply UGamePlatformUIWindowDrag::MouseMove(const FGeometry&, const FPointerEvent& Event)
{
    if(!Matches(Event)) return FReply::Unhandled();
    if(!Owner.IsValid() || !Owner->IsVisible() || !Event.IsMouseButtonDown(EKeys::LeftMouseButton) || !ResolveTargets())
    { Cancel(); return FReply::Handled(); }
    if(!Finite(Event.GetScreenSpacePosition())) return FReply::Handled();
    FVector2D Delta=FVector2D(Event.GetScreenSpacePosition())-StartPointer;
    Delta.X=ClampAxis(Delta.X,StartBounds.Left,StartBounds.Right,ViewportBounds.Left,ViewportBounds.Right,false);
    Delta.Y=ClampAxis(Delta.Y,StartBounds.Top,StartBounds.Bottom,ViewportBounds.Top,ViewportBounds.Bottom,true);
    for(const FTarget& Target:Targets)
        if(Target.Widget.IsValid()) Target.Widget->SetRenderTranslation(Target.StartTranslation+Delta/Target.LayoutScale);
    return FReply::Handled();
}

FReply UGamePlatformUIWindowDrag::MouseUp(const FPointerEvent& Event)
{
    if(!Matches(Event) || Event.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
    Cancel(); return FReply::Handled();
}

void UGamePlatformUIWindowDrag::CaptureLost(const FCaptureLostEvent& Event)
{ if(bDragging && uint32(Event.UserIndex)==UserIndex && uint32(Event.PointerIndex)==PointerIndex) Cancel(); }

void UGamePlatformUIWindowDrag::Cancel()
{
    const bool bWasDragging=bDragging; bDragging=false;
    if(FSlateApplication::IsInitialized())
    {
        FSlateApplication& App=FSlateApplication::Get();
        App.OnApplicationActivationStateChanged().Remove(ApplicationHandle);
        // 先结束本地状态，再释放精确用户/指针，避免捕获丢失同步重入及清除另一窗口的捕获。
        const TSharedPtr<FSlateUser> User=App.GetUser(UserIndex);
        if(bWasDragging && User && User->DoesWidgetHaveCapture(CapturedSlate.Pin(),PointerIndex)) User->ReleaseCapture(PointerIndex);
    }
    ApplicationHandle.Reset(); CapturedSlate.Reset();
}

void UGamePlatformUIWindowDrag::ResetPosition()
{
    Cancel();
    for(const FTarget& Target:Targets) if(Target.Widget.IsValid()) Target.Widget->SetRenderTranslation(Target.OriginalTranslation);
}

void UGamePlatformUIWindowDrag::Shutdown()
{
    ResetPosition();
    if(Owner.IsValid()) Owner->OnNativeVisibilityChanged.Remove(VisibilityHandle);
    VisibilityHandle.Reset(); Targets.Reset(); Bounds.Reset(); Owner.Reset(); bHasAdaptiveContext=false; bPendingLayoutFit=false;
}

void UGamePlatformUIWindowDrag::Adapt(const FGamePlatformUIAdaptiveContext& Context)
{
    if(bHasAdaptiveContext && (!AdaptiveContext.ViewportSize.Equals(Context.ViewportSize) ||
        !FMath::IsNearlyEqual(AdaptiveContext.DPIScale,Context.DPIScale) || AdaptiveContext.SafeZonePadding!=Context.SafeZonePadding))
    { ResetPosition(); bPendingLayoutFit=Settings.bEnabled; }
    AdaptiveContext=Context; bHasAdaptiveContext=true;
}

void UGamePlatformUIWindowDrag::VisibilityChanged(ESlateVisibility Visibility)
{ if(Visibility==ESlateVisibility::Collapsed || Visibility==ESlateVisibility::Hidden || Visibility==ESlateVisibility::HitTestInvisible) Cancel(); }
void UGamePlatformUIWindowDrag::ApplicationActivationChanged(bool bActive) { if(!bActive) Cancel(); }
void UGamePlatformUIWindowDrag::BeginDestroy() { Shutdown(); Super::BeginDestroy(); }
