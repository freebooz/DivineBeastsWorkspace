#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformCombatSettings.generated.h"

/** 平台战斗安全上限与非敏感策略配置。 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Combat"))
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Magnitude", meta=(ClampMin="0.0"))
    float MaxDamageMagnitude = 100000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Magnitude", meta=(ClampMin="0.0"))
    float MaxHealingMagnitude = 100000.0f;

    /** 单个AttackPower/AbilityPower伤害系数安全上限，防止错误Definition放大到非有限范围。 */
    UPROPERTY(Config, EditAnywhere, Category="DamageFormula", meta=(ClampMin="0.0"))
    float MaxDamageAttributeCoefficient = 10.0f;

    /** Armor/MagicResistance减伤曲线常数；默认100。 */
    UPROPERTY(Config, EditAnywhere, Category="DamageFormula", meta=(ClampMin="1.0"))
    float DefenseMitigationConstant = 100.0f;

    UPROPERTY(Config, EditAnywhere, Category="HitValidation", meta=(ClampMin="0.0"))
    float MaxHitDistance = 5000.0f;

    UPROPERTY(Config, EditAnywhere, Category="HitValidation", meta=(ClampMin="0.0"))
    float MaxTraceOriginOffset = 250.0f;

    UPROPERTY(Config, EditAnywhere, Category="HitValidation", meta=(ClampMin="0.0"))
    float MaxSweepRadius = 300.0f;

    UPROPERTY(Config, EditAnywhere, Category="Control", meta=(ClampMin="0.0"))
    float MaxControlDuration = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category="Targeting")
    bool bAllowSelfDamage = false;

    UPROPERTY(Config, EditAnywhere, Category="Targeting")
    bool bAllowSelfHealing = true;
};
