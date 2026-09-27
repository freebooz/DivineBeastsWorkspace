#pragma once
#include "Types/GamePlatformSpawnRequest.h"

/** 服务器出生候选提供者；不负责生成、认证或控制。游戏线程同步有界调用，不得重入模式写接口。 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformSpawnPolicy
{
public:
    virtual ~IGamePlatformSpawnPolicy() = default;
    /** 输出只读候选；未就绪返回NotExecuted，失败给稳定码。候选仍由GameMode复核和占位。 */
    virtual FGamePlatformResult CollectCandidates(const FGamePlatformSpawnRequest& Request,
        TArray<FGamePlatformSpawnCandidate>& OutCandidates) = 0;
};
