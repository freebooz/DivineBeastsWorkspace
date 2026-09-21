#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXTrailDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXTrailDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXTrailDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Trail;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trail", meta=(ClampMin="0.0"))
    float DefaultWidth = 16.0f;
};
