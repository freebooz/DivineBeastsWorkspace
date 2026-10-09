#pragma once

#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXDefinition;
class UNiagaraComponent;
class UWorld;

/** Niagara 的唯一具体执行入口。 */
class FGamePlatformVFXNiagaraExecutor
{
public:
    /** 游戏线程检查弱目标、Owner与World；附着定义要求有效目标，失效目标不能退化为落地播放。 */
    static bool IsAttachmentValid(const UWorld& World, const UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXSpawnContext& Spawn);

    static UNiagaraComponent* Spawn(
        UWorld& World,
        const UGamePlatformVFXDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        bool bUsePool);

private:
    static void ApplyParameters(UNiagaraComponent& Component, const FGamePlatformVFXParameters& Parameters);
};
