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
