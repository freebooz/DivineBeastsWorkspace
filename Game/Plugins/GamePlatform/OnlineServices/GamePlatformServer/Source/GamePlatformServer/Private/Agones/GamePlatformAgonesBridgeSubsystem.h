#pragma once

// GamePlatform（平台层）专服 Agones Sidecar REST 桥接；不依赖任何 MOBA 或项目身份。
// 按 GameInstance 生命周期绑定平台控制面，只负责容器健康、就绪、容量与安全关闭。
// MainArena 的 GameServerAllocation 由 Go 控制面原子分配；可共享的世界实例首次拥有活跃玩家后，自行标记 Agones Allocated 以防缩容误删。

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformAgonesBridgeSubsystem.generated.h"

/**
 * UGamePlatformAgonesBridgeSubsystem（Agones 专服侧车生命周期适配子系统）。
 * 只在专服且 Agones 注入 AGONES_SDK_HTTP_PORT 时创建；缺失该变量的本地模式完全不受影响。
 * 所有状态、委托与计时器都属于本 GameInstance；网络完成回调按对象弱引用与请求代次拒绝过期操作。
 */
UCLASS()
class GAMEPLATFORMSERVER_API UGamePlatformAgonesBridgeSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    /** SDK 操作种类。只允许 loopback HTTP，禁止把 Agones SDK API 暴露给游戏客户端。 */
    friend class FGamePlatformAgonesBridgeSchedulingTest;

    enum class EAgonesSdkOperation : uint8
    {
        None, // 本周期无待执行操作，不产生网络请求。
        Health,
        Ready,
        Allocate,
        Counter,
        Shutdown
    };

    /** 只接收 GamePlatformServer 已确认的控制面状态；不得把地图 BeginPlay 当作 Ready。 */
    void HandleServerLifecycle(const FGamePlatformServerLifecycleSnapshot& Snapshot);
    /** 来自平台控制面已验证的权威人数采样；不枚举客户端实体、不生成玩家数据。 */
    void HandlePlayerCountSampled(int32 CurrentPlayers, int32 Capacity);
    /** 每秒一次的轻量调度，只允许一个 HTTP 请求在途；Shutdown 优先，健康独立，世界首次有玩家先保护实例再报告 Counter。 */
    bool TickSdk(float DeltaSeconds);
    /** 无网络副作用的调度选择；Shutdown 优先，Health 最多让出两个调度周期避免请求饥饿。 */
    EAgonesSdkOperation SelectNextSdkOperation(double Now) const;
    /** 启动有限超时 HTTP 请求；Counter 只在显式启用时调用 Beta 接口。 */
    void StartSdkRequest(EAgonesSdkOperation Operation);
    /** GameThread 单次终态；对停止/旧代次返回的结果不产生任何状态修改。 */
    void CompleteSdkRequest(uint64 RequestGeneration, EAgonesSdkOperation Operation,
        bool bSucceeded, int32 SamplePlayers, int32 SampleCapacity);
    /** 先解绑后取消，避免排空期间旧 Ready 请求回调重新打开可分配状态。 */
    void CancelSdkRequest();

    FDelegateHandle LifecycleChangedHandle;
    FDelegateHandle PlayerCountHandle;
    FTSTicker::FDelegateHandle TickerHandle;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
    EAgonesSdkOperation ActiveOperation = EAgonesSdkOperation::Health;
    uint64 RequestGeneration = 0;
    int32 SdkHttpPort = 0;
    FString CounterKey;
    int32 LatestPlayers = -1;
    int32 LatestCapacity = 0;
    double NextHealthAtSeconds = 0.0;
    double NextRetryAtSeconds = 0.0;
    double HealthIntervalSeconds = 4.0;
    bool bEnabled = false;
    bool bReadyRequested = false;
    bool bReadyAcknowledged = false;
    bool bDraining = false;
    bool bNeedsSelfAllocation = false;
    bool bSelfAllocated = false;
    bool bShutdownRequested = false;
    bool bShutdownAcknowledged = false;
    bool bCounterDirty = false;
    bool bClosing = false;
};
