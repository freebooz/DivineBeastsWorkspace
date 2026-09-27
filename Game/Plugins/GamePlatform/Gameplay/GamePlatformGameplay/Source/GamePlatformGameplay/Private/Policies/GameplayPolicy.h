#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// 本文件是无UE依赖的确定性策略内核。调用者负责在游戏线程串行调用并把真实UObject事实投影为值。
namespace GamePlatformGameplay::Policy
{
/** 服务器体验运行阶段；Released和Failed不会被旧异步事件重新激活。 */
enum class EExperienceStage : std::uint8_t
{
    Unassigned,
    Preparing,
    Active,
    Draining,
    Released,
    Failed
};

/**
 * 判断一次体验阶段转换是否属于第一版支持范围。
 * 相同阶段是幂等重算；本函数不执行资源申请、回滚或通知。
 */
constexpr bool CanTransition(const EExperienceStage From, const EExperienceStage To) noexcept
{
    if (From == To)
    {
        return true;
    }

    switch (From)
    {
    case EExperienceStage::Unassigned:
        return To == EExperienceStage::Preparing || To == EExperienceStage::Released || To == EExperienceStage::Failed;
    case EExperienceStage::Preparing:
        return To == EExperienceStage::Active || To == EExperienceStage::Draining || To == EExperienceStage::Failed;
    case EExperienceStage::Active:
        return To == EExperienceStage::Draining || To == EExperienceStage::Failed;
    case EExperienceStage::Draining:
        return To == EExperienceStage::Released || To == EExperienceStage::Failed;
    case EExperienceStage::Failed:
        return To == EExperienceStage::Draining || To == EExperienceStage::Released;
    case EExperienceStage::Released:
    default:
        return false;
    }
}

/** 单个服务器玩家在当前世界记录中的阶段；不代表客户端输入是否已经开放。 */
enum class EPlayerStage : std::uint8_t
{
    Accepted,
    WaitingExperience,
    WaitingSpawn,
    Spawning,
    Possessed,
    AwaitingClient,
    Active,
    Leaving,
    Removed
};

/**
 * 判断玩家阶段转换。失败重试回到WaitingSpawn，但Removed是不可复活终态。
 * 相同阶段视为幂等重算；调用者仍须核对准入、世界、体验、Pawn和令牌代次。
 */
constexpr bool CanTransition(const EPlayerStage From, const EPlayerStage To) noexcept
{
    if (From == To)
    {
        return true;
    }
    if (To == EPlayerStage::Leaving)
    {
        return From != EPlayerStage::Removed && From != EPlayerStage::Leaving;
    }

    switch (From)
    {
    case EPlayerStage::Accepted:
        return To == EPlayerStage::WaitingExperience;
    case EPlayerStage::WaitingExperience:
        return To == EPlayerStage::WaitingSpawn;
    case EPlayerStage::WaitingSpawn:
        return To == EPlayerStage::Spawning;
    case EPlayerStage::Spawning:
        return To == EPlayerStage::Possessed || To == EPlayerStage::WaitingSpawn;
    case EPlayerStage::Possessed:
        return To == EPlayerStage::AwaitingClient || To == EPlayerStage::WaitingSpawn;
    case EPlayerStage::AwaitingClient:
        return To == EPlayerStage::Active || To == EPlayerStage::WaitingSpawn;
    case EPlayerStage::Active:
        return To == EPlayerStage::WaitingSpawn;
    case EPlayerStage::Leaving:
        return To == EPlayerStage::Removed;
    case EPlayerStage::Removed:
    default:
        return false;
    }
}

/** 服务器准入的非敏感代次投影；字段均来自可信服务器适配，不接收客户端自报。 */
struct FAdmissionIdentity
{
    /** 当前真实连接代次；新连接必须递增。 */
    std::uint64_t ConnectionGeneration = 0;
    /** 当前已提交会话绑定代次；旧SessionEpoch不得影响新绑定。 */
    std::uint64_t SessionEpoch = 0;
    /** 当前世界内玩家记录代次；显示名相同也不得复用。 */
    std::uint64_t PlayerGeneration = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return ConnectionGeneration != 0 && SessionEpoch != 0 && PlayerGeneration != 0;
    }

    constexpr bool operator==(const FAdmissionIdentity&) const noexcept = default;
};

/** 仅完全一致且两侧合法的准入代次可操作同一玩家记录。 */
[[nodiscard]] constexpr bool IsCurrentAdmission(
    const FAdmissionIdentity& Current,
    const FAdmissionIdentity& Candidate) noexcept
{
    return Current.IsValid() && Candidate.IsValid() && Current == Candidate;
}

/** 世界、玩家和生成三代次组成的出生幂等键；不包含可伪造的显示名或对象地址。 */
struct FSpawnOperationKey
{
    std::uint64_t WorldGeneration = 0;
    std::uint64_t PlayerGeneration = 0;
    std::uint64_t SpawnGeneration = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return WorldGeneration != 0 && PlayerGeneration != 0 && SpawnGeneration != 0;
    }

    constexpr bool operator==(const FSpawnOperationKey&) const noexcept = default;
};

/**
 * 拥有者准备确认令牌的纯值部分。Challenge是每次签发的随机非零值，不是认证凭据，
 * 不能替代服务器对真实连接、Pawn拥有关系及准入资格的重新验证。
 */
