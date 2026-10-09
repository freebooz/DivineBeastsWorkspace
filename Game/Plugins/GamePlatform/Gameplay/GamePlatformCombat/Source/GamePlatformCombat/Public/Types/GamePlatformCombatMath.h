#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformCombatResult.h"
#include "Types/GamePlatformCombatTypes.h"

/** FGamePlatformDamageFormulaInput（平台通用伤害公式输入）。 */
struct GAMEPLATFORMCOMBAT_API FGamePlatformDamageFormulaInput
{
    float BaseDamage = 0.0f;
    float AttackPower = 0.0f;
    float AttackPowerCoefficient = 0.0f;
    float AbilityPower = 0.0f;
    float AbilityPowerCoefficient = 0.0f;
    float Armor = 0.0f;
    float ArmorPenetration = 0.0f;
    float MagicResistance = 0.0f;
    float MagicPenetration = 0.0f;
    float DamageReduction = 0.0f;
    float CriticalChance = 0.0f;
    float CriticalDamage = 1.0f;
    float CriticalRoll = 1.0f;
    float DefenseMitigationConstant = 100.0f;
    EGamePlatformDamageType DamageType = EGamePlatformDamageType::Untyped;
    bool bCanCritical = false;
};

/** FGamePlatformDamageFormulaOutput（平台通用伤害公式输出）。 */
struct GAMEPLATFORMCOMBAT_API FGamePlatformDamageFormulaOutput
{
    float RawDamage = 0.0f;
    float EffectiveDefense = 0.0f;
    float FinalDamage = 0.0f;
    bool bCritical = false;
};

class GAMEPLATFORMCOMBAT_API FGamePlatformCombatMath
{
public:
    /** 纯函数伤害公式；不读取Actor/ASC，便于自动化测试和服务器复现。 */
    static FGamePlatformDamageFormulaOutput CalculateDamageMagnitude(
        const FGamePlatformDamageFormulaInput& Input);

    /** 使用EventId生成0..1确定性服务器掷值，同一事件重放保持一致。 */
    static float MakeDeterministicUnitRoll(const FGuid& EventId);

    static bool IsCriticalHit(bool bCanCritical, float CriticalChance, float CriticalRoll);

    static FGamePlatformCombatResult ResolveDamage(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float CurrentShield,
        float CurrentHealth,
        bool bBypassShield);

    static FGamePlatformCombatResult ResolveHealing(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float CurrentHealth,
        float MaxHealth,
        float CurrentShield);
};
