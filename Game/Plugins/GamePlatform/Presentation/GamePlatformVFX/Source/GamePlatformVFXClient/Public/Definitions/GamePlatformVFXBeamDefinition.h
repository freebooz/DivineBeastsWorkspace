#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXBeamDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXBeamDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXBeamDefinition();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName SourceParameterName = TEXT("User.Source");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName TargetParameterName = TEXT("User.Target");
};
