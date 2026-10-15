// Niagara Data Channels（NDC）通用瞬时命中事件写入桥；仅客户端游戏线程。
// 必须由现有VFX WorldSubsystem完成请求去重、有效世界检查和资源租约后调用。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXRequest.h"

class UWorld;
class UNiagaraDataChannelAsset;

/** 高频短命中统一写入NDC：不创建NiagaraComponent，不参与权威伤害判定。 */
class FGamePlatformVFXDataChannelBridge final
{
public:
    /** 返回本次NDC写入是否被引擎受理；失败时上层可回退原有Niagara System。 */
    static bool WriteInstantBurst(
        UWorld& World,
        const UNiagaraDataChannelAsset& ChannelAsset,
        const FGamePlatformVFXRequest& Request);
};
