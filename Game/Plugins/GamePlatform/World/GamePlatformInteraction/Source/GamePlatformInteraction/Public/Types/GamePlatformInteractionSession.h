#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionResult.h"
#include "Types/GamePlatformInteractionTypes.h"
#include "GamePlatformInteractionSession.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid SessionId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> InteractorActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 InteractorGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid TargetInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetRevisionAtStart = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FName OptionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionMode Mode = EGamePlatformInteractionMode::Instant;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    double ServerStartTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    float RequiredDuration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionSessionState State =
        EGamePlatformInteractionSessionState::None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGamePlatformInteractionResult Result;
};
