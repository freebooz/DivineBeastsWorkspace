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
