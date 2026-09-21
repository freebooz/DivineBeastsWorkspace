#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformVFXTypes.h"

class FGamePlatformVFXScalabilityPolicy
{
public:
    static bool CanSpawn(EGamePlatformVFXImportance Importance, int32 ActiveInstances, int32 AmbientInstances);
};
