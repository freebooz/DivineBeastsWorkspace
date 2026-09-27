#include "Components/GamePlatformSlotWidget.h"

void UGamePlatformSlotWidget::ApplySlotState(
    const FGamePlatformUISlotState& InState)
{
    const float ClampedOverlay =
        FMath::Clamp(InState.OverlayProgress, 0.0f, 1.0f);

    if (SlotState.SlotId == InState.SlotId &&
        SlotState.ContentId == InState.ContentId &&
        SlotState.Icon == InState.Icon &&
        SlotState.Count == InState.Count &&
        FMath::IsNearlyEqual(
            SlotState.OverlayProgress,
            ClampedOverlay,
            0.0001f) &&
        SlotState.bEnabled == InState.bEnabled &&
        SlotState.bPending == InState.bPending)
    {
        return;
    }

    SlotState = InState;
    SlotState.OverlayProgress = ClampedOverlay;
    BP_OnSlotStateChanged(SlotState);
}
