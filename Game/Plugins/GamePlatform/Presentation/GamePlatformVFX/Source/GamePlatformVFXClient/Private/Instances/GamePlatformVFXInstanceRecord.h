#pragma once

#include "Types/GamePlatformVFXHandle.h"

class UNiagaraComponent;
class UGamePlatformVFXDefinition;

enum class EGamePlatformVFXInstanceState : uint8
{
    Pending,
    Active,
    Stopping
};

/**
 * 单个VFX运行实例的最小状态。
 * 不保存完整Request或DefinitionPath，避免高并发效果为上下文Tag和参数Map长期保留重复副本。
 */
struct FGamePlatformVFXInstanceRecord
{
    FGamePlatformVFXHandle Handle;
    TWeakObjectPtr<UGamePlatformVFXDefinition> Definition;
    TWeakObjectPtr<UNiagaraComponent> Component;
    TArray<FGamePlatformVFXHandle> Children;
    EGamePlatformVFXInstanceState State = EGamePlatformVFXInstanceState::Pending;
    bool bPooled = false;
};
