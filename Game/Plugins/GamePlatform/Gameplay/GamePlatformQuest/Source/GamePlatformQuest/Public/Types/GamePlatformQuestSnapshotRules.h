#pragma once
#include <cstddef>
#include <cstdint>

// 任务快照与对账纯值策略；游戏线程调用，由服务器拥有事件和Revision，客户端只消费显示序列。
namespace GamePlatformQuestSnapshotPolicy
{
inline bool CanTransferReplay(std::size_t DeferredCount, std::size_t UniqueReplayCount, std::size_t Limit)
{ return DeferredCount <= Limit && UniqueReplayCount <= Limit - DeferredCount; }
inline bool ShouldAccept(std::int64_t Revision, std::int64_t Sequence, bool StateChanged,
    std::int64_t ExistingRevision, std::int64_t ExistingSequence)
{
    if (Revision != ExistingRevision) { return Revision > ExistingRevision; }
    // 0为旧调用方未提供显示序列；只沿用状态变化兼容，不接受它覆盖已带序列的新流。
    if (Sequence == 0 && ExistingSequence == 0) { return StateChanged; }
    return Sequence > ExistingSequence;
}
}
