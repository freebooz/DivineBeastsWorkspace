#include "Execution/GamePlatformDamageExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Attributes/GamePlatformDefenseAttributeSet.h"
#include "Attributes/GamePlatformOffenseAttributeSet.h"
#include "Settings/GamePlatformCombatSettings.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Types/GamePlatformCombatMath.h"

void UGamePlatformDamageExecutionCalculation::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    const float Requested = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Damage,
        false,
        0.0f);

    if (!FMath::IsFinite(Requested) || Requested <= 0.0f)
    {
        return;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();

    FGamePlatformDamageFormulaInput Formula;
    Formula.BaseDamage = Requested;
    Formula.AttackPowerCoefficient = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Damage_AttackPowerCoefficient,
        false,
        0.0f);
    Formula.AbilityPowerCoefficient = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Damage_AbilityPowerCoefficient,
        false,
        0.0f);
    Formula.bCanCritical = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Damage_CanCritical,
        false,
        0.0f) > 0.5f;
    Formula.CriticalRoll = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Damage_CriticalRoll,
        false,
        1.0f);
    Formula.DefenseMitigationConstant = Settings->DefenseMitigationConstant;

    const int32 DamageTypeValue = FMath::Clamp(
        FMath::RoundToInt(Spec.GetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_Type,
            false,
            static_cast<float>(EGamePlatformDamageType::Untyped))),
        static_cast<int32>(EGamePlatformDamageType::Untyped),
        static_cast<int32>(EGamePlatformDamageType::TrueDamage));
    Formula.DamageType = static_cast<EGamePlatformDamageType>(DamageTypeValue);

    const UAbilitySystemComponent* SourceASC =
        ExecutionParams.GetSourceAbilitySystemComponent();
    const UAbilitySystemComponent* TargetASC =
        ExecutionParams.GetTargetAbilitySystemComponent();

    if (SourceASC)
    {
        if (const UGamePlatformOffenseAttributeSet* Offense =
                SourceASC->GetSet<UGamePlatformOffenseAttributeSet>())
        {
            Formula.AttackPower = Offense->GetAttackPower();
            Formula.AbilityPower = Offense->GetAbilityPower();
            Formula.ArmorPenetration = Offense->GetArmorPenetration();
            Formula.MagicPenetration = Offense->GetMagicPenetration();
            Formula.CriticalChance = Offense->GetCriticalChance();
            Formula.CriticalDamage = Offense->GetCriticalDamage();
        }
    }

    if (TargetASC)
    {
        if (const UGamePlatformDefenseAttributeSet* Defense =
                TargetASC->GetSet<UGamePlatformDefenseAttributeSet>())
        {
            Formula.Armor = Defense->GetArmor();
            Formula.MagicResistance = Defense->GetMagicResistance();
            Formula.DamageReduction = Defense->GetDamageReduction();
        }
    }

    const FGamePlatformDamageFormulaOutput FormulaResult =
        FGamePlatformCombatMath::CalculateDamageMagnitude(Formula);
    const float FinalDamage = FMath::Clamp(
        FormulaResult.FinalDamage,
        0.0f,
        FMath::Max(0.0f, Settings->MaxDamageMagnitude));

    if (FinalDamage <= 0.0f)
    {
        return;
    }

    OutExecutionOutput.AddOutputModifier(
        FGameplayModifierEvaluatedData(
            UGamePlatformCombatAttributeSet::GetIncomingDamageAttribute(),
            EGameplayModOp::Additive,
            FinalDamage));
}
