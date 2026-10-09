#include "Types/GamePlatformCombatMath.h"

namespace
{
float SafeNonNegative(float Value)
{
    return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}

float SafeUnit(float Value)
{
    return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
}
}

FGamePlatformDamageFormulaOutput FGamePlatformCombatMath::CalculateDamageMagnitude(
    const FGamePlatformDamageFormulaInput& Input)
{
    FGamePlatformDamageFormulaOutput Output;

    const float BaseDamage = SafeNonNegative(Input.BaseDamage);
    const float AttackPower = SafeNonNegative(Input.AttackPower);
    const float AttackCoefficient = SafeNonNegative(Input.AttackPowerCoefficient);
    const float AbilityPower = SafeNonNegative(Input.AbilityPower);
    const float AbilityCoefficient = SafeNonNegative(Input.AbilityPowerCoefficient);

    float RawDamage = BaseDamage +
        (AttackPower * AttackCoefficient) +
        (AbilityPower * AbilityCoefficient);

    Output.bCritical = IsCriticalHit(
        Input.bCanCritical,
        Input.CriticalChance,
        Input.CriticalRoll);
    if (Output.bCritical)
    {
        RawDamage *= FMath::Max(1.0f, SafeNonNegative(Input.CriticalDamage));
    }

    Output.RawDamage = SafeNonNegative(RawDamage);
    float MitigationMultiplier = 1.0f;

    switch (Input.DamageType)
    {
    case EGamePlatformDamageType::Physical:
    {
        Output.EffectiveDefense = FMath::Max(
            0.0f,
            SafeNonNegative(Input.Armor) - SafeNonNegative(Input.ArmorPenetration));
        const float Constant = FMath::Max(1.0f, SafeNonNegative(Input.DefenseMitigationConstant));
        MitigationMultiplier = Constant / (Constant + Output.EffectiveDefense);
        break;
    }
    case EGamePlatformDamageType::Magic:
    {
        Output.EffectiveDefense = FMath::Max(
            0.0f,
            SafeNonNegative(Input.MagicResistance) - SafeNonNegative(Input.MagicPenetration));
        const float Constant = FMath::Max(1.0f, SafeNonNegative(Input.DefenseMitigationConstant));
        MitigationMultiplier = Constant / (Constant + Output.EffectiveDefense);
        break;
    }
    case EGamePlatformDamageType::TrueDamage:
        // TrueDamage（真实伤害）明确忽略护甲/法抗和通用DamageReduction。
        Output.FinalDamage = Output.RawDamage;
        return Output;
    case EGamePlatformDamageType::Untyped:
    default:
        break;
    }

    const float DamageReductionMultiplier = 1.0f - SafeUnit(Input.DamageReduction);
    Output.FinalDamage = SafeNonNegative(
        Output.RawDamage * MitigationMultiplier * DamageReductionMultiplier);
    return Output;
}

float FGamePlatformCombatMath::MakeDeterministicUnitRoll(const FGuid& EventId)
{
    if (!EventId.IsValid())
    {
        return 1.0f;
    }

    const uint32 Hash = GetTypeHash(EventId);
    constexpr uint32 Mask24 = 0x00FFFFFFu;
    constexpr float Denominator = 16777216.0f; // 2^24
    return static_cast<float>(Hash & Mask24) / Denominator;
}

bool FGamePlatformCombatMath::IsCriticalHit(
    bool bCanCritical,
    float CriticalChance,
    float CriticalRoll)
{
    return bCanCritical &&
        SafeUnit(CriticalRoll) < SafeUnit(CriticalChance);
}

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
