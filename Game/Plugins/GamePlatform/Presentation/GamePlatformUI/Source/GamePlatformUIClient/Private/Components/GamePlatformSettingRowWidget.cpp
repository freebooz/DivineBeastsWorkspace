#include "Components/GamePlatformSettingRowWidget.h"
#include "ViewModels/GamePlatformUISnapshotScope.h"

bool UGamePlatformSettingRowWidget::ApplySettingRow(
    const FGamePlatformUISettingRowState& InState)
{
    if (!FGamePlatformUISnapshotScope::CanAcceptNamedRevision(
            State.SettingId, State.Revision, InState.SettingId, InState.Revision))
    {
        return false;
    }
    State = InState;
    BP_OnSettingRowChanged(State);
    return true;
}
