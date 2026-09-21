#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXPortalDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXPortalDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXPortalDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Portal;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
    FName ProgressParameter = TEXT("User.VFX.Progress01");
};
