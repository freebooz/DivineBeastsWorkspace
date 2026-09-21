#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXWorldDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXWorldDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXWorldDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::World;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World", meta=(ClampMin="0.0"))
    float MaxRecommendedDistance = 5000.0f;
};
