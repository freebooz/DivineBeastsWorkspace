#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXAreaDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXAreaDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXAreaDefinition();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName RadiusParameterName = TEXT("User.Radius");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(ClampMin="0.0"))
    float DefaultRadius = 100.0f;
};
