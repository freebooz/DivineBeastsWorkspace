#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformNavigationSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Navigation"))
class GAMEPLATFORMNAVIGATION_API UGamePlatformNavigationSettings final
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="1.0"))
    float MaxProjectionExtent = 5000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Safety", meta=(ClampMin="1.0"))
    float MaxPathDistance = 100000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Async", meta=(ClampMin="0.1"))
    float DefaultRequestTimeoutSeconds = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category="Async", meta=(ClampMin="1"))
    int32 MaxOutstandingAsyncRequests = 256;

    UPROPERTY(Config, EditAnywhere, Category="Invoker", meta=(ClampMin="1.0"))
    float MaxInvokerGenerationRadius = 10000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Invoker", meta=(ClampMin="1.0"))
    float MaxInvokerRemovalRadius = 15000.0f;

    UPROPERTY(Config, EditAnywhere, Category="Area", meta=(ClampMin="1.0"))
    float HighCostAreaMultiplier = 5.0f;
};
