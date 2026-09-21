#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXShieldDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXShieldDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXShieldDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Shield;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shield")
    FName HitPulseParameter = TEXT("User.VFX.HitPulse");
};
