#include "Effects/DivineBeastsMomentumGainGameplayEffect.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "GameplayEffectTypes.h"

FName UDivineBeastsMomentumGainGameplayEffect::GetMagnitudeDataName()
{
    return FName(TEXT("SSLM.Momentum.BasicHitGain"));
}

UDivineBeastsMomentumGainGameplayEffect::UDivineBeastsMomentumGainGameplayEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UDivineBeastsMomentumAttributeSet::GetMomentumAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = GetMagnitudeDataName();
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
