#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXBeamDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXBeamDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXBeamDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Beam;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam")
    FName SourcePositionParameter = TEXT("User.VFX.SourcePosition");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam")
    FName TargetPositionParameter = TEXT("User.VFX.TargetPosition");
};
