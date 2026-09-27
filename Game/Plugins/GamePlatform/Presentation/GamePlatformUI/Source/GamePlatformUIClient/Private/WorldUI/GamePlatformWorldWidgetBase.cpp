#include "WorldUI/GamePlatformWorldWidgetBase.h"

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
