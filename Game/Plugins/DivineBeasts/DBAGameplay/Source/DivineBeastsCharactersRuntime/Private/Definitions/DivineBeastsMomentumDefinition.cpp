#include "Definitions/DivineBeastsMomentumDefinition.h"

bool FDivineBeastsMomentumDefinition::IsValid(FString& OutError) const
{
    if (!FMath::IsFinite(InitialMomentum) ||
        !FMath::IsFinite(MaxMomentum) ||
        !FMath::IsFinite(BasicAttackHitGain) ||
        !FMath::IsFinite(GainMultiplier) ||
        !FMath::IsFinite(DecayRate))
    {
        OutError = TEXT("Momentum Definition包含非有限数值。");
        return false;
    }
    if (MaxMomentum < 0.0f || MaxMomentum > 100.0f ||
        InitialMomentum < 0.0f || InitialMomentum > MaxMomentum)
    {
        OutError = TEXT("Momentum必须满足0 <= InitialMomentum <= MaxMomentum <= 100。");
        return false;
    }
    if (BasicAttackHitGain < 0.0f || BasicAttackHitGain > 100.0f ||
        GainMultiplier < 0.0f || DecayRate < 0.0f)
    {
        OutError = TEXT("气势普攻命中收益须在0～100范围，旧兼容倍率/衰减不得为负。");
        return false;
    }
    return true;
}

float FDivineBeastsMomentumDefinition::CalculateBasicAttackHitGain(
    const float CurrentMomentum, const float CurrentMaxMomentum) const
{
    if (!FMath::IsFinite(CurrentMomentum) || !FMath::IsFinite(CurrentMaxMomentum) ||
        !FMath::IsFinite(BasicAttackHitGain) || BasicAttackHitGain <= 0.0f ||
        CurrentMaxMomentum <= 0.0f || CurrentMaxMomentum > 100.0f ||
        CurrentMomentum < 0.0f || CurrentMomentum > CurrentMaxMomentum)
    {
        return 0.0f;
    }
    return FMath::Min(BasicAttackHitGain, CurrentMaxMomentum - CurrentMomentum);
}
