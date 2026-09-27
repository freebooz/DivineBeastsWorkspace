#pragma once

#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXDefinition;
class UNiagaraComponent;
class UWorld;

/** Niagara 的唯一具体执行入口。 */
class FGamePlatformVFXNiagaraExecutor
{
public:
    static UNiagaraComponent* Spawn(
        UWorld& World,
        const UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        bool bUsePool);

private:
    static void ApplyParameters(UNiagaraComponent& Component, const FGamePlatformVFXParameters& Parameters);
};
