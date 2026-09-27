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
    UGamePlatformStunGameplayEffect();
};

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformSilenceGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UGamePlatformSilenceGameplayEffect();
};
