#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXAreaDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXAreaDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXAreaDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Area;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="0.0"))
    float VisualRadius = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area")
    FName RadiusParameter = TEXT("User.VFX.TargetRadius");
};
