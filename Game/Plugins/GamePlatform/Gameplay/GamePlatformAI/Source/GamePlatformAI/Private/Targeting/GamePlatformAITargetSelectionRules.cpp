#include "Targeting/GamePlatformAITargetSelectionRules.h"

bool FGamePlatformAITargetSelectionRules::IsBetter(
    const FGamePlatformAITargetSortKey& Candidate,
    const FGamePlatformAITargetSortKey& Current,
    bool bPreferVisible)
{
    if (Candidate.bEligible != Current.bEligible)
    {
        return Candidate.bEligible;
    }

    if (!Candidate.bEligible)
    {
        return false;
    }

    if (bPreferVisible &&
        Candidate.bVisible != Current.bVisible)
    {
        return Candidate.bVisible;
    }

    if (!FMath::IsNearlyEqual(
            Candidate.DistanceSquared,
            Current.DistanceSquared))
    {
        return Candidate.DistanceSquared <
            Current.DistanceSquared;
    }

    if (!FMath::IsNearlyEqual(
            Candidate.LastSensedTime,
            Current.LastSensedTime))
    {
        return Candidate.LastSensedTime >
            Current.LastSensedTime;
    }

    return Candidate.EntityId.ToString(EGuidFormats::Digits) <
        Current.EntityId.ToString(EGuidFormats::Digits);
}
