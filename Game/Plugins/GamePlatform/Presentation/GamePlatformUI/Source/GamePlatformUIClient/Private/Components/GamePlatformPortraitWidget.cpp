#include "Components/GamePlatformPortraitWidget.h"

void UGamePlatformPortraitWidget::ApplyPortraitState(
    const FGamePlatformUIPortraitState& InState)
{
    if (PortraitState.DisplayId == InState.DisplayId &&
        PortraitState.DisplayName.EqualTo(InState.DisplayName) &&
        PortraitState.Level == InState.Level &&
        PortraitState.PortraitTexture == InState.PortraitTexture &&
        PortraitState.StatusId == InState.StatusId &&
        PortraitState.bHighlighted == InState.bHighlighted)
    {
        return;
    }

    PortraitState = InState;
    BP_OnPortraitStateChanged(PortraitState);
}
