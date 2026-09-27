#include "Types/GamePlatformCombatMath.h"

FGamePlatformCombatResult FGamePlatformCombatMath::ResolveDamage(
    const FGuid& EventId,
    float RequestedMagnitude,
    float FinalMagnitude,
    float CurrentShield,
    float CurrentHealth,
    bool bBypassShield)
{
    FGamePlatformCombatResult Result;
    Result.EventId = EventId;
    Result.RequestedMagnitude = FMath::Max(0.0f, RequestedMagnitude);
    Result.FinalMagnitude = FMath::Max(0.0f, FinalMagnitude);

    const float SafeShield = FMath::Max(0.0f, CurrentShield);
    const float SafeHealth = FMath::Max(0.0f, CurrentHealth);
    float RemainingDamage = Result.FinalMagnitude;

    if (!bBypassShield && SafeShield > 0.0f)
    {
        Result.AppliedToShield = FMath::Min(SafeShield, RemainingDamage);
        RemainingDamage -= Result.AppliedToShield;
    }

    if (RemainingDamage > 0.0f && SafeHealth > 0.0f)
    {
        Result.AppliedToHealth = FMath::Min(SafeHealth, RemainingDamage);
    }

    Result.RemainingShield =
        FMath::Max(0.0f, SafeShield - Result.AppliedToShield);
    Result.RemainingHealth =
        FMath::Max(0.0f, SafeHealth - Result.AppliedToHealth);
    Result.bCausedDeath =
        SafeHealth > 0.0f && Result.RemainingHealth <= 0.0f;

    return Result;
}

FGamePlatformCombatResult FGamePlatformCombatMath::ResolveHealing(
    const FGuid& EventId,
    float RequestedMagnitude,
    float FinalMagnitude,
    float CurrentHealth,
    float MaxHealth,
    float CurrentShield)
{
    FGamePlatformCombatResult Result;
    Result.EventId = EventId;
    Result.RequestedMagnitude = FMath::Max(0.0f, RequestedMagnitude);
    Result.FinalMagnitude = FMath::Max(0.0f, FinalMagnitude);

    const float SafeMaxHealth = FMath::Max(0.0f, MaxHealth);
    const float SafeHealth = FMath::Clamp(CurrentHealth, 0.0f, SafeMaxHealth);
    const float MissingHealth = FMath::Max(0.0f, SafeMaxHealth - SafeHealth);

    Result.AppliedToHealth = FMath::Min(MissingHealth, Result.FinalMagnitude);
    Result.RemainingHealth =
        FMath::Min(SafeMaxHealth, SafeHealth + Result.AppliedToHealth);
    Result.RemainingShield = FMath::Max(0.0f, CurrentShield);
    return Result;
}
