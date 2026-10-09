#pragma once

#include "GameplayEffect.h"
#include "GamePlatformCombatGameplayEffects.generated.h"

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformDamageGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UGamePlatformDamageGameplayEffect();
};

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformHealingGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UGamePlatformHealingGameplayEffect();
};

/**
 * UGamePlatformShieldGameplayEffect（平台限时护盾玩法效果）。
 * 只授予Shielded（被护盾保护）标签和独立GE生命周期，不包含Shield/MaxShield数值属性。
 * 实际可吸收容量属于目标CombatComponent（战斗组件）的服务器临时效果实例账本，
 * 过期/驱散后不能再吸收。多个独立来源不使用单栈刷新替换。
 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformShieldGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    explicit UGamePlatformShieldGameplayEffect(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformStunGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    /**
     * 构造眩晕效果的类默认对象，并以稳定名称创建其标签组件。
     *
     * @param ObjectInitializer UE 默认子对象构造器；必须由引擎传入，禁止在构造期改用无名称 NewObject。
     */
    explicit UGamePlatformStunGameplayEffect(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformSilenceGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    /**
     * 构造沉默效果的类默认对象，并以稳定名称创建其标签组件。
     *
     * @param ObjectInitializer UE 默认子对象构造器；必须由引擎传入，禁止在构造期改用无名称 NewObject。
     */
    explicit UGamePlatformSilenceGameplayEffect(const FObjectInitializer& ObjectInitializer);
};
