#pragma once

#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"

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
    TWeakObjectPtr<UNiagaraComponent> Component;
    TArray<FGamePlatformVFXHandle> Children;
    EGamePlatformVFXInstanceState State = EGamePlatformVFXInstanceState::Pending;
    bool bPooled = false;
};
