#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformInteractionSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Interaction"))
class GAMEPLATFORMINTERACTION_API UGamePlatformInteractionSettings final
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Discovery", meta=(ClampMin="0.02"))
    float FocusRefreshInterval = 0.10f;

    UPROPERTY(Config, EditAnywhere, Category="Sessions", meta=(ClampMin="0.05"))
    float HoldValidationInterval = 0.20f;

    UPROPERTY(Config, EditAnywhere, Category="Validation", meta=(ClampMin="1.0"))
    float MaxConfiguredInteractionDistance = 600.0f;

    UPROPERTY(Config, EditAnywhere, Category="Validation", meta=(ClampMin="0.0"))
    float MaxTraceOriginOffset = 250.0f;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="0.1"))
    float BeginRequestWindowSeconds = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="1"))
    int32 MaxBeginRequestsPerWindow = 8;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="1"))
    int32 MaxCancelRequestsPerWindow = 12;

    UPROPERTY(Config, EditAnywhere, Category="Dedup", meta=(ClampMin="8"))
    int32 MaxRecentRequests = 128;

    UPROPERTY(Config, EditAnywhere, Category="Dedup", meta=(ClampMin="1.0"))
    float RecentRequestLifetimeSeconds = 30.0f;

    UPROPERTY(Config, EditAnywhere, Category="RateLimit", meta=(ClampMin="0.0"))
    float MinClientRequestInterval = 0.08f;
};
