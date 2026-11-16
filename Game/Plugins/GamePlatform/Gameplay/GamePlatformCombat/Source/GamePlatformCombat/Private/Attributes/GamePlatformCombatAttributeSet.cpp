#include "Attributes/GamePlatformCombatAttributeSet.h"

#include "Components/GamePlatformCombatComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Settings/GamePlatformCombatSettings.h"

UGamePlatformCombatAttributeSet::UGamePlatformCombatAttributeSet()
{
    InitMaxHealth(100.0f);
    InitHealth(100.0f);
    InitMaxEnergy(100.0f);
    InitEnergy(100.0f);
    InitDamageBonus(0.0f);
    InitDamageReduction(0.0f);
    InitMoveSpeedMultiplier(1.0f);
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

    // 能量为独立资源，只向拥有者复制，避免泄露敌方私有资源。
    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet, Energy, COND_OwnerOnly, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet, MaxEnergy, COND_OwnerOnly, REPNOTIFY_Always);

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
    // 移动速度影响远端可见运动与动画，按Actor网络相关性向拥有者和观察者复制。
    DOREPLIFETIME_CONDITION_NOTIFY(
        UGamePlatformCombatAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
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
    else if (Attribute == GetMaxEnergyAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    else if (Attribute == GetEnergyAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxEnergy()));
    }
    else if (Attribute == GetDamageBonusAttribute() ||
             Attribute == GetDamageReductionAttribute())
    {
        // 允许Buff/Debuff的有符号加减；安全上限只保护数值不溢出，不作用户可写权限。
        const float Limit = FMath::Max(
            0.0f, GetDefault<UGamePlatformCombatSettings>()->MaxDamageMagnitude);
        NewValue = FMath::Clamp(NewValue, -Limit, Limit);
    }
    else if (Attribute == GetMoveSpeedMultiplierAttribute())
    {
        // 所有独立减速GE的加法结果由GAS聚合；最大减速70%，即倍率不小于0.30。
        NewValue = FMath::Clamp(NewValue, 0.30f, 1.0f);
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
    SetMaxEnergy(FMath::Max(0.0f, GetMaxEnergy()));
    SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));
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

void UGamePlatformCombatAttributeSet::OnRep_Energy(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, Energy, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_MaxEnergy(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, MaxEnergy, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_DamageBonus(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, DamageBonus, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_MoveSpeedMultiplier(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, MoveSpeedMultiplier, OldValue);
}

void UGamePlatformCombatAttributeSet::OnRep_DamageReduction(
    const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformCombatAttributeSet, DamageReduction, OldValue);
}
