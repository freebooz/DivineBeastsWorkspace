#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GamePlatformCombatAttributeSet.generated.h"

#define GAMEPLATFORM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** 正式战斗生命/护盾属性集；Incoming字段为不复制的结算元属性。 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UGamePlatformCombatAttributeSet();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void PreAttributeChange(
        const FGameplayAttribute& Attribute,
        float& NewValue) override;

    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Combat|Vitals")
    FGameplayAttributeData Health;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Combat|Vitals")
    FGameplayAttributeData MaxHealth;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Shield, Category="Combat|Vitals")
    FGameplayAttributeData Shield;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, Shield)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxShield, Category="Combat|Vitals")
    FGameplayAttributeData MaxShield;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, MaxShield)

    UPROPERTY(BlueprintReadOnly, Category="Combat|Meta")
    FGameplayAttributeData IncomingDamage;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, IncomingDamage)

    UPROPERTY(BlueprintReadOnly, Category="Combat|Meta")
    FGameplayAttributeData IncomingHealing;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, IncomingHealing)

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Shield(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxShield(const FGameplayAttributeData& OldValue);
};
