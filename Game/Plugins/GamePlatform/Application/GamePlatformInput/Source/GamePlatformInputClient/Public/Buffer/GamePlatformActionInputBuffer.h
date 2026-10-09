#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInputTypes.h"

/**
 * FGamePlatformActionInputBuffer（游戏平台战斗动作输入缓冲）。
 *
 * 纯数据、无Tick、无Gameplay副作用。只收集当前本地玩家的Started/Completed/Canceled离散事件；
 * 移动/视角连续轴不应经过本缓冲。何时进入缓冲、何时可执行由上层动画和技能规则决定。
 * Enqueue/Consume均由LocalPlayer游戏线程调用，严禁将缓冲事件当作服务器授权。
 */
class GAMEPLATFORMINPUTCLIENT_API FGamePlatformActionInputBuffer
{
public:
    explicit FGamePlatformActionInputBuffer(int32 InMaxEvents = 8, double InLifetimeSeconds = 0.25);

    /** 追加合法离散输入；拒绝失效代次/序号/异常时间；队列满则丢最早输入。 */
    bool Enqueue(const FGamePlatformInputEvent& Event, double NowSeconds);

    /**
     * 按原始序号顺序提取当前绑定代次仍有效且未过期的输入，随后清空队列。
     * 调用方须在技能合法取消窗口、GAS token有效时执行这些输入，禁止直接调用Gameplay结果。
     */
    int32 ConsumePending(
        double NowSeconds,
        uint64 ExpectedBindingGeneration,
        TArray<FGamePlatformInputEvent>& OutEvents);

    /** 角色离场、配置重绑、世界切换/应用失焦时必须撤销旧输入。 */
    void Reset();

    int32 Num() const { return Pending.Num(); }

private:
    struct FPendingInput
    {
        FGamePlatformInputEvent Event;
        double ExpiresAtSeconds = 0.0;
    };

    void PruneExpired(double NowSeconds);
    TArray<FPendingInput> Pending;

    int32 MaxEvents = 8;
    double LifetimeSeconds = 0.25;
};
