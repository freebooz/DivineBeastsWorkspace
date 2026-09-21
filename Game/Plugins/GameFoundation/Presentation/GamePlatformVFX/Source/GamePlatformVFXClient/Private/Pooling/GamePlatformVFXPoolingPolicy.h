#pragma once
#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Types/GamePlatformVFXTypes.h"

class FGamePlatformVFXPoolingPolicy
{
public:
    static ENCPoolMethod ToNiagaraPoolingMethod(EGamePlatformVFXPoolingMode Mode);
};
