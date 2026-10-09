#include "Attributes/GamePlatformDefenseAttributeSet.h"

#include "Net/UnrealNetwork.h"

UGamePlatformDefenseAttributeSet::UGamePlatformDefenseAttributeSet()
{
    InitArmor(0.0f);
    InitMagicResistance(0.0f);
    InitDamageReduction(0.0f);
}

void UGamePlatformDefenseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 只对拥有者同步三项战斗内参；其他观察者仅显示正式授权的战斗快照。
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformDefenseAttributeSet, Armor, COND_OwnerOnly, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformDefenseAttributeSet, MagicResistance, COND_OwnerOnly, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGamePlatformDefenseAttributeSet, DamageReduction, COND_OwnerOnly, REPNOTIFY_Always);
}

void UGamePlatformDefenseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (!FMath::IsFinite(NewValue))
    {
        NewValue = 0.0f;
    }

    if (Attribute == GetDamageReductionAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
    else if (Attribute == GetArmorAttribute() || Attribute == GetMagicResistanceAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UGamePlatformDefenseAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformDefenseAttributeSet, Armor, OldValue);
}

void UGamePlatformDefenseAttributeSet::OnRep_MagicResistance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformDefenseAttributeSet, MagicResistance, OldValue);
}

void UGamePlatformDefenseAttributeSet::OnRep_DamageReduction(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGamePlatformDefenseAttributeSet, DamageReduction, OldValue);
}
