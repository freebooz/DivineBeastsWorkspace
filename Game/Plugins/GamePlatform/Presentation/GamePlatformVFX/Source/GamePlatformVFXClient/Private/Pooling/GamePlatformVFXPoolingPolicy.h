#pragma once

#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXDefinition;
class UGamePlatformVFXSettings;

class FGamePlatformVFXPoolingPolicy
{
public:
    static bool ShouldUseNiagaraPool(
        const UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const UGamePlatformVFXSettings& Settings);
};
