#include "Gameplay/GamePlatformEquipmentGASGrantPort.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "GameplayEffect.h"

FGamePlatformEquipmentGASGrantPort::FGamePlatformEquipmentGASGrantPort(
    TSharedPtr<
        IGamePlatformEquipmentGameplayAssetResolver,
        ESPMode::ThreadSafe> InResolver)
    : Resolver(MoveTemp(InResolver))
{
}

bool FGamePlatformEquipmentGASGrantPort::Grant(
    UAbilitySystemComponent* AbilitySystem,
    const UGamePlatformEquipmentDefinition& Definition,
    FGamePlatformEquipmentGameplayGrantHandle& OutHandle) const
{
    OutHandle.Reset();

    if (!IsValid(AbilitySystem))
    {
        return false;
    }

    if (Definition.AbilitySetDefinitionIds.IsEmpty() &&
        Definition.GameplayEffectDefinitionIds.IsEmpty())
    {
        OutHandle.GrantId = FGuid::NewGuid();
        return true;
    }

    if (!Resolver.IsValid())
    {
        return false;
    }

    TArray<TSubclassOf<UGameplayAbility>> Abilities;
    TArray<TSubclassOf<UGameplayEffect>> Effects;

    for (const FName AbilitySetId :
         Definition.AbilitySetDefinitionIds)
    {
        TArray<TSubclassOf<UGameplayAbility>> SetAbilities;
        TArray<TSubclassOf<UGameplayEffect>> SetEffects;

        if (!Resolver->ResolveAbilitySet(
                AbilitySetId,
                SetAbilities,
                SetEffects))
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        Abilities.Append(SetAbilities);
        Effects.Append(SetEffects);
    }

    for (const FName EffectId :
         Definition.GameplayEffectDefinitionIds)
    {
        TSubclassOf<UGameplayEffect> EffectClass;
        if (!Resolver->ResolveGameplayEffect(
                EffectId,
                EffectClass) ||
            !EffectClass)
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        Effects.Add(EffectClass);
    }

    OutHandle.GrantId = FGuid::NewGuid();

    for (const TSubclassOf<UGameplayAbility>& AbilityClass :
         Abilities)
    {
        if (!AbilityClass)
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        const FGameplayAbilitySpecHandle Handle =
            AbilitySystem->GiveAbility(
                FGameplayAbilitySpec(
                    AbilityClass,
                    1,
                    INDEX_NONE,
                    nullptr));

        if (!Handle.IsValid())
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        OutHandle.AbilityHandles.Add(Handle);
    }

    for (const TSubclassOf<UGameplayEffect>& EffectClass :
         Effects)
    {
        if (!EffectClass)
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        const UGameplayEffect* Effect =
            EffectClass->GetDefaultObject<UGameplayEffect>();

        if (!IsValid(Effect))
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        FGameplayEffectContextHandle Context =
            AbilitySystem->MakeEffectContext();

        const FActiveGameplayEffectHandle Handle =
            AbilitySystem->ApplyGameplayEffectToSelf(
                Effect,
                1.0f,
                Context);

        if (!Handle.IsValid())
        {
            Revoke(AbilitySystem, OutHandle);
            return false;
        }

        OutHandle.EffectHandles.Add(Handle);
    }

    return true;
}

void FGamePlatformEquipmentGASGrantPort::Revoke(
    UAbilitySystemComponent* AbilitySystem,
    FGamePlatformEquipmentGameplayGrantHandle& Handle) const
{
    if (IsValid(AbilitySystem))
    {
        for (const FGameplayAbilitySpecHandle& AbilityHandle :
             Handle.AbilityHandles)
        {
            if (AbilityHandle.IsValid())
            {
                AbilitySystem->ClearAbility(AbilityHandle);
            }
        }

        for (const FActiveGameplayEffectHandle& EffectHandle :
             Handle.EffectHandles)
        {
            if (EffectHandle.IsValid())
            {
                AbilitySystem->RemoveActiveGameplayEffect(
                    EffectHandle);
            }
        }
    }

    Handle.Reset();
}
