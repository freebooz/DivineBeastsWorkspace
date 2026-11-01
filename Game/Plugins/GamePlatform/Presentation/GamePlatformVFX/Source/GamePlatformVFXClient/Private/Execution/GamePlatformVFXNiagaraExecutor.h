#pragma once

#include "Types/GamePlatformVFXRequest.h"

class UGamePlatformVFXDefinition;
class UNiagaraComponent;
class UWorld;
struct FGamePlatformMotionSnapshot;

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
    /** 只写System中类型一致且已声明的六个运动User参数；返回写入数，无效样本返回0。 */
    static int32 ApplyMotionParameters(UNiagaraComponent& Component,const FGamePlatformMotionSnapshot& Snapshot);
    /** 从当前附着源的中立接口读取；不移动Actor，不访问伤害或项目技能。 */
    static bool UpdateProjectileMotion(UNiagaraComponent& Component,TFunctionRef<bool()> StillOwned);

private:
    static void ApplyParameters(UNiagaraComponent& Component, const FGamePlatformVFXParameters& Parameters);
};
