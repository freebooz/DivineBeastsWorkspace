#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXInstantDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXInstantDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXInstantDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Instant;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Instant")
    float ExpectedLifetimeSeconds = 1.0f;
};
