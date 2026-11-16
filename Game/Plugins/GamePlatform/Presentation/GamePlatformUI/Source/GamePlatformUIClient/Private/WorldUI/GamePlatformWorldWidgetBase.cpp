#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SWidget.h"

void UGamePlatformWorldWidgetBase::ApplyWorldUIRequest(
    const FGamePlatformWorldUIRequest& InRequest)
{
    if (!bNeedsImmediateLayout)
    {
        const auto& Previous = WorldUIRequest;
        bNeedsImmediateLayout = Previous.RequestId != InRequest.RequestId ||
            Previous.SurfaceKey != InRequest.SurfaceKey || Previous.StyleId != InRequest.StyleId ||
            !Previous.PrimaryText.IdenticalTo(InRequest.PrimaryText, ETextIdenticalModeFlags::DeepCompare) ||
            !Previous.SecondaryText.IdenticalTo(InRequest.SecondaryText, ETextIdenticalModeFlags::DeepCompare) ||
            !Previous.SelectionText.IdenticalTo(InRequest.SelectionText, ETextIdenticalModeFlags::DeepCompare) ||
            Previous.bSelected != InRequest.bSelected || Previous.bCompactSelection != InRequest.bCompactSelection ||
            (Previous.Resource.MaximumValue > 0.) != (InRequest.Resource.MaximumValue > 0.) ||
            // 自定义蓝图可以根据完整内容调整布局，仍保留请求应用后的即时测量。
            GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(ThisClass, BP_OnWorldUIRequestApplied));
    }
    WorldUIRequest = InRequest;
    BP_OnWorldUIRequestApplied(WorldUIRequest);
}

void UGamePlatformWorldWidgetBase::SetProjectedState(
    FVector2D InScreenPosition,
    bool bInVisible,
    float InDistanceCentimeters)
{
    const uint64 Revision = ++ProjectionRevision;
    const bool bBecomesVisible = bInVisible && !IsVisible();
    // 集中投影得到视口像素，Overlay子控件的渲染位移使用DPI逻辑坐标。
    // 顶部居中锚在来源位置；不在每个名称板中Tick读取相机。
    SetVisibility(bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Revision != ProjectionRevision) return;

    const auto SlateWidget = GetCachedWidget();
    if (bInVisible && SlateWidget && (bBecomesVisible || bNeedsImmediateLayout || MeasuredSlate.Pin() != SlateWidget))
    {
        // 先解除Collapsed再测量。没有位置或镜头变化时也能在本次事件中完成正确居中。
        bNeedsImmediateLayout = false;
        MeasuredSlate = SlateWidget;
        ForceLayoutPrepass();
        if (Revision != ProjectionRevision) return;
    }
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const FVector2D Size = SlateWidget ? FVector2D(SlateWidget->GetDesiredSize()) : FVector2D::ZeroVector;
    SetRenderTranslation(InScreenPosition / FMath::Max(Scale, 0.001f) - FVector2D(Size.X * 0.5, Size.Y));
    BP_OnWorldUIProjected(
        InScreenPosition,
        bInVisible,
        InDistanceCentimeters);
}

void UGamePlatformWorldWidgetBase::ResetWorldUIState()
{
    ++ProjectionRevision;
    BP_OnWorldUIRecycled();
    WorldUIRequest = FGamePlatformWorldUIRequest();
    bNeedsImmediateLayout = true;
    MeasuredSlate.Reset();
    SetVisibility(ESlateVisibility::Collapsed);
}
