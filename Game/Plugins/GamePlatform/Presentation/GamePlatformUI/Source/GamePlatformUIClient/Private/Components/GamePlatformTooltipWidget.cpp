#include "Components/GamePlatformTooltipWidget.h"
#include "ViewModels/GamePlatformUISnapshotScope.h"

bool UGamePlatformTooltipWidget::ApplyTooltip(
    const FGamePlatformUITooltipState& InState)
{
    if (!FGamePlatformUISnapshotScope::CanAcceptNamedRevision(
            State.ContentId, State.Revision, InState.ContentId, InState.Revision))
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
