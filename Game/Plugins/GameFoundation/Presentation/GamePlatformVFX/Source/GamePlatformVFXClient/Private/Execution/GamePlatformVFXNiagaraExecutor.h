#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXDefinition;
class UNiagaraComponent;
class UWorld;

class FGamePlatformVFXNiagaraExecutor
{
public:
    static UNiagaraComponent* Spawn(UWorld* World, const UGamePlatformVFXDefinition& Definition, const FGamePlatformVFXRequest& Request);
    static void ApplyParameters(UNiagaraComponent& Component, const FGamePlatformVFXParameters& Parameters);
};
