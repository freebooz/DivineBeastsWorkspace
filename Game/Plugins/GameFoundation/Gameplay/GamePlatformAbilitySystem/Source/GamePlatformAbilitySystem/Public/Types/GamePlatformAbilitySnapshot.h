#pragma once
#include "Types/GamePlatformAbilitySetGrantHandle.h"
#include "Types/GamePlatformResult.h"

/** 操作终态与资源期限分离；Applied后仍持有类资源，直到显式撤销／ASC关闭。 */
enum class EGamePlatformAbilityGrantState : uint8 { Invalid, Loading, Applied, Revoking, Released, Failed };
/** 本机诊断副本，未网络广播，不暴露账号、服务凭据或其他实例私有授权配置。 */
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityGrantSnapshot
{
    FGamePlatformAbilitySetGrantHandle Handle;
    EGamePlatformAbilityGrantState State = EGamePlatformAbilityGrantState::Invalid;
    FGamePlatformResult Result;
    int32 AbilityCount = 0;
    int32 ActiveEffectCount = 0;
};
/** 本端ASC诊断；统计是逻辑数量，未测量网络带宽或物理内存。 */
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilitySnapshot
{
    FGuid ScopeId;
    int64 AvatarGeneration = 0;
    bool bActorInfoValid = false;
    bool bGameplayAllowed = false;
    int32 Grants = 0;
    int32 RetainedAttributeSets = 0;
    FGamePlatformResult LastResult;
};
