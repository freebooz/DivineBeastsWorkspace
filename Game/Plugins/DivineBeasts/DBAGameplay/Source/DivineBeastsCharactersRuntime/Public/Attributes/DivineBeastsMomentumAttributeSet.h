#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "DivineBeastsMomentumAttributeSet.generated.h"

struct FDivineBeastsMomentumDefinition;

#define DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** UDivineBeastsMomentumAttributeSet（神兽联盟气势属性集）。 */
UCLASS()
class DIVINEBEASTSCHARACTERSRUNTIME_API UDivineBeastsMomentumAttributeSet final
    : public UGamePlatformAttributeSet
{
    GENERATED_BODY()

public:
    UDivineBeastsMomentumAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    /** 仅用于服务器/权威初始化；后续变化统一通过 GameplayEffect 修改。 */
    void InitializeFromDefinition(const FDivineBeastsMomentumDefinition& Definition);

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Momentum, Category="DivineBeasts|Momentum")
    FGameplayAttributeData Momentum;
    DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS(UDivineBeastsMomentumAttributeSet, Momentum)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMomentum, Category="DivineBeasts|Momentum")
    FGameplayAttributeData MaxMomentum;
    DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS(UDivineBeastsMomentumAttributeSet, MaxMomentum)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MomentumGainMultiplier, Category="DivineBeasts|Momentum")
    FGameplayAttributeData MomentumGainMultiplier;
    DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS(UDivineBeastsMomentumAttributeSet, MomentumGainMultiplier)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MomentumDecayRate, Category="DivineBeasts|Momentum")
    FGameplayAttributeData MomentumDecayRate;
    DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS(UDivineBeastsMomentumAttributeSet, MomentumDecayRate)

private:
    UFUNCTION() void OnRep_Momentum(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxMomentum(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MomentumGainMultiplier(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MomentumDecayRate(const FGameplayAttributeData& OldValue);
};

#undef DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS
