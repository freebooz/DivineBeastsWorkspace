#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "GamePlatformVFXProjectileDefinition.generated.h"

/** Projectile（投射物）仅负责视觉轨迹，不执行Trace、碰撞或伤害。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXProjectileDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXProjectileDefinition();
};
