#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformDefenseAttributeSet.generated.h"

#define GAMEPLATFORM_DEFENSE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformDefenseAttributeSet（游戏平台防御属性集）。
 * 三项抵抗/减伤数值仅向拥有者复制；其他客户端通过授权战斗结果观察。
 * 不向未获准观察的敌人或观战者暴露精确防御数值。
 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformDefenseAttributeSet final
    : public UGamePlatformAttributeSet
{
    GENERATED_BODY()

public:
    UGamePlatformDefenseAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Combat|Defense")
    FGameplayAttributeData Armor;
    GAMEPLATFORM_DEFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformDefenseAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicResistance, Category="Combat|Defense")
    FGameplayAttributeData MagicResistance;
    GAMEPLATFORM_DEFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformDefenseAttributeSet, MagicResistance)

    /** 0..1 的通用最终减伤系数；具体叠加公式由 Combat 结算策略解释。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageReduction, Category="Combat|Defense")
    FGameplayAttributeData DamageReduction;
    GAMEPLATFORM_DEFENSE_ATTRIBUTE_ACCESSORS(UGamePlatformDefenseAttributeSet, DamageReduction)

private:
    UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MagicResistance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);
};

#undef GAMEPLATFORM_DEFENSE_ATTRIBUTE_ACCESSORS
