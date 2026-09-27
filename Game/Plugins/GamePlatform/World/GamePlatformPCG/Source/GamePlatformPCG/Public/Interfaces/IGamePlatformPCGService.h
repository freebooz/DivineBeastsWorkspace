#pragma once
#include "Types/GamePlatformPCGTypes.h"
class UWorld;
class UGamePlatformPCGProfileDefinition;

/** 明确世界的游戏线程门面；不接受生产网络授权，不提供HTTP或地图编辑操作。 */
class GAMEPLATFORMPCG_API IGamePlatformPCGService
{
public:
    virtual ~IGamePlatformPCGService() = default;
    /** 仅已存在Game/PIE世界实例，Editor创作使用独立模块；无服务返回nullptr。 */
    static IGamePlatformPCGService* Get(UWorld& World);
    /** 已加载配置的纯校验加世界基础事实检查；不会同步加载资产、等待总Ready或启动生成。 */
    virtual FGamePlatformResult ValidateProfile(const UGamePlatformPCGProfileDefinition& Profile) const = 0;
    /** 接纳后返回Loading句柄；最多4个未清理请求，无无限队列。结果依订阅/快照异步读取。 */
    virtual FGamePlatformPCGHandle RequestGeneration(const FGamePlatformPCGRequest& Request,FGamePlatformResult& OutResult) = 0;
    /** 取消未终态请求；成功结果使用Release。原生未排空时保持Cleaning，不伪报即时停止。 */
    virtual FGamePlatformResult CancelGeneration(const FGamePlatformPCGHandle& Handle) = 0;
    /** 停止持有结果并清理所属输出；精确句柄重复调用幂等，不删除他源/人工/静态资产。 */
    virtual FGamePlatformResult ReleaseGeneration(const FGamePlatformPCGHandle& Handle) = 0;
    /** 精确句柄读取，无效返回错误值；Ready贡献需检查bIsResultValid而非历史Outcome。 */
    virtual FGamePlatformPCGSnapshot GetGenerationSnapshot(const FGamePlatformPCGHandle& Handle) const = 0;
    /** 弱Owner必须属于本世界；游戏线程延后通知，完成后清理/失效也会通知，不只发布一次成功。 */
    virtual FGamePlatformPCGSubscription SubscribeGeneration(const FGamePlatformPCGHandle& Handle,TWeakObjectPtr<UObject> Owner,
        TFunction<void(const FGamePlatformPCGSnapshot&)> Callback) = 0;
    /** 本作用域精确撤销；不存在/跨世界返回false。 */
    virtual bool UnsubscribeGeneration(const FGamePlatformPCGSubscription& Subscription) = 0;
};
