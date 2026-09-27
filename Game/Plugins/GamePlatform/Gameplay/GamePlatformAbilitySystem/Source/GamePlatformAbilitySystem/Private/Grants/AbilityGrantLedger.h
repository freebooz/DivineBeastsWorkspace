#pragma once
#include <cstdint>
#include <map>
#include <string>

/** 无UE依赖的本实例授权阶段登记；所有方法由ASC游戏线程串行调用。GUID作用域由公开句柄外层核对。 */
namespace GamePlatformAbilityPolicy
{
enum class EGrantState { Invalid, Pending, Applied, Revoking, Released, Failed };
class FGrantLedger
{
public:
    explicit FGrantLedger(std::size_t MaximumRecords) : Limit(MaximumRecords) {}
    /** Sequence不得复用；Source在Pending/Applied/Revoking阶段唯一。历史保留有界，满额拒绝而非遗忘所有权。 */
    bool Begin(std::uint64_t Sequence, std::uint64_t Generation, const std::string& Source)
    {
        if (!Sequence || !Generation || Source.empty() || Entries.size() >= Limit || Entries.contains(Sequence)) return false;
        for (const auto& Pair : Entries)
            if (Pair.second.Source == Source && Pair.second.State != EGrantState::Released && Pair.second.State != EGrantState::Failed) return false;
        Entries.emplace(Sequence, FRecord{Generation, Source, EGrantState::Pending}); return true;
    }
    /** 只允许当前Pending记录完成一次；取消先转Revoking，任何迟到成功都被拒绝。 */
    bool Complete(std::uint64_t Sequence, std::uint64_t Generation, bool bSuccess)
    {
        auto It = Entries.find(Sequence);
        if (It == Entries.end() || It->second.Generation != Generation || It->second.State != EGrantState::Pending) return false;
        It->second.State = bSuccess ? EGrantState::Applied : EGrantState::Failed; return true;
    }
    bool BeginRelease(std::uint64_t Sequence, std::uint64_t Generation)
    {
        auto It = Entries.find(Sequence);
        if (It == Entries.end() || It->second.Generation != Generation) return false;
        if (It->second.State != EGrantState::Released) It->second.State = EGrantState::Revoking;
        return true;
    }
    bool FinishRelease(std::uint64_t Sequence)
    {
        auto It = Entries.find(Sequence);
        if (It == Entries.end() || (It->second.State != EGrantState::Revoking && It->second.State != EGrantState::Released)) return false;
        It->second.State = EGrantState::Released; return true;
    }
    EGrantState Get(std::uint64_t Sequence) const
    { const auto It = Entries.find(Sequence); return It == Entries.end() ? EGrantState::Invalid : It->second.State; }
private:
    struct FRecord { std::uint64_t Generation; std::string Source; EGrantState State; };
    std::size_t Limit;
    std::map<std::uint64_t, FRecord> Entries;
};
/** 本地接口不具备认证效力；当前拥有者、真实激活门禁和当前非零Avatar代次必须同时成立。 */
inline bool AllowsInput(bool bLocalOwner, bool bGameplayAllowed, std::uint64_t Current, std::uint64_t Submitted)
{ return bLocalOwner && bGameplayAllowed && Current != 0 && Current == Submitted; }
}
