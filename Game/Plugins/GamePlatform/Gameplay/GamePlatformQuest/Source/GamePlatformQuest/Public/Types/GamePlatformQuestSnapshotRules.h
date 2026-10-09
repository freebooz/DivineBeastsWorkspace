#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>

// 任务快照与对账纯值策略；游戏线程调用，由服务器拥有事件和Revision，客户端只消费显示序列。
namespace GamePlatformQuestSnapshotPolicy
{
// 重放只归属接纳时的任务实例；调用方另外持有EventId和事件载荷，不能把Q2重放扩散到已提交Q1。
template<class QuestIdentity, class InstanceIdentity>
inline bool MatchesReplayTarget(const QuestIdentity& ActualQuest, const InstanceIdentity& ActualInstance,
    const QuestIdentity& ExpectedQuest, const InstanceIdentity& ExpectedInstance)
{ return ActualQuest == ExpectedQuest && ActualInstance == ExpectedInstance; }
// 持久快照必须自洽，拒绝非有限数值、越界和与数值矛盾的完成位；数量/身份由领域校验器核验。
inline bool IsProgressValid(double Current, double Required, bool Completed)
{ return std::isfinite(Current) && std::isfinite(Required) && Required > 0.0 && Current >= 0.0 &&
    Current <= Required && Completed == (Current >= Required); }
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
