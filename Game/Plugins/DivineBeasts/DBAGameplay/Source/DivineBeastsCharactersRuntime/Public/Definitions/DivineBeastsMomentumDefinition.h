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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum", meta=(ClampMin="0.0"))
    float GainMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Momentum", meta=(ClampMin="0.0"))
    float DecayRate = 0.0f;

    bool IsValid(FString& OutError) const;
};
