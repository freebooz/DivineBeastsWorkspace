#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionResult.h"
#include "Types/GamePlatformInteractionTypes.h"
#include "GamePlatformInteractionEvent.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionEventType EventType =
        EGamePlatformInteractionEventType::InteractionStarted;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid SessionId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> InteractorActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FName OptionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGamePlatformInteractionResult Result;
};
