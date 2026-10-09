// 服务器装备GAS桥：局部保活Resolver，授予候选逐步复核作用域；撤销先脱离句柄，允许GAS通知同步关闭组件。
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
    FGamePlatformEquipmentGameplayGrantHandle& OutHandle,
    TFunction<bool()> IsScopeCurrent) const
{
    OutHandle.Reset();
    const TWeakObjectPtr<UAbilitySystemComponent> WeakASC(AbilitySystem);
    const auto ScopeCurrent = [&]() { return WeakASC.IsValid() && (!IsScopeCurrent || IsScopeCurrent()); };
    const auto LocalResolver = Resolver;
    const auto AbilitySetIds = Definition.AbilitySetDefinitionIds;
    const auto EffectIds = Definition.GameplayEffectDefinitionIds;

    if (!ScopeCurrent())
    {
        return false;
    }

    if (AbilitySetIds.IsEmpty() &&
        EffectIds.IsEmpty())
    {
        OutHandle.GrantId = FGuid::NewGuid();
        return true;
    }

    if (!LocalResolver.IsValid())
    {
        return false;
    }

    TArray<TSubclassOf<UGameplayAbility>> Abilities;
    TArray<TSubclassOf<UGameplayEffect>> Effects;

    for (const FName AbilitySetId :
         AbilitySetIds)
    {
        TArray<TSubclassOf<UGameplayAbility>> SetAbilities;
        TArray<TSubclassOf<UGameplayEffect>> SetEffects;

        if (!LocalResolver->ResolveAbilitySet(
                AbilitySetId,
                SetAbilities,
                SetEffects) || !ScopeCurrent())
        {
            Revoke(WeakASC.Get(), OutHandle);
            return false;
        }

        Abilities.Append(SetAbilities);
        Effects.Append(SetEffects);
    }

    for (const FName EffectId :
         EffectIds)
    {
        TSubclassOf<UGameplayEffect> EffectClass;
        if (!LocalResolver->ResolveGameplayEffect(
                EffectId,
                EffectClass) ||
            !EffectClass || !ScopeCurrent())
        {
            Revoke(WeakASC.Get(), OutHandle);
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
            Revoke(WeakASC.Get(), OutHandle);
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
            Revoke(WeakASC.Get(), OutHandle);
            return false;
        }

        OutHandle.AbilityHandles.Add(Handle);
        if (!ScopeCurrent()) { Revoke(WeakASC.Get(), OutHandle); return false; }
    }

    for (const TSubclassOf<UGameplayEffect>& EffectClass :
         Effects)
    {
        if (!EffectClass)
        {
            Revoke(WeakASC.Get(), OutHandle);
            return false;
        }

        const UGameplayEffect* Effect =
            EffectClass->GetDefaultObject<UGameplayEffect>();

        if (!IsValid(Effect) || !ScopeCurrent())
        {
            Revoke(WeakASC.Get(), OutHandle);
            return false;
        }

        FGameplayEffectContextHandle Context =
            AbilitySystem->MakeEffectContext();
        if (!ScopeCurrent()) { Revoke(WeakASC.Get(), OutHandle); return false; }

        const FActiveGameplayEffectHandle Handle =
            AbilitySystem->ApplyGameplayEffectToSelf(
                Effect,
                1.0f,
                Context);

        if (!Handle.IsValid())
        {
            Revoke(WeakASC.Get(), OutHandle);
            return false;
        }

        OutHandle.EffectHandles.Add(Handle);
        if (!ScopeCurrent()) { Revoke(WeakASC.Get(), OutHandle); return false; }
    }

    return true;
}

void FGamePlatformEquipmentGASGrantPort::Revoke(
    UAbilitySystemComponent* AbilitySystem,
    FGamePlatformEquipmentGameplayGrantHandle& Handle) const
{
    // 先消费所有权；ClearAbility/RemoveEffect可同步再次调用清理，不能遍历被重入清空的容器。
    auto Abilities = MoveTemp(Handle.AbilityHandles);
    auto Effects = MoveTemp(Handle.EffectHandles);
    Handle.Reset();
    const TWeakObjectPtr<UAbilitySystemComponent> WeakASC(AbilitySystem);
    for (const auto& Ability : Abilities)
    { if (auto* ASC = WeakASC.Get(); IsValid(ASC) && Ability.IsValid()) { ASC->ClearAbility(Ability); } }
    for (const auto& Effect : Effects)
    { if (auto* ASC = WeakASC.Get(); IsValid(ASC) && Effect.IsValid()) { ASC->RemoveActiveGameplayEffect(Effect); } }
}
