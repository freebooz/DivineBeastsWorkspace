#pragma once
#include "Interfaces/IGamePlatformLoadingTask.h"

/** 实例服务的所有API仅游戏线程；快照通知在后续调度发布，不同步推进调用方流程。 */
class GAMEPLATFORMLOADING_API IGamePlatformLoadingService
{
public:
    virtual ~IGamePlatformLoadingService() = default;
    /** 无有效实例服务返回nullptr；不创建进程全局替身，不启动玩家操作。 */
    static IGamePlatformLoadingService* Get(UGameInstance& Instance);
    /** 单主操作，已有未释放操作返回Busy；Owner必须属于本实例，失效导致取消和清理。 */
    virtual FGamePlatformLoadingHandle StartLoadingOperation(const FGamePlatformLoadingOperationSpec& Spec,
        TWeakObjectPtr<UObject> Owner, FGamePlatformResult& OutResult) = 0;
    /** 完整身份匹配；只取消运行中操作，不回滚服务端事务，终态重复调用无副作用。 */
    virtual FGamePlatformResult CancelLoadingOperation(const FGamePlatformLoadingHandle& Handle) = 0;
    /** 显式释放成功资源；运行中先取消。终态快照保留至下一次Start，重复释放幂等。 */
    virtual FGamePlatformResult ReleaseLoadingOperation(const FGamePlatformLoadingHandle& Handle) = 0;
    /** 本实例最近操作的值快照，不跨实例读取。 */
    virtual FGamePlatformLoadingSnapshot GetLoadingSnapshot() const = 0;
    /** 带弱所有者、精确操作过滤的延后状态订阅；初始通知也延后。失效不再通知。 */
    virtual FGamePlatformLoadingRegistration SubscribeLoadingState(const FGamePlatformLoadingHandle& Handle,
        TWeakObjectPtr<UObject> Owner, TFunction<void(const FGamePlatformLoadingSnapshot&)> Callback) = 0;
    /** 只撤销本实例同身份订阅；不存在返回false。 */
    virtual bool UnsubscribeLoadingState(const FGamePlatformLoadingRegistration& Registration) = 0;
    /** 重复类型/空工厂失败；活跃操作期间禁止变更工厂集合。内置Data/WorldPresence不接受覆盖。 */
    virtual FGamePlatformLoadingRegistration RegisterTaskFactory(FName Type, FGamePlatformLoadingTaskFactory Factory,
        FGamePlatformResult& OutResult) = 0;
    /** 未释放操作期间返回Busy；撤销后新操作不能使用该工厂。 */
    virtual FGamePlatformResult UnregisterTaskFactory(const FGamePlatformLoadingRegistration& Registration) = 0;
    /** 必须同时匹配句柄、Ready屏障及持有资源；WorldPresence还检查当前世界事实仍有效。 */
    virtual bool IsReadyToPlay(const FGamePlatformLoadingHandle& Handle) const = 0;
    /** 项目bootstrap的基础可操作声明；严格绑定当前操作和实例世界，不代替Session准入。 */
    virtual FGamePlatformResult ReportWorldOperable(const FGamePlatformLoadingHandle& Handle, UWorld& World) = 0;
};
