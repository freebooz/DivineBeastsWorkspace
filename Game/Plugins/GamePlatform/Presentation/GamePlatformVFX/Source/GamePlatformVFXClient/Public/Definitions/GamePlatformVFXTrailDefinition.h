#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXTrailDefinition.generated.h"

/** Trail（拖尾）只处理视觉附着与生命周期，不拥有攻击窗口。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXTrailDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXTrailDefinition();
};
