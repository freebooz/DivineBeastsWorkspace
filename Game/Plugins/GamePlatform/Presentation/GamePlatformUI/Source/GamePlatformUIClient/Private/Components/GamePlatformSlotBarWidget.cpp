#include "Components/GamePlatformSlotBarWidget.h"

bool UGamePlatformSlotBarWidget::ApplySlotBarState(
    const FGamePlatformUISlotBarState& NewState)
{
    constexpr int32 MaxSlots = 32;
    if (NewState.BarId.IsNone() || NewState.Revision < 0 ||
        NewState.Slots.Num() > MaxSlots ||
        (State.BarId == NewState.BarId && NewState.Revision <= State.Revision))
    {
        return false;
    }
    TSet<FName> UsedIds;
    UsedIds.Reserve(NewState.Slots.Num());
    for (const FGamePlatformUISlotState& SlotEntry : NewState.Slots)
    {
        if (SlotEntry.SlotId.IsNone() || UsedIds.Contains(SlotEntry.SlotId) ||
            !FMath::IsFinite(SlotEntry.OverlayProgress))
        {
            return false;
        }
        UsedIds.Add(SlotEntry.SlotId);
    }
    State = NewState;
    for (FGamePlatformUISlotState& SlotEntry : State.Slots)
    {
        SlotEntry.OverlayProgress = FMath::Clamp(SlotEntry.OverlayProgress, 0.0f, 1.0f);
    }
    BP_OnSlotBarChanged(State);
    return true;
}
