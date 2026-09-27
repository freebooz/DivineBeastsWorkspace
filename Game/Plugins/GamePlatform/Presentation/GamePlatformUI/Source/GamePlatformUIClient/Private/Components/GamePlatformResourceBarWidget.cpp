#include "Components/GamePlatformResourceBarWidget.h"

#include "Components/ProgressBar.h"

void UGamePlatformResourceBarWidget::ApplyResourceState(
    const FGamePlatformUIResourceBarState& InState)
{
    const bool bEquivalent =
        ResourceState.ResourceId == InState.ResourceId &&
        ResourceState.bShowValueText == InState.bShowValueText &&
        FMath::IsNearlyEqual(
            ResourceState.CurrentValue,
            InState.CurrentValue,
            0.0001) &&
        FMath::IsNearlyEqual(
            ResourceState.MaximumValue,
            InState.MaximumValue,
            0.0001);

    if (bEquivalent)
    {
        return;
    }

    ResourceState = InState;

    if (IsValid(ProgressBar))
    {
        ProgressBar->SetPercent(
            static_cast<float>(ResourceState.GetNormalizedValue()));
    }

    BP_OnResourceStateChanged(ResourceState);
}
