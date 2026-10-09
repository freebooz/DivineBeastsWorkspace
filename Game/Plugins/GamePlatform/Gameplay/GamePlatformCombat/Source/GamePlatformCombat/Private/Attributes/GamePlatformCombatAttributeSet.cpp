#include "Attributes/GamePlatformCombatAttributeSet.h"

#include "Components/GamePlatformCombatComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UGamePlatformCombatAttributeSet::UGamePlatformCombatAttributeSet()
{
    InitMaxHealth(100.0f);
    InitHealth(100.0f);
    InitMaxShield(100.0f);
    InitShield(0.0f);
    InitIncomingDamage(0.0f);
    InitIncomingHealing(0.0f);
}

void UGamePlatformCombatAttributeSet::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 四项生命/护盾可见值按Actor相关性复制，其余战斗细节不由本属性集公开。
    // 不能仅用COND_None视作迷雾授权：战场视野仍须控制Actor相关性或提供经批准的观测快照。
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
        Shield,
        COND_None,
        REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet,
        MaxShield,
        COND_None,
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
    else if (Attribute == GetMaxShieldAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetShieldAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxShield()));
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
    SetMaxShield(FMath::Max(0.0f, GetMaxShield()));
    SetShield(FMath::Clamp(GetShield(), 0.0f, GetMaxShield()));
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

void UGamePlatformCombatAttributeSet::OnRep_Shield(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, Shield, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_MaxShield(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, MaxShield, OldValue);
}
