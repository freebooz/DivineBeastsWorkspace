#pragma once
#include "Types/GamePlatformWorldContext.h"
/** 扩展就绪贡献者只采样事实；游戏线程调用，不允许重入World修改接口。 */
class GAMEPLATFORMWORLD_API IGamePlatformWorldReadinessContributor
{
public:
    virtual ~IGamePlatformWorldReadinessContributor() = default;
    /** NotExecuted表示等待，Success成功，Failure失败；不得用固定时间冒充就绪。 */
    virtual FGamePlatformResult Evaluate(const FGamePlatformWorldContext& Context) const = 0;
};
