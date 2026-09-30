#include "Definitions/DivineBeastsMomentumDefinition.h"

bool FDivineBeastsMomentumDefinition::IsValid(FString& OutError) const
{
    if (!FMath::IsFinite(InitialMomentum) ||
        !FMath::IsFinite(MaxMomentum) ||
        !FMath::IsFinite(GainMultiplier) ||
        !FMath::IsFinite(DecayRate))
    {
        OutError = TEXT("Momentum Definition包含非有限数值。");
        return false;
    }
    if (MaxMomentum < 0.0f || InitialMomentum < 0.0f || InitialMomentum > MaxMomentum)
    {
        OutError = TEXT("Momentum必须满足0 <= InitialMomentum <= MaxMomentum。");
        return false;
    }
    if (GainMultiplier < 0.0f || DecayRate < 0.0f)
    {
        OutError = TEXT("Momentum GainMultiplier/DecayRate不得为负。");
        return false;
    }
    return true;
}
