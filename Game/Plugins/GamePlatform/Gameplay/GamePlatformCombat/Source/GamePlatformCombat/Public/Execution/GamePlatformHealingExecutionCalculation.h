#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "GamePlatformHealingExecutionCalculation.generated.h"

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformHealingExecutionCalculation final
    : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
