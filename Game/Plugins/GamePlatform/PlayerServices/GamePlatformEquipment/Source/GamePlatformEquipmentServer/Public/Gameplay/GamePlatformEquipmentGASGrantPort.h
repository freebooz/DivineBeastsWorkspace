#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UGamePlatformEquipmentDefinition;

class GAMEPLATFORMEQUIPMENTSERVER_API IGamePlatformEquipmentGameplayAssetResolver
{
public:
    virtual ~IGamePlatformEquipmentGameplayAssetResolver() = default;

    virtual bool ResolveAbilitySet(
        FName AbilitySetDefinitionId,
        TArray<TSubclassOf<UGameplayAbility>>& OutAbilities,
        TArray<TSubclassOf<UGameplayEffect>>& OutEffects) = 0;

    virtual bool ResolveGameplayEffect(
        FName GameplayEffectDefinitionId,
        TSubclassOf<UGameplayEffect>& OutEffect) = 0;
};

struct GAMEPLATFORMEQUIPMENTSERVER_API FGamePlatformEquipmentGameplayGrantHandle
{
    FGuid GrantId;
    TArray<FGameplayAbilitySpecHandle> AbilityHandles;
    TArray<FActiveGameplayEffectHandle> EffectHandles;

    bool IsValid() const
    {
        return GrantId.IsValid();
    }

    void Reset()
    {
        GrantId.Invalidate();
        AbilityHandles.Reset();
        EffectHandles.Reset();
    }
};

class GAMEPLATFORMEQUIPMENTSERVER_API FGamePlatformEquipmentGASGrantPort
{
public:
    explicit FGamePlatformEquipmentGASGrantPort(
        TSharedPtr<
            IGamePlatformEquipmentGameplayAssetResolver,
            ESPMode::ThreadSafe> InResolver);

    bool Grant(
        UAbilitySystemComponent* AbilitySystem,
        const UGamePlatformEquipmentDefinition& Definition,
        FGamePlatformEquipmentGameplayGrantHandle& OutHandle) const;

    void Revoke(
        UAbilitySystemComponent* AbilitySystem,
        FGamePlatformEquipmentGameplayGrantHandle& Handle) const;

private:
    TSharedPtr<
        IGamePlatformEquipmentGameplayAssetResolver,
        ESPMode::ThreadSafe> Resolver;
};