struct FPreparationToken
{
    std::uint64_t ExperienceEpoch = 0;
    std::uint64_t PlayerGeneration = 0;
    std::uint64_t PawnGeneration = 0;
    std::uint64_t StateRevision = 0;
    std::uint64_t Challenge = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return ExperienceEpoch != 0 && PlayerGeneration != 0 && PawnGeneration != 0
            && StateRevision != 0 && Challenge != 0;
    }

    constexpr bool operator==(const FPreparationToken&) const noexcept = default;
};

/** 仅完整匹配当前签发值的准备报告可继续；重复完成由上层玩家阶段拦截。 */
[[nodiscard]] constexpr bool IsCurrentPreparation(
    const FPreparationToken& Current,
    const FPreparationToken& Candidate) noexcept
{
    return Current.IsValid() && Candidate.IsValid() && Current == Candidate;
}

/** 有界等待队列；DeadlineSeconds使用调用方同一单调时钟，单位秒。 */
class FWaitingQueue
{
public:
    /** MaximumEntries为硬上限；0表示拒绝所有等待，不自动扩容。 */
    explicit FWaitingQueue(const std::size_t MaximumEntries) noexcept
        : Capacity(MaximumEntries)
    {
    }

    /**
     * 登记一个玩家。玩家代次必须非零，截止秒必须是有限正数；重复或满载返回false。
     * 本函数不延长已有玩家截止时间，防止重复请求形成无限等待。
     */
    [[nodiscard]] bool TryAdd(const std::uint64_t PlayerGeneration, const double DeadlineSeconds)
    {
        if (PlayerGeneration == 0 || !std::isfinite(DeadlineSeconds) || DeadlineSeconds <= 0.0
            || Entries.size() >= Capacity)
        {
            return false;
        }
        if (std::any_of(Entries.begin(), Entries.end(), [PlayerGeneration](const FEntry& Entry)
            { return Entry.PlayerGeneration == PlayerGeneration; }))
        {
            return false;
        }
        Entries.push_back({PlayerGeneration, DeadlineSeconds});
        return true;
    }

    /** 精确移除自己的记录；重复移除返回false且不改变其他记录。 */
    [[nodiscard]] bool Remove(const std::uint64_t PlayerGeneration)
    {
        const auto Iterator = std::find_if(Entries.begin(), Entries.end(), [PlayerGeneration](const FEntry& Entry)
            { return Entry.PlayerGeneration == PlayerGeneration; });
        if (Iterator == Entries.end())
        {
            return false;
        }
        Entries.erase(Iterator);
        return true;
    }

    /**
     * 按入队顺序返回并移除截止时间不晚于NowSeconds的玩家；非法时钟不改变队列。
     * 调用方负责把每个返回值终结一次并执行服务器侧离开或恢复策略。
     */
    [[nodiscard]] std::vector<std::uint64_t> CollectExpired(const double NowSeconds)
    {
        std::vector<std::uint64_t> Result;
        if (!std::isfinite(NowSeconds))
        {
            return Result;
        }

        const auto FirstRetained = std::stable_partition(Entries.begin(), Entries.end(),
            [NowSeconds](const FEntry& Entry) { return Entry.DeadlineSeconds > NowSeconds; });
        for (auto Iterator = FirstRetained; Iterator != Entries.end(); ++Iterator)
        {
            Result.push_back(Iterator->PlayerGeneration);
        }
        Entries.erase(FirstRetained, Entries.end());
        return Result;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return Entries.size(); }

private:
    struct FEntry
    {
        std::uint64_t PlayerGeneration = 0;
        double DeadlineSeconds = 0.0;
    };

    std::size_t Capacity = 0;
    std::vector<FEntry> Entries;
};

/** 由服务器实际世界查询产生的出生候选投影；CandidateId在当前世界运行内稳定。 */
struct FSpawnCandidate
{
    std::uint64_t CandidateId = 0;
    std::int32_t Priority = 0;
    bool bCurrentWorld = false;
    bool bRequiredRegionReady = false;
    bool bReserved = false;
};

/**
 * 过滤跨世界、区域未就绪、已占用或零身份候选，再按优先级升序及身份升序稳定排序并去重。
 * 碰撞检测由引擎策略在生成前再次执行；本函数不把未检查碰撞的点声明为可生成。
 */
[[nodiscard]] inline std::vector<std::uint64_t> OrderEligibleSpawnCandidates(
    const std::vector<FSpawnCandidate>& Candidates)
{
    std::vector<FSpawnCandidate> Eligible;
    Eligible.reserve(Candidates.size());
    for (const FSpawnCandidate& Candidate : Candidates)
    {
        if (Candidate.CandidateId != 0 && Candidate.bCurrentWorld && Candidate.bRequiredRegionReady && !Candidate.bReserved)
        {
            Eligible.push_back(Candidate);
        }
    }

    std::sort(Eligible.begin(), Eligible.end(), [](const FSpawnCandidate& Left, const FSpawnCandidate& Right)
    {
        return Left.Priority != Right.Priority
            ? Left.Priority < Right.Priority
            : Left.CandidateId < Right.CandidateId;
    });

    std::vector<std::uint64_t> Result;
    Result.reserve(Eligible.size());
    for (const FSpawnCandidate& Candidate : Eligible)
    {
        if (std::find(Result.begin(), Result.end(), Candidate.CandidateId) == Result.end())
        {
            Result.push_back(Candidate.CandidateId);
        }
    }
    return Result;
}
}
