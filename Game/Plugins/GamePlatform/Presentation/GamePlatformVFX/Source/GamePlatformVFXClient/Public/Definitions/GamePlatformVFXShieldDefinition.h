#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXShieldDefinition.generated.h"

/** Shield（护盾）只表现已存在的护盾事实，不维护第二套数值状态。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXShieldDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXShieldDefinition();
};
