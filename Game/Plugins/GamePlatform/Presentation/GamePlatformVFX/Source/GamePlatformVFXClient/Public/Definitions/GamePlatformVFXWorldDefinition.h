#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXWorldDefinition.generated.h"

/** World（世界环境特效）适用于OpenWorld/Village等长生命周期环境表现。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXWorldDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXWorldDefinition();
};
