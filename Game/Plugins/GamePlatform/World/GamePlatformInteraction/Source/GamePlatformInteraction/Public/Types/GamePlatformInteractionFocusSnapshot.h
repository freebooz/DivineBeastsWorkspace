#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionOption.h"
#include "GamePlatformInteractionFocusSnapshot.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionFocusSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid TargetInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGamePlatformInteractionOption Option;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    float Distance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    bool bLocallyAvailable = false;

    bool IsValid() const
    {
        return ::IsValid(TargetActor) &&
               TargetInstanceId.IsValid() &&
               !Option.OptionId.IsNone();
    }
};
