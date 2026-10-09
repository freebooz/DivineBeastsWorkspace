#include "Attributes/GamePlatformOffenseAttributeSet.h"

#include "Net/UnrealNetwork.h"

UGamePlatformOffenseAttributeSet::UGamePlatformOffenseAttributeSet()
{
    InitAttackPower(0.0f);
    InitAbilityPower(0.0f);
    InitCriticalChance(0.0f);
    InitCriticalDamage(1.5f);
    InitArmorPenetration(0.0f);
    InitMagicPenetration(0.0f);
}

void UGamePlatformOffenseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
// 攻击内部数值只向拥有者同步；GAS预测修正保持REPNOTIFY_Always（始终通知）。
#define GP_REP_OFFENSE(Name) DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformOffenseAttributeSet, Name, COND_OwnerOnly, REPNOTIFY_Always)
    GP_REP_OFFENSE(AttackPower);
    GP_REP_OFFENSE(AbilityPower);
    GP_REP_OFFENSE(CriticalChance);
    GP_REP_OFFENSE(CriticalDamage);
    GP_REP_OFFENSE(ArmorPenetration);
    GP_REP_OFFENSE(MagicPenetration);
#undef GP_REP_OFFENSE
}

void UGamePlatformOffenseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (!FMath::IsFinite(NewValue))
    {
        NewValue = 0.0f;
    }

    if (Attribute == GetCriticalChanceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
    else if (Attribute == GetAttackPowerAttribute() ||
             Attribute == GetAbilityPowerAttribute() ||
             Attribute == GetCriticalDamageAttribute() ||
             Attribute == GetArmorPenetrationAttribute() ||
             Attribute == GetMagicPenetrationAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

#define GP_OFFENSE_REPNOTIFY(Name) \
void UGamePlatformOffenseAttributeSet::OnRep_##Name(const FGameplayAttributeData& OldValue) \
{ GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformOffenseAttributeSet, Name, OldValue); }

GP_OFFENSE_REPNOTIFY(AttackPower)
GP_OFFENSE_REPNOTIFY(AbilityPower)
GP_OFFENSE_REPNOTIFY(CriticalChance)
GP_OFFENSE_REPNOTIFY(CriticalDamage)
GP_OFFENSE_REPNOTIFY(ArmorPenetration)
GP_OFFENSE_REPNOTIFY(MagicPenetration)
#undef GP_OFFENSE_REPNOTIFY
