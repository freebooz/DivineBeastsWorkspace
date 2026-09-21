#pragma once

#include "Types/GamePlatformResult.h"
#include "Types/GamePlatformWorldStreaming.h"

class UWorld;

/**
 * 当前World的私有流送协调器，所有方法（含析构）仅游戏线程。
 * 只弱持有World/Owner/已有关卡；WP Provider由本对象持有并在销毁前精确注销。
 * 调用方须在发布就绪快照前Tick；不驱动World Tick，不创建加载器，不拥有世界生命周期。
 */
class FGamePlatformWorldStreaming final
{
public:
    /** 代次必须来自所属World上下文；只服务Game/PIE，不在Commandlet启动流送。 */
    FGamePlatformWorldStreaming(UWorld& World, FGuid ContextGeneration);
    ~FGamePlatformWorldStreaming();

    /** 接受返回新句柄与Success，初始状态Pending；拒绝无句柄且不建立就绪义务。 */
    FGamePlatformWorldStreamingHandle Request(const FGamePlatformWorldStreamingRequest& Request, FGamePlatformResult& OutResult);
    /** 精确已知句柄幂等撤销（失败请求也可撤销）；不卸载传统关卡、不恢复外部标志。 */
    FGamePlatformResult Cancel(const FGamePlatformWorldStreamingHandle& Handle);
    /** 采样真实引擎状态、Owner/World有效性和超时，不以固定帧数/延时冒充完成。 */
    void Tick();
    /** 无必需请求时为真；关闭/失效World为假。调用前须Tick获取最新底层状态。 */
    bool IsRequiredReady() const;
    /** 未显式撤销的必需请求是否失败；不会把可选请求失败升级为世界失败。 */
    bool HasRequiredFailure() const;
    /** 最近Tick快照；无效/跨代次/其他协调器的句柄返回Failed与明确错误码。 */
    FGamePlatformWorldStreamingResult GetState(const FGamePlatformWorldStreamingHandle& Handle) const;
    /** 幂等停止接受请求、注销自己的源、解绑世界委托；终止记录保留到析构供查询。 */
    void Shutdown();

    FGamePlatformWorldStreaming(const FGamePlatformWorldStreaming&) = delete;
    FGamePlatformWorldStreaming& operator=(const FGamePlatformWorldStreaming&) = delete;

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
