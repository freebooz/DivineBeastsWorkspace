#include "Components/GamePlatformTooltipWidget.h"

bool UGamePlatformTooltipWidget::ApplyTooltip(
    const FGamePlatformUITooltipState& InState)
{
    if (InState.ContentId.IsNone() || InState.Revision < 0 ||
        (State.ContentId == InState.ContentId &&
         InState.Revision <= State.Revision))
    {
        return false;
    }
    State = InState;
    BP_OnTooltipChanged(State);
    return true;
}

void UGamePlatformTooltipWidget::ClearTooltip()
{
    if (State.ContentId.IsNone())
    {
        return;
    }
    State = FGamePlatformUITooltipState();
    BP_OnTooltipChanged(State);
}
