#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformControlAttributeSet.generated.h"

#define GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** UGamePlatformControlAttributeSet（游戏平台控制与韧性属性集）。 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformControlAttributeSet final
    : public UGamePlatformAttributeSet
{
    GENERATED_BODY()

public:
    UGamePlatformControlAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    /** 0..1，表示控制持续时间抗性；具体控制公式由 Combat 策略解释。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Tenacity, Category="Combat|Control")
    FGameplayAttributeData Tenacity;
    GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(UGamePlatformControlAttributeSet, Tenacity)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Poise, Category="Combat|Control")
    FGameplayAttributeData Poise;
    GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(UGamePlatformControlAttributeSet, Poise)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxPoise, Category="Combat|Control")
    FGameplayAttributeData MaxPoise;
    GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(UGamePlatformControlAttributeSet, MaxPoise)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PoiseRegen, Category="Combat|Control")
    FGameplayAttributeData PoiseRegen;
    GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(UGamePlatformControlAttributeSet, PoiseRegen)

private:
    UFUNCTION() void OnRep_Tenacity(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Poise(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_PoiseRegen(const FGameplayAttributeData& OldValue);
};

#undef GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS
