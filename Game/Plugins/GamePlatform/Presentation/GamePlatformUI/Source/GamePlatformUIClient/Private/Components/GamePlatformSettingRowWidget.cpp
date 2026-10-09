#include "Components/GamePlatformSettingRowWidget.h"

bool UGamePlatformSettingRowWidget::ApplySettingRow(
    const FGamePlatformUISettingRowState& InState)
{
    if (InState.SettingId.IsNone() || InState.Revision < 0 ||
        (State.SettingId == InState.SettingId &&
         InState.Revision <= State.Revision))
    {
        return false;
    }
    State = InState;
    BP_OnSettingRowChanged(State);
    return true;
}
