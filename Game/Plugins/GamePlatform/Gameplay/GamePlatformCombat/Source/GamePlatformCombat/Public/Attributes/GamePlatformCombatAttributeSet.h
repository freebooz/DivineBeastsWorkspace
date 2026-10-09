#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformCombatAttributeSet.generated.h"

#define GAMEPLATFORM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformCombatAttributeSet（平台统一战斗属性集）。
 * 唯一基础战斗数值：Health/MaxHealth（生命及上限），DamageBonus（伤害增减）
 * 与DamageReduction（伤害减免）；后两项支持GAS增减益效果使用加法聚合。
 * Shield/MaxShield（旧护盾字段）已删除：护盾改由限时GameplayEffect（玩法效果）
 * 与战斗组件持有的临时吸收池结算，不再维护或复制护盾永久属性。
 * IncomingDamage/IncomingHealing（伤害/治疗元属性）不复制、消费后清零。
 * 生命值按Actor网络相关性公开；伤害修正仅拥有者可见。
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

    /** 技能基础伤害的有符号加法修正；正数Buff增强，负数Debuff削弱。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageBonus, Category="Combat|Modifiers")
    FGameplayAttributeData DamageBonus;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, DamageBonus)

    /** 受到伤害的有符号加法减免；正数Buff减伤，负数Debuff使目标易伤。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageReduction, Category="Combat|Modifiers")
    FGameplayAttributeData DamageReduction;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, DamageReduction)

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
    void OnRep_DamageBonus(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);
};
