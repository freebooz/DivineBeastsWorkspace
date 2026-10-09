#pragma once

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "GamePlatformControlAttributeSet.generated.h"

#define GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UGamePlatformControlAttributeSet（游戏平台控制持续时间抗性属性集）。
 * 当前仅保留服务器控制结算实际使用的Tenacity（控制韧性）；
 * 未接通的Poise/MaxPoise/PoiseRegen（失衡值/上限/恢复）不再定义为GAS属性。
 * Tenacity仅向拥有者同步；眩晕/沉默由GameplayTag与GameplayEffect表达。
 */
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

private:
    UFUNCTION() void OnRep_Tenacity(const FGameplayAttributeData& OldValue);
};

#undef GAMEPLATFORM_CONTROL_ATTRIBUTE_ACCESSORS
