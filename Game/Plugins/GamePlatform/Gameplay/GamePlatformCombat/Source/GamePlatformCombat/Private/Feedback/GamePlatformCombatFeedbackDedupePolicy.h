#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformCombatTypes.h"

/**
 * GamePlatformCombatFeedbackDedupe（平台已确认战斗表现事实去重策略）。
 *
 * 仅作用于World内的客户端表现通知；同一Guid下的Damage与Death是不同事实，
 * 必须分别播放，不能直接按Guid去重。限长循环槽避免每次命中RemoveAt(0)搬移内存。
 * 不参与Gameplay/GAS授权、结算、复制或服务器性能关键路径。
 */
namespace GamePlatformCombatFeedbackDedupe
{
    struct FCache
    {
        /** Guid对应已经见过的事件类型位；EGamePlatformCombatEventType目前只有7种。 */
        TMap<FGuid, uint8> SeenTypeMasks;
        /** 唯一Guid的环形插入记录，容量满后复用最老槽。 */
        TArray<FGuid> RingIds;
        int32 NextSlot = 0;

        void Reset()
        {
            SeenTypeMasks.Reset();
            RingIds.Reset();
            NextSlot = 0;
        }

        /**
         * 返回true表示第一次见到此(Guid,EventType)，false表示重复或非法。
         * 同一Guid下不同事件类型会保留各自位，不挤掉彼此的去重信息。
         */
        /** 世界子系统和自动化测试均复用同一判定函数，避免两套去重逻辑漂移。 */
        static bool TryRememberState(
            TMap<FGuid, uint8>& SeenTypeMasks,
            TArray<FGuid>& RingIds,
            int32& NextSlot,
            const FGuid& EventId,
            EGamePlatformCombatEventType EventType,
            int32 MaximumDistinctEvents = 2048)
        {
            const int32 TypeIndex = static_cast<int32>(EventType);
            if (!EventId.IsValid() || TypeIndex < 0 || TypeIndex >= 8 ||
                MaximumDistinctEvents <= 0)
            {
                return false;
            }

            const uint8 TypeMask = static_cast<uint8>(1u << TypeIndex);
            if (uint8* Existing = SeenTypeMasks.Find(EventId))
            {
                if ((*Existing & TypeMask) != 0)
                {
                    return false;
                }
                *Existing |= TypeMask;
                return true;
            }

            // 构建每World有界内存：缓存只记有限个唯一Guid，挤出最旧而非累积整场记录。
            if (RingIds.Num() >= MaximumDistinctEvents)
            {
                SeenTypeMasks.Remove(RingIds[NextSlot]);
                RingIds[NextSlot] = EventId;
                NextSlot = (NextSlot + 1) % MaximumDistinctEvents;
            }
            else
            {
                RingIds.Add(EventId);
            }
            SeenTypeMasks.Add(EventId, TypeMask);
            return true;
        }

        bool TryRemember(const FGuid& EventId,
                         EGamePlatformCombatEventType EventType,
                         int32 MaximumDistinctEvents = 2048)
        {
            return TryRememberState(
                SeenTypeMasks, RingIds, NextSlot, EventId, EventType, MaximumDistinctEvents);
        }

        int32 NumUniqueEvents() const { return SeenTypeMasks.Num(); }
    };
}
