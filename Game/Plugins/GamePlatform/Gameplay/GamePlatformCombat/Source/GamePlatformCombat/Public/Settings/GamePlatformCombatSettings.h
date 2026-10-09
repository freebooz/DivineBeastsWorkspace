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

    /** 每个服务器权威盾效果最多可吸收的点数；防止错配资源无限堆叠。 */
    UPROPERTY(Config, EditAnywhere, Category="ShieldEffects", meta=(ClampMin="0.0"))
    float MaxShieldCapacity = 100000.0f;

    /** 临时盾效果持续秒数上限，失效由GAS定时器负责，不由每帧Tick轮询。 */
    UPROPERTY(Config, EditAnywhere, Category="ShieldEffects", meta=(ClampMin="0.0"))
    float MaxShieldDuration = 120.0f;

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
