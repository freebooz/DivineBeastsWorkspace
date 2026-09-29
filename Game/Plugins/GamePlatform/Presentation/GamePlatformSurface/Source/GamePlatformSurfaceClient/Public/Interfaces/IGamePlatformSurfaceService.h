#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformSurfaceTypes.h"

class UWorld;

/** 表面环境状态变化通知；只在状态真正变化后广播，不按帧轮询。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformSurfaceStateChanged,
    const FGamePlatformSurfaceEnvironmentState&,
    int32 /* Revision */);

/**
 * GamePlatformSurface低层客户端服务接口。
 *
 * 供天气适配、世界表现、项目组合和测试使用；服务器玩法不得依赖本接口决定碰撞、移动或战斗结果。
 */
class GAMEPLATFORMSURFACECLIENT_API IGamePlatformSurfaceService
{
public:
    virtual ~IGamePlatformSurfaceService() = default;

    /** 获取指定客户端世界的Surface服务；不支持的世界或Dedicated Server返回nullptr。 */
    static IGamePlatformSurfaceService* Get(UWorld& World);

    /**
     * 应用新的环境表面状态。
     * 非有限数值整包拒绝；合法值先裁剪到安全范围，再事件驱动写入MPC。
     */
    virtual FGamePlatformSurfaceUpdateResult ApplyEnvironmentState(
        const FGamePlatformSurfaceEnvironmentState& State) = 0;

    /** 返回当前世界已接受的表面状态。 */
    virtual const FGamePlatformSurfaceEnvironmentState& GetEnvironmentState() const = 0;

    /** 返回当前状态修订号；仅状态真正变化时递增。 */
    virtual int32 GetRevision() const = 0;

    /** 重新解析配置中的MPC并把当前状态重新推送；用于编辑器资产生成或热重载后的显式恢复。 */
    virtual FGamePlatformSurfaceUpdateResult RefreshMaterialBinding() = 0;

    /** 订阅状态变化；调用方必须保存句柄并在自身生命周期结束时取消。 */
    virtual FDelegateHandle AddStateChangedHandler(const FGamePlatformSurfaceStateChanged::FDelegate& Handler) = 0;

    /** 取消此前的状态变化订阅；未知句柄按幂等方式忽略。 */
    virtual void RemoveStateChangedHandler(FDelegateHandle Handle) = 0;
};
