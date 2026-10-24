#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformCombatAttributeSet.generated.h"

#define GAMEPLATFORM_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)     GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)     GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformCombatAttributeSet（平台统一战斗属性集）。
 * 唯一具体平台战斗属性集：Health/MaxHealth（生命及上限）、Energy/MaxEnergy（独立能量及上限），
 * DamageBonus（伤害增减）与DamageReduction（伤害减免）支持GAS加法聚合；
 * MoveSpeedMultiplier（移动速度倍率）是原生GAS的短时效果聚合属性，默认1，最低0.3；
 * 角色运动消费者必须监听属性改变并按Hero原速应用到CharacterMovement，不以该数值作为独立速度身份。
 * Shield/MaxShield（旧护盾字段）已删除：护盾改由限时GameplayEffect（玩法效果）
 * 与战斗组件持有的临时吸收池结算，不再维护或复制护盾永久属性。
 * IncomingDamage/IncomingHealing（伤害/治疗元属性）不复制、消费后清零。
 * 生命值按Actor网络相关性公开；能量及伤害修正只向拥有者复制；气势另由SSLM项目层提供。
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

    /** 通用能量（蓝条资源），与项目气势Momentum分离；默认100/100，成本与恢复由权威GAS处理。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Energy, Category="Combat|Resources")
    FGameplayAttributeData Energy;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, Energy)

    /** 通用能量上限，拥有者可读取，服务端是唯一写入方。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxEnergy, Category="Combat|Resources")
    FGameplayAttributeData MaxEnergy;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, MaxEnergy)

    /** 技能基础伤害的有符号加法修正；正数Buff增强，负数Debuff削弱。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageBonus, Category="Combat|Modifiers")
    FGameplayAttributeData DamageBonus;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, DamageBonus)

    /** 受到伤害的有符号加法减免；正数Buff减伤，负数Debuff使目标易伤。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageReduction, Category="Combat|Modifiers")
    FGameplayAttributeData DamageReduction;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, DamageReduction)

    /** GE加法叠加的临时移动速度倍率，1=正常，0.7=减速30%；向所有相关客户端复制运动可见事实。
     * 主基础速度始终由角色定义拥有，不在平台属性集保存第二份。
     * GAS重算/过期后会触发OnRep与AttributeChange事件，消费者只在事件回调更新移动。 */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeedMultiplier, Category="Combat|Movement")
    FGameplayAttributeData MoveSpeedMultiplier;
    GAMEPLATFORM_ATTRIBUTE_ACCESSORS(UGamePlatformCombatAttributeSet, MoveSpeedMultiplier)

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
    void OnRep_Energy(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_DamageBonus(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_DamageReduction(const FGameplayAttributeData& OldValue);
};
