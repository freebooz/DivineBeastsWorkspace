#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformInteractionTypes.h"
#include "GamePlatformInteractionOption.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FName OptionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FGameplayTag InteractionTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    EGamePlatformInteractionMode Mode = EGamePlatformInteractionMode::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    int32 Priority = 0;

    /** 厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="0.0"))
    float MaxDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    bool bRequireLineOfSight = true;

    /** 仅Hold模式使用，秒。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="0.0"))
    float HoldDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    EGamePlatformInteractionConcurrencyPolicy ConcurrencyPolicy =
        EGamePlatformInteractionConcurrencyPolicy::Exclusive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="1"))
    int32 MaxConcurrent = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    FName PromptId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
    EGamePlatformInteractionCommitKind CommitKind =
        EGamePlatformInteractionCommitKind::Custom;

    bool IsStructurallyValid() const
    {
        return !OptionId.IsNone() &&
               FMath::IsFinite(MaxDistance) &&
               MaxDistance > 0.0f &&
               MaxConcurrent > 0 &&
               (Mode == EGamePlatformInteractionMode::Instant ||
                (FMath::IsFinite(HoldDuration) && HoldDuration > 0.0f));
    }
};
