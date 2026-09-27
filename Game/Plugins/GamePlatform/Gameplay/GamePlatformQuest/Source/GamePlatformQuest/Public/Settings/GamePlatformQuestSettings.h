#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformQuestSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform Quest"))
class GAMEPLATFORMQUEST_API UGamePlatformQuestSettings final
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxActiveQuests = 32;

    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16"))
    int32 MaxRecentEventIds = 256;

    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16"))
    int32 MaxDeferredEventsPerPlayer = 512;

    UPROPERTY(Config, EditAnywhere, Category="Persistence", meta=(ClampMin="0.1"))
    float ProgressFlushDelaySeconds = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="Client", meta=(ClampMin="1"))
    int32 ClientMaxTrackedQuests = 5;
};
