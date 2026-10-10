#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadSafeCounter64.h"
#include "State/GamePlatformDebugStateProvider.h"

/**
 * GamePlatformDebug（游戏平台调试插件）中立注册表。
 * 开发诊断调用方按需采集，进程对象只拥有机制与弱目标，不保存用户/比赛强引用。
 * 元数据读写受锁保护；涉及Actor/UObject的目标解析和采集必须在游戏线程，Provider回调在锁外执行。
 */
class GAMEPLATFORMDEBUG_API FGamePlatformDebugRegistry
{
public:
    /** 取得进程机制注册表；模块关闭须成对注销自己注册的Provider/命令。 */
    static FGamePlatformDebugRegistry& Get();

    /** 持有Provider共享引用；空ID或重复ID返回false，不能覆盖已有提供者。 */
    bool RegisterStateProvider(const TSharedRef<IGamePlatformDebugStateProvider>& Provider);
    /** 幂等删除对应机制与启用标志；已经取出的采集快照可继续完成。 */
    void UnregisterStateProvider(FName ProviderId);
    /** 返回按ID排序的值快照，不暴露内部容器。 */
    TArray<FName> GetProviderIds() const;

    /** 注册gp.Debug.前缀的中立命令元数据；名称为空/不合规/重复返回false，不执行命令。 */
    bool RegisterCommandDescriptor(const FGamePlatformDebugCommandDescriptor& Descriptor);
    /** 按名称幂等注销元数据；实际控制台执行者由调用模块拥有。 */
    void UnregisterCommandDescriptor(FName CommandName);
    /** 返回排序后的可用命令值快照，调用方不得据此绕过权限。 */
    TArray<FGamePlatformDebugCommandDescriptor> GetCommandDescriptors() const;

    /** 仅切换已注册Provider；缺失ID返回false，不创建空实现。 */
    bool SetProviderEnabled(FName ProviderId, bool bEnabled);
    /** 查询启用标志；缺失Provider返回false。 */
    bool IsProviderEnabled(FName ProviderId) const;

    /** 游戏线程采集；缺失/禁用/成本未授权/目标不支持返回false和脱敏失败快照，不修改Gameplay。 */
    bool CollectSnapshot(
        FName ProviderId,
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& OutSnapshot);

    /** 游戏线程为存活Actor取得弱目标身份；空/销毁对象返回无效目标，不延长Actor生命周期。 */
    FGamePlatformDebugTarget ResolveDebugTarget(AActor* Actor);
    /** 游戏线程按需移除失效弱目标，不进行逐帧遍历世界。 */
    void PruneExpiredTargets();

    /** 过滤敏感字段并应用字段数、字符串长度和Payload上限。 */
    void SanitizeAndBoundSnapshot(FGamePlatformDebugSnapshot& Snapshot) const;

    /** 返回仅包含已经过过滤字段的可复制文本。 */
    FString BuildSanitizedSummary(const FGamePlatformDebugSnapshot& Snapshot) const;

    /** 强制下一次快照具有新的 Revision（修订号），不修改 Gameplay。 */
    uint64 ForceRefreshRevision();

private:
    FGamePlatformDebugRegistry() = default;

    static bool IsSensitiveField(const FGamePlatformDebugField& Field);

    mutable FCriticalSection Mutex;
    TMap<FName, TSharedPtr<IGamePlatformDebugStateProvider>> Providers;
    TMap<FName, FGamePlatformDebugCommandDescriptor> Commands;
    TMap<FName, bool> ProviderEnabled;
    TMap<TWeakObjectPtr<AActor>, FGamePlatformDebugTarget> Targets;
    FThreadSafeCounter64 RevisionCounter;
};
