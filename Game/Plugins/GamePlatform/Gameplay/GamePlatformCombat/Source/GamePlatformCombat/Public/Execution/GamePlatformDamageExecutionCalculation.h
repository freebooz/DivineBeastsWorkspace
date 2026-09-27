#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "GamePlatformDamageExecutionCalculation.generated.h"

UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformDamageExecutionCalculation final
    : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
