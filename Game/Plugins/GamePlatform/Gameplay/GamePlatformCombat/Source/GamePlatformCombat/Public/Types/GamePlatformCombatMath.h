#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformCombatResult.h"
#include "Types/GamePlatformCombatTypes.h"

/** FGamePlatformDamageFormulaInput（平台通用伤害公式输入）。 */
struct GAMEPLATFORMCOMBAT_API FGamePlatformDamageFormulaInput
{
    float BaseDamage = 0.0f;
    /** 施放者GAS属性DamageBonus（伤害增强），Buff为正数，削弱为负数，单位伤害点。 */
    float DamageBonus = 0.0f;
    /** 目标GAS属性DamageReduction（伤害减免），Buff为正数，易伤Debuff为负数。 */
    float DamageReduction = 0.0f;
    EGamePlatformDamageType DamageType = EGamePlatformDamageType::Untyped;
};

/** FGamePlatformDamageFormulaOutput（平台通用伤害公式输出）。 */
struct GAMEPLATFORMCOMBAT_API FGamePlatformDamageFormulaOutput
{
    float RawDamage = 0.0f;
    float FinalDamage = 0.0f;
};

class GAMEPLATFORMCOMBAT_API FGamePlatformCombatMath
{
public:
    /** 纯函数伤害公式；不读取Actor/ASC，便于自动化测试和服务器复现。 */
    static FGamePlatformDamageFormulaOutput CalculateDamageMagnitude(
        const FGamePlatformDamageFormulaInput& Input);

    static FGamePlatformCombatResult ResolveDamage(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float AvailableShieldEffectCapacity,
        float CurrentHealth,
        bool bBypassShield);

    static FGamePlatformCombatResult ResolveHealing(
        const FGuid& EventId,
        float RequestedMagnitude,
        float FinalMagnitude,
        float CurrentHealth,
        float MaxHealth);
};
