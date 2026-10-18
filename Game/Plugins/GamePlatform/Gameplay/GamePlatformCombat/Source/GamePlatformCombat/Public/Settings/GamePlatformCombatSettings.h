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

    /** 减速最大比例：最多70%，最终倍率下限0.30；服务端与GE/属性聚合共同限制。 */
    UPROPERTY(Config, EditAnywhere, Category="Control", meta=(ClampMin="0.05", ClampMax="0.70"))
    float MaxSlowFraction = 0.70f;

    /** 持续减速不超过10秒；V6控制槽另限制最多3秒。 */
    UPROPERTY(Config, EditAnywhere, Category="Control", meta=(ClampMin="0.1", ClampMax="10.0"))
    float MaxSlowDuration = 10.0f;

    UPROPERTY(Config, EditAnywhere, Category="Targeting")
    bool bAllowSelfDamage = false;

    UPROPERTY(Config, EditAnywhere, Category="Targeting")
    bool bAllowSelfHealing = true;
};
