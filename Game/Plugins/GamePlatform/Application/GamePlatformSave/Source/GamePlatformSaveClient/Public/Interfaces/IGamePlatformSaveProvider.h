#pragma once

// GamePlatformSave命名空间扩展接口。
// 上层领域只通过该接口声明自身Schema版本和迁移逻辑，Save插件不反向依赖具体业务模块。
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"

/**
 * 本地存档命名空间提供者。
 *
 * 生命周期：调用方以TSharedRef注册，Save服务在进行中的Load请求内持有共享引用。
 * 线程：全部方法仅在游戏线程调用。
 * 权威：迁移结果仍是非权威本地数据，不能替代后端或服务器校验。
 */
class GAMEPLATFORMSAVECLIENT_API IGamePlatformSaveProvider
{
public:
    virtual ~IGamePlatformSaveProvider() = default;

    /** 返回唯一稳定命名空间；不能为空。 */
    virtual FName GetSaveNamespace() const = 0;

    /** 返回当前Schema版本；必须大于0。 */
    virtual int32 GetCurrentSchemaVersion() const = 0;

    /**
     * 把旧Payload迁移到TargetSchemaVersion。
     * 实现必须确定性、不得访问磁盘或网络；成功后Payload应符合目标版本。
     */
    virtual FGamePlatformResult MigratePayload(
        int32 StoredSchemaVersion,
        int32 TargetSchemaVersion,
        TArray<uint8>& InOutPayload) const = 0;
};
