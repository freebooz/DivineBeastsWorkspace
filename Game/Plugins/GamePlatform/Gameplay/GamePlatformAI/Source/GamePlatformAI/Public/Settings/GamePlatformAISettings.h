#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformAISettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform AI"))
class GAMEPLATFORMAI_API UGamePlatformAISettings final
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="0.0"))
    float MaxSightRadius = 8000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="0.0"))
    float MaxLoseSightRadius = 10000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="0.0"))
    float MaxPerceptionAge = 30.0f;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="0.01"))
    float MinDecisionInterval = 0.05f;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxTargetCandidates = 32;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="0.0"))
    float MaxLeashRadius = 10000.0f;
};
