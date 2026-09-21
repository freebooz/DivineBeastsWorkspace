#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXAttachedDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXAttachedDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXAttachedDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Attached;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attached")
    FName DefaultAttachSocket = NAME_None;
};
