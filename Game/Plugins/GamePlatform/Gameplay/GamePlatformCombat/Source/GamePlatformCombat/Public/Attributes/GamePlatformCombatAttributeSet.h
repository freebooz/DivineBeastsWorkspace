#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformCombatAttributeSet.generated.h"

#define GAMEPLATFORM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformCombatAttributeSet（游戏平台生命/护盾与Meta结算属性集）。
 * 保留现有 Damage/Healing Execution（伤害/治疗执行）主链；攻击、防御、控制属性拆入同插件独立 AttributeSet。
 * Incoming 字段为不复制的瞬时结算元属性，不是长期角色状态。
 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatAttributeSet : public UGamePlatformAttributeSet
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
