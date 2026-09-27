#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXAttachedDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXAttachedDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXAttachedDefinition();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName DefaultAttachPoint = NAME_None;
};
