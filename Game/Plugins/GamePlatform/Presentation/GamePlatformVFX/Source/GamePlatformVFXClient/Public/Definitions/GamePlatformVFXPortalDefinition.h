#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXPortalDefinition.generated.h"

/** Portal（传送门）只表现Opening/Stable/Closing，不签发Ticket也不执行Travel。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXPortalDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXPortalDefinition();
};
