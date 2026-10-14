#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UGamePlatformWorldWidgetBase::ApplyWorldUIRequest(
    const FGamePlatformWorldUIRequest& InRequest)
{
    WorldUIRequest = InRequest;
    BP_OnWorldUIRequestApplied(WorldUIRequest);
}

void UGamePlatformWorldWidgetBase::SetProjectedState(
    FVector2D InScreenPosition,
    bool bInVisible,
    float InDistanceCentimeters)
{
    // 集中投影得到视口像素，Overlay子控件的渲染位移使用DPI逻辑坐标。
    // 顶部居中锚在来源位置；不在每个名称板中Tick读取相机。
    SetVisibility(bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    // 首次注册及隐藏后重显尚无有效DesiredSize。事件链不能等待下一次相机移动补排版，
    // 先解除Collapsed并做局部Slate预排版，再计算头顶居中偏移，静止镜头也能首帧正确。
    if (bInVisible) ForceLayoutPrepass();
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const FVector2D Size = GetDesiredSize();
    SetRenderTranslation(InScreenPosition / FMath::Max(Scale, 0.001f) - FVector2D(Size.X * 0.5, Size.Y));
    SetVisibility(
        bInVisible
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    BP_OnWorldUIProjected(
        InScreenPosition,
        bInVisible,
        InDistanceCentimeters);
}

void UGamePlatformWorldWidgetBase::ResetWorldUIState()
{
    BP_OnWorldUIRecycled();
    WorldUIRequest = FGamePlatformWorldUIRequest();
    SetVisibility(ESlateVisibility::Collapsed);
}
