#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

/**
 * 神兽联盟竞技客户端缓存策略。
 * 仅比较已经由服务器发布的英雄身份、角色代次及已授予技能的Profile身份集合。
 * 不负责技能合法性、Damage计算、资产加载或网络复制。可独立执行自动化测试。
 */
namespace DivineBeastsHitFeedbackGrantPolicy
{
    inline bool RequiresRefresh(
        const TSet<FPrimaryAssetId>& CachedIds,
        const FName CachedHeroId,
        const int32 CachedAvatarGeneration,
        const TSet<FPrimaryAssetId>& AuthorizedIds,
        const FName AuthorizedHeroId,
        const int32 AuthorizedAvatarGeneration)
    {
        if (CachedHeroId != AuthorizedHeroId ||
            CachedAvatarGeneration != AuthorizedAvatarGeneration ||
            CachedIds.Num() != AuthorizedIds.Num())
        {
            return true;
        }

        // 授予槽位排序不应造成Profile释放和重复磁盘IO；只比较稳定身份集合。
        for (const FPrimaryAssetId& Id : AuthorizedIds)
        {
            if (!CachedIds.Contains(Id))
            {
                return true;
            }
        }
        return false;
    }
}
