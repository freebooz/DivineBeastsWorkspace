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

/**
 * UDivineBeastsMomentumAttributeSet（神兽联盟气势属性集）。
 * 仅保留Momentum（当前气势）和MaxMomentum（气势上限）两项运行时属性。
 * 气势倍率/衰减不再作为复制的GAS属性；旧HeroDefinition字段暂保留用于资源兼容。
 * 气势仅向拥有者复制；其他观察者仅展示有权限的项目级只读投影。
 */
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

private:
    UFUNCTION() void OnRep_Momentum(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxMomentum(const FGameplayAttributeData& OldValue);
};

#undef DIVINEBEASTS_MOMENTUM_ATTRIBUTE_ACCESSORS
