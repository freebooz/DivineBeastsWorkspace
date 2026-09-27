#include "Execution/GamePlatformDamageExecutionCalculation.h"

#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Settings/GamePlatformCombatSettings.h"
#include "Tags/GamePlatformCombatTags.h"

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
    const float FinalDamage = FMath::Clamp(
        Requested,
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
