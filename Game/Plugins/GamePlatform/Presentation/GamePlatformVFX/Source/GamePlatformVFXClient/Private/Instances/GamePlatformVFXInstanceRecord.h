#pragma once

#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXPreloadHandle.h"

class UNiagaraComponent;
class UGamePlatformVFXDefinition;

enum class EGamePlatformVFXInstanceState : uint8
{
    Pending,
    Active,
    Stopping
};

struct FGamePlatformVFXInstanceRecord
{
    FGamePlatformVFXHandle Handle;
    FGamePlatformVFXRequest Request;
    TWeakObjectPtr<UGamePlatformVFXDefinition> Definition;
    FSoftObjectPath DefinitionPath;
    FGamePlatformVFXPreloadHandle LoadLease;
    double StartedAtSeconds = 0.0;
    float MaxLifetimeSeconds = 0.0f;
    TWeakObjectPtr<UNiagaraComponent> Component;
    TArray<FGamePlatformVFXHandle> Children;
    EGamePlatformVFXInstanceState State = EGamePlatformVFXInstanceState::Pending;
    bool bPooled = false;
};
