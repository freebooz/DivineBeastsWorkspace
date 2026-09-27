#include "Execution/GamePlatformHealingExecutionCalculation.h"

#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Settings/GamePlatformCombatSettings.h"
#include "Tags/GamePlatformCombatTags.h"

void UGamePlatformHealingExecutionCalculation::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    const float Requested = Spec.GetSetByCallerMagnitude(
        GamePlatformCombatTags::Data_Healing,
        false,
        0.0f);

    if (!FMath::IsFinite(Requested) || Requested <= 0.0f)
    {
        return;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();
    const float FinalHealing = FMath::Clamp(
        Requested,
        0.0f,
        FMath::Max(0.0f, Settings->MaxHealingMagnitude));

    if (FinalHealing <= 0.0f)
    {
        return;
    }

    OutExecutionOutput.AddOutputModifier(
        FGameplayModifierEvaluatedData(
            UGamePlatformCombatAttributeSet::GetIncomingHealingAttribute(),
            EGameplayModOp::Additive,
            FinalHealing));
}
