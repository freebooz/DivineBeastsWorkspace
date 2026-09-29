#pragma once

#include "CoreMinimal.h"
#include "Initialization/GamePlatformCharacterInitializer.h"

class ACharacter;

/**
 * FGamePlatformCharacterInitializationExecutor（平台角色初始化执行器）。
 *
 * 统一 Spawn/Respawn 流程只通过本执行器调用项目 Character Initializer：
 * - 正式组合根必须恰好注册一个初始化器；
 * - 0 个或多个实现一律 Fail Closed；
 * - 不负责 SpawnActor / Possess，也不持有项目类型。
 */
struct GAMEPLATFORMCHARACTER_API FGamePlatformCharacterInitializationExecutor
{
    /** 解析当前进程唯一角色初始化器；失败返回 nullptr 并给出中文诊断。 */
    static IGamePlatformCharacterInitializer* ResolveUniqueInitializer(FString& OutError);

    /** 对已创建的 ACharacter 执行项目初始化；调用方仍负责 Pawn 创建、控制和 Gameplay Active 门禁。 */
    static bool InitializeCharacter(
        ACharacter& Character,
        const FGamePlatformCharacterInitializationContext& Context,
        FString& OutError);
};
