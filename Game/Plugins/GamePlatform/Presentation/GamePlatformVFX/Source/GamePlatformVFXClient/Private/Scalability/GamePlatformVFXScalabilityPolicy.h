#pragma once

#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXSettings;

class FGamePlatformVFXScalabilityPolicy
{
public:
    static bool ShouldSpawn(
        const FGamePlatformVFXRequest& Request,
        int32 ActiveInstances,
        const UGamePlatformVFXSettings& Settings);
};
