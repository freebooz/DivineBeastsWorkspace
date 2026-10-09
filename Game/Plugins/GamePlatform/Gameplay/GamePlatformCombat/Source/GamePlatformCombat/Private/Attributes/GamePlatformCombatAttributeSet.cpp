#include "Attributes/GamePlatformCombatAttributeSet.h"

#include "Components/GamePlatformCombatComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Settings/GamePlatformCombatSettings.h"

UGamePlatformCombatAttributeSet::UGamePlatformCombatAttributeSet()
{
    InitMaxHealth(100.0f);
    InitHealth(100.0f);
    InitDamageBonus(0.0f);
    InitDamageReduction(0.0f);
    InitIncomingDamage(0.0f);
    InitIncomingHealing(0.0f);
}

void UGamePlatformCombatAttributeSet::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Health与MaxHealth按Actor相关性复制（还需战场迷雾资格门禁）。
    // DamageBonus和DamageReduction只向拥有者复制，不公开战斗内部加减配置。
    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet,
        Health,
        COND_None,
        REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet,
        MaxHealth,
        COND_None,
        REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet,
        DamageBonus,
        COND_OwnerOnly,
        REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet,
        DamageReduction,
        COND_OwnerOnly,
        REPNOTIFY_Always);
}

void UGamePlatformCombatAttributeSet::PreAttributeChange(
    const FGameplayAttribute& Attribute,
    float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (!FMath::IsFinite(NewValue))
    {
        NewValue = 0.0f;
    }

    if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxHealth()));
    }
    else if (Attribute == GetDamageBonusAttribute() ||
             Attribute == GetDamageReductionAttribute())
    {
        // 允许Buff/Debuff的有符号加减；安全上限只保护数值不溢出，不作用户可写权限。
        const float Limit = FMath::Max(
            0.0f, GetDefault<UGamePlatformCombatSettings>()->MaxDamageMagnitude);
        NewValue = FMath::Clamp(NewValue, -Limit, Limit);
    }
    else if (Attribute == GetIncomingDamageAttribute() ||
             Attribute == GetIncomingHealingAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UGamePlatformCombatAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent();
    AActor* AvatarActor = AbilitySystem ? AbilitySystem->GetAvatarActor() : nullptr;
    UGamePlatformCombatComponent* CombatComponent =
        AvatarActor ? AvatarActor->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float FinalDamage = FMath::Max(0.0f, GetIncomingDamage());
        SetIncomingDamage(0.0f);

        if (FinalDamage > 0.0f && IsValid(CombatComponent))
        {
            CombatComponent->ResolveIncomingDamage(*this, Data.EffectSpec, FinalDamage);
        }
    }
    else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
    {
        const float FinalHealing = FMath::Max(0.0f, GetIncomingHealing());
        SetIncomingHealing(0.0f);

        if (FinalHealing > 0.0f && IsValid(CombatComponent))
        {
            CombatComponent->ResolveIncomingHealing(*this, Data.EffectSpec, FinalHealing);
        }
    }

    SetMaxHealth(FMath::Max(0.0f, GetMaxHealth()));
    SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    const float Limit = FMath::Max(
        0.0f, GetDefault<UGamePlatformCombatSettings>()->MaxDamageMagnitude);
    SetDamageBonus(FMath::Clamp(GetDamageBonus(), -Limit, Limit));
    SetDamageReduction(FMath::Clamp(GetDamageReduction(), -Limit, Limit));
}

void UGamePlatformCombatAttributeSet::OnRep_Health(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, Health, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_MaxHealth(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, MaxHealth, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_DamageBonus(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, DamageBonus, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_DamageReduction(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, DamageReduction, OldValue);
}
