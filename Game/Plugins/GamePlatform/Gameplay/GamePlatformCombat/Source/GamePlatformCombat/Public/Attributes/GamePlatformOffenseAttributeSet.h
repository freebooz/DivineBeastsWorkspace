#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformOffenseAttributeSet.generated.h"

#define GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformOffenseAttributeSet（游戏平台攻击属性集）。
 * 只持有六项被服务器伤害公式实际消费的攻击数值。
 * AttackSpeed（攻击速度）尚无运行消费端，不作为当前GAS属性。
 * 内部计算值只向拥有者同步，不向所有相关客户端公开攻击、暴击和穿透配置。
 * 不认识生肖、MOBA模式或项目技能名称。
 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformOffenseAttributeSet final
    : public UGamePlatformAttributeSet
{
    GENERATED_BODY()

public:
    UGamePlatformOffenseAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackPower, Category="Combat|Offense")
    FGameplayAttributeData AttackPower;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AbilityPower, Category="Combat|Offense")
    FGameplayAttributeData AbilityPower;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, AbilityPower)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalChance, Category="Combat|Offense")
    FGameplayAttributeData CriticalChance;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, CriticalChance)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalDamage, Category="Combat|Offense")
    FGameplayAttributeData CriticalDamage;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, CriticalDamage)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ArmorPenetration, Category="Combat|Offense")
    FGameplayAttributeData ArmorPenetration;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, ArmorPenetration)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicPenetration, Category="Combat|Offense")
    FGameplayAttributeData MagicPenetration;
    GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformOffenseAttributeSet, MagicPenetration)

private:
    UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AbilityPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_CriticalDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MagicPenetration(const FGameplayAttributeData& OldValue);
};

#undef GAMEPLATFORM_OFFENSE_ATTRIBUTE_ACCESSORS
