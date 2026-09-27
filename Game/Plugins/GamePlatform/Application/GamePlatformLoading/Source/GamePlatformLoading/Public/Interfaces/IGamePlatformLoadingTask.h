#pragma once
#include "Types/GamePlatformLoadingTypes.h"
class UGameInstance;

/** 任务轮询是游戏线程、有限频率采样；不是每帧插值。异步提供者需把完成结果切回游戏线程。 */
enum class EGamePlatformLoadingTaskUpdate : uint8 { Pending, Succeeded, Failed };
struct FGamePlatformLoadingTaskUpdate
{
    EGamePlatformLoadingTaskUpdate State = EGamePlatformLoadingTaskUpdate::Pending;
    double Progress01 = 0;
    FName Error;
};

/** 每次尝试独立实例，不可跨GameInstance复用；实现不能回调重入Loading的变更接口。 */
class GAMEPLATFORMLOADING_API IGamePlatformLoadingTask
{
public:
    virtual ~IGamePlatformLoadingTask() = default;
    /** 游戏线程一次启动；失败由返回结果终结，不能固定延时伪造成功。 */
    virtual FGamePlatformResult Start(UGameInstance& Instance, const FGamePlatformLoadingTaskSpec& Spec) = 0;
    /** 游戏线程只读当前事实；必须非阻塞。终态只会接纳一次。 */
    virtual FGamePlatformLoadingTaskUpdate Poll() = 0;
    /** 成功后的持续使用屏障，仅游戏线程采样且不得重入Loading变更接口。
     * 默认适用于完成后保持有效的资源任务；世界贡献者必须重查世界代次与现状。
     * false不重发操作终态，但IsReadyToPlay将拒绝已失效的完成快照。
     */
    virtual bool IsReadyToUse() const { return true; }
    /** 游戏线程停止异步请求/解绑/释放自身资源；每个已创建任务清理一次，必须可处理Start失败。 */
    virtual void Release() = 0;
};
/** 工厂返回全新的独占任务；已登记工厂不得包含其他实例的强对象。空任务将产生真实失败。 */
using FGamePlatformLoadingTaskFactory = TFunction<TUniquePtr<IGamePlatformLoadingTask>()>;
