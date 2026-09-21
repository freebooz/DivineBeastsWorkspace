#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXProjectileDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXProjectileDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXProjectileDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Projectile;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.0"))
    float VisualSpeed = 1200.0f;
};
