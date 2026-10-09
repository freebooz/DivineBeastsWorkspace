#include "Types/GamePlatformCombatMath.h"

namespace
{
float SafeNonNegative(float Value)
{
    return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}

}

FGamePlatformDamageFormulaOutput FGamePlatformCombatMath::CalculateDamageMagnitude(
    const FGamePlatformDamageFormulaInput& Input)
{
    FGamePlatformDamageFormulaOutput Output;

    const float BaseDamage = SafeNonNegative(Input.BaseDamage);
    // 一条服务器权威加减链：技能基础伤害 + 来源GAS伤害增减。
    // 增减数值通过GAS GameplayEffect的Additive修饰器叠加，无需额外属性集。
    const float Bonus = FMath::IsFinite(Input.DamageBonus)
        ? Input.DamageBonus : 0.0f;
    const float Reduction = FMath::IsFinite(Input.DamageReduction)
        ? Input.DamageReduction : 0.0f;
    const float Enhanced = SafeNonNegative(BaseDamage + Bonus);
    Output.RawDamage = Enhanced;

    // 保留Physical/Magic（物理/法术）的原有类型身份，但不再走不同的护甲/法抗公式。
    // TrueDamage（真实伤害）保留绕过普通减伤的约定，仍可由独立护盾GE先吸收。
    if (Input.DamageType == EGamePlatformDamageType::TrueDamage)
    {
        Output.FinalDamage = Enhanced;
        return Output;
    }

    // 普通防御Buff增加Reduction，易伤Debuff可使Reduction为负，提高最终伤害。
    Output.FinalDamage = SafeNonNegative(Enhanced - Reduction);
    return Output;
}

FGamePlatformCombatResult FGamePlatformCombatMath::ResolveDamage(
    const FGuid& EventId,
    float RequestedMagnitude,
    float FinalMagnitude,
    float AvailableShieldEffectCapacity,
    float CurrentHealth,
    bool bBypassShield)
{
    FGamePlatformCombatResult Result;
    Result.EventId = EventId;
    Result.RequestedMagnitude = FMath::Max(0.0f, RequestedMagnitude);
    Result.FinalMagnitude = FMath::Max(0.0f, FinalMagnitude);

    // Shield只是限时GE的剩余吸收容量，不再来自AttributeSet（永久GAS属性）。
    const float SafeShield = SafeNonNegative(AvailableShieldEffectCapacity);
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
    float MaxHealth)
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
    return Result;
}
