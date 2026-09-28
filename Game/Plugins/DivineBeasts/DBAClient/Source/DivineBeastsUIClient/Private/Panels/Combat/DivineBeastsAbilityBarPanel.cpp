#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"

void UDivineBeastsAbilityBarPanel::ApplyAbilitySlots(
    const TArray<FGamePlatformUISlotState>& InSlots)
{
    bool bChanged = AbilitySlots.Num() != InSlots.Num();

    if (!bChanged)
    {
        for (int32 Index = 0; Index < InSlots.Num(); ++Index)
        {
            const FGamePlatformUISlotState& A = AbilitySlots[Index];
            const FGamePlatformUISlotState& B = InSlots[Index];

            if (A.SlotId != B.SlotId ||
                A.ContentId != B.ContentId ||
                A.Icon != B.Icon ||
                A.Count != B.Count ||
                !FMath::IsNearlyEqual(
                    A.OverlayProgress,
                    B.OverlayProgress,
                    0.0001f) ||
                A.bEnabled != B.bEnabled ||
                A.bPending != B.bPending)
            {
                bChanged = true;
                break;
            }
        }
    }

    if (!bChanged)
    {
        return;
    }

    AbilitySlots = InSlots;
    for (FGamePlatformUISlotState& AbilitySlotState : AbilitySlots)
    {
        AbilitySlotState.OverlayProgress =
            FMath::Clamp(
                AbilitySlotState.OverlayProgress,
                0.0f,
                1.0f);
    }

    BP_OnAbilitySlotsChanged();
}
