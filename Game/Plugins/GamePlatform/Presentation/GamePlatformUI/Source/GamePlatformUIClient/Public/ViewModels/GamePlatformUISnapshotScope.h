#pragma once

#include "CoreMinimal.h"

/**
 * FGamePlatformUISnapshotScope（平台界面来源快照校验契约）。
 * 世界、账号、角色、组队及HUD的只读通知共同遵循相同来源与修订规则。
 * 这是无UObject状态的值策略，不新建Subsystem（子系统）或并行缓存。
 */
struct GAMEPLATFORMUICLIENT_API FGamePlatformUISnapshotScope final
{
    /**
     * 仅接受已绑定的同一来源、非负且严格递增的修订号。
     * 不决定网络权限；调用方仍须检查字段完整性和观察者可见性。
     */
    static bool CanAccept(const FGuid& CurrentScopeId, int64 CurrentRevision,
        const FGuid& IncomingScopeId, int64 IncomingRevision)
    {
        return CurrentScopeId.IsValid() && CurrentScopeId == IncomingScopeId &&
            IncomingRevision >= 0 && IncomingRevision > CurrentRevision;
    }

    /** 无显式FGuid作用域的地图、目标清单、倒计时及槽位栏按FName稳定身份去重。
     * 当前身份更新必须严格递增修订；切换身份允许修订从0开始。
     * 此处仅审核展示数据的版本，不替代领域授权校验。 */
    static bool CanAcceptNamedRevision(
        FName CurrentIdentity, int64 CurrentRevision,
        FName IncomingIdentity, int64 IncomingRevision)
    {
        return !IncomingIdentity.IsNone() && IncomingRevision >= 0 &&
            (CurrentIdentity != IncomingIdentity || IncomingRevision > CurrentRevision);
    }
};
