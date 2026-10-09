#include "Execution/GamePlatformDamageExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
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

    // 由GAS属性聚合器读取来源/目标当前值：已包含持续Buff/Debuff的加法效果。
    if (SourceASC)
    {
        if (const UGamePlatformCombatAttributeSet* Source =
                SourceASC->GetSet<UGamePlatformCombatAttributeSet>())
        {
            Formula.DamageBonus = Source->GetDamageBonus();
        }
    }

    if (TargetASC)
    {
        if (const UGamePlatformCombatAttributeSet* Target =
                TargetASC->GetSet<UGamePlatformCombatAttributeSet>())
        {
            Formula.DamageReduction = Target->GetDamageReduction();
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
