#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsMomentumDefinition.generated.h"

/**
 * FDivineBeastsMomentumDefinition（神兽联盟气势定义）。
 * 只描述项目层数据驱动规则，不保存运行时真值；运行时真值归 GAS AttributeSet。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsMomentumDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum", meta=(ClampMin="0.0"))
    float InitialMomentum = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum", meta=(ClampMin="0.0"))
    float MaxMomentum = 100.0f;

    /** 旧资产兼容字段：当前未实现自动气势获得倍率，数值不再注册为GAS属性或复制。
     * 保留序列化身份是为了不破坏旧HeroDefinition（英雄定义）资产；正式规则激活前不消费该值。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum|Legacy", meta=(ClampMin="0.0", DeprecatedProperty, DeprecationMessage="当前版本没有气势倍率结算，本字段仅用于旧数据兼容"))
    float GainMultiplier = 1.0f;

    /** 旧资产兼容字段：当前未实现气势按秒衰减，保留定义结构原有序列化字段。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum|Legacy", meta=(ClampMin="0.0", DeprecatedProperty, DeprecationMessage="当前版本没有气势自动衰减，本字段仅用于旧数据兼容"))
    float DecayRate = 0.0f;

    bool IsValid(FString& OutError) const;
};
