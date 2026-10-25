#include "ViewModels/Combat/DivineBeastsAbilityBarProjectionDiff.h"

bool FDivineBeastsAbilityBarProjectionDiff::HasSlotsChanged(
    const TArray<FGamePlatformUISlotState>& Previous,
    const TArray<FGamePlatformUISlotState>& Current)
{
    if (Previous.Num() != Current.Num())
    {
        return true;
    }
    for (int32 Index = 0; Index < Previous.Num(); ++Index)
    {
        const FGamePlatformUISlotState& A = Previous[Index];
        const FGamePlatformUISlotState& B = Current[Index];
        if (A.SlotId != B.SlotId || A.ContentId != B.ContentId ||
            A.Icon != B.Icon || A.bEnabled != B.bEnabled ||
            A.bPending != B.bPending ||
            !FMath::IsNearlyEqual(A.OverlayProgress, B.OverlayProgress, 0.0001f))
        {
            return true;
        }
    }
    return false;
}

bool FDivineBeastsAbilityBarProjectionDiff::HasDetailsChanged(
    const TArray<FDivineBeastsAbilitySlotDetails>& Previous,
    const TArray<FDivineBeastsAbilitySlotDetails>& Current)
{
    if (Previous.Num() != Current.Num())
    {
        return true;
    }
    for (int32 Index = 0; Index < Previous.Num(); ++Index)
    {
        const FDivineBeastsAbilitySlotDetails& A = Previous[Index];
        const FDivineBeastsAbilitySlotDetails& B = Current[Index];
        if (A.SlotId != B.SlotId || A.AbilityId != B.AbilityId ||
            A.AbilityLevel != B.AbilityLevel ||
            !A.DisplayName.EqualTo(B.DisplayName) ||
            !A.Description.EqualTo(B.Description) ||
            !A.DisabledReason.EqualTo(B.DisabledReason) ||
            !FMath::IsNearlyEqual(A.CooldownRemainingSeconds, B.CooldownRemainingSeconds, 0.01f) ||
            !FMath::IsNearlyEqual(A.CooldownTotalSeconds, B.CooldownTotalSeconds, 0.01f) ||
            // 即使剩余秒数与遮罩近似相同，新的GAS冷却终点仍要触发材质动画刷新。
            !FMath::IsNearlyEqual(A.CooldownEndsAtAppSeconds, B.CooldownEndsAtAppSeconds, 0.01f))
        {
            return true;
        }
    }
    return false;
}
