#include "Attributes/DivineBeastsMomentumAttributeSet.h"

#include "Definitions/DivineBeastsMomentumDefinition.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UDivineBeastsMomentumAttributeSet::UDivineBeastsMomentumAttributeSet()
{
    InitMomentum(0.0f);
    InitMaxMomentum(100.0f);
    InitMomentumGainMultiplier(1.0f);
    InitMomentumDecayRate(0.0f);
}

void UDivineBeastsMomentumAttributeSet::InitializeFromDefinition(const FDivineBeastsMomentumDefinition& Definition)
{
    FString Error;
    if (!Definition.IsValid(Error))
    {
        return;
    }
    InitMaxMomentum(Definition.MaxMomentum);
    InitMomentum(FMath::Clamp(Definition.InitialMomentum, 0.0f, Definition.MaxMomentum));
    InitMomentumGainMultiplier(Definition.GainMultiplier);
    InitMomentumDecayRate(Definition.DecayRate);
}

void UDivineBeastsMomentumAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UDivineBeastsMomentumAttributeSet, Momentum, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UDivineBeastsMomentumAttributeSet, MaxMomentum, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UDivineBeastsMomentumAttributeSet, MomentumGainMultiplier, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UDivineBeastsMomentumAttributeSet, MomentumDecayRate, COND_None, REPNOTIFY_Always);
}

void UDivineBeastsMomentumAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (!FMath::IsFinite(NewValue))
    {
        NewValue = 0.0f;
    }

    if (Attribute == GetMomentumAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxMomentum()));
    }
    else if (Attribute == GetMaxMomentumAttribute() ||
             Attribute == GetMomentumGainMultiplierAttribute() ||
             Attribute == GetMomentumDecayRateAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UDivineBeastsMomentumAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    SetMaxMomentum(FMath::Max(0.0f, GetMaxMomentum()));
    SetMomentum(FMath::Clamp(GetMomentum(), 0.0f, GetMaxMomentum()));
    SetMomentumGainMultiplier(FMath::Max(0.0f, GetMomentumGainMultiplier()));
    SetMomentumDecayRate(FMath::Max(0.0f, GetMomentumDecayRate()));
}

void UDivineBeastsMomentumAttributeSet::OnRep_Momentum(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UDivineBeastsMomentumAttributeSet, Momentum, OldValue);
}

void UDivineBeastsMomentumAttributeSet::OnRep_MaxMomentum(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UDivineBeastsMomentumAttributeSet, MaxMomentum, OldValue);
}

void UDivineBeastsMomentumAttributeSet::OnRep_MomentumGainMultiplier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UDivineBeastsMomentumAttributeSet, MomentumGainMultiplier, OldValue);
}

void UDivineBeastsMomentumAttributeSet::OnRep_MomentumDecayRate(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UDivineBeastsMomentumAttributeSet, MomentumDecayRate, OldValue);
}
