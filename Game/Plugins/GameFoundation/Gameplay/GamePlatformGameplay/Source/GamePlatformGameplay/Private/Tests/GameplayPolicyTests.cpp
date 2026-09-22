#if defined(GAMEPLAY_NATIVE_TESTS)
#include "../Policies/GameplayPolicy.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using namespace GamePlatformGameplay::Policy;

int FailureCount = 0;

void Expect(const bool bCondition, const std::string_view Message)
{
    if (!bCondition)
    {
        ++FailureCount;
        std::cerr << "FAILED: " << Message << '\n';
    }
}

void TestExperienceTransitions()
{
    Expect(CanTransition(EExperienceStage::Unassigned, EExperienceStage::Preparing),
        "未选定体验应能进入准备阶段");
    Expect(CanTransition(EExperienceStage::Preparing, EExperienceStage::Active),
        "准备完成应能激活体验");
    Expect(CanTransition(EExperienceStage::Active, EExperienceStage::Draining),
        "活动体验应能开始排空");
    Expect(CanTransition(EExperienceStage::Draining, EExperienceStage::Released),
        "排空完成应能释放体验");
    Expect(CanTransition(EExperienceStage::Preparing, EExperienceStage::Failed),
        "准备失败应进入失败终态");
    Expect(!CanTransition(EExperienceStage::Active, EExperienceStage::Preparing),
        "第一版不得把活动体验伪装成热切换准备");
    Expect(!CanTransition(EExperienceStage::Released, EExperienceStage::Active),
        "已经释放的运行不能被旧事件重新激活");
}

void TestPlayerTransitions()
{
    Expect(CanTransition(EPlayerStage::Accepted, EPlayerStage::WaitingExperience),
        "可信接入后可等待体验");
    Expect(CanTransition(EPlayerStage::WaitingExperience, EPlayerStage::WaitingSpawn),
        "体验准备后可等待出生");
    Expect(CanTransition(EPlayerStage::WaitingSpawn, EPlayerStage::Spawning),
        "出生条件就绪后可进入生成");
    Expect(CanTransition(EPlayerStage::Spawning, EPlayerStage::Possessed),
        "服务器控制成功后可进入已控制阶段");
    Expect(CanTransition(EPlayerStage::Possessed, EPlayerStage::AwaitingClient),
        "控制成功后只能等待当前拥有者准备");
    Expect(CanTransition(EPlayerStage::AwaitingClient, EPlayerStage::Active),
        "当前拥有者准备完成后可激活玩家");
    Expect(CanTransition(EPlayerStage::Active, EPlayerStage::WaitingSpawn),
        "服务器授权重生成可回到等待出生");
    Expect(CanTransition(EPlayerStage::Spawning, EPlayerStage::Leaving),
        "生成中离开必须可收敛清理");
    Expect(!CanTransition(EPlayerStage::Accepted, EPlayerStage::Active),
        "准入事实不能绕过体验、出生和拥有者确认");
    Expect(!CanTransition(EPlayerStage::Removed, EPlayerStage::Accepted),
        "已移除记录不能被迟到接入复活");
}

void TestAdmissionGeneration()
{
    const FAdmissionIdentity Current{11, 5, 9};
    Expect(IsCurrentAdmission(Current, FAdmissionIdentity{11, 5, 9}),
        "完全相同的连接、会话和玩家代次应匹配");
    Expect(!IsCurrentAdmission(Current, FAdmissionIdentity{10, 5, 9}),
        "旧连接代次不得接管当前玩家");
    Expect(!IsCurrentAdmission(Current, FAdmissionIdentity{11, 4, 9}),
        "旧SessionEpoch不得清理或激活当前玩家");
    Expect(!IsCurrentAdmission(Current, FAdmissionIdentity{11, 5, 8}),
        "旧PlayerGeneration不得改变当前记录");
    Expect(!IsCurrentAdmission(Current, FAdmissionIdentity{}),
        "零代次不是可信准入");
}

void TestSpawnIdempotency()
{
    const FSpawnOperationKey Current{101, 7, 3};
    Expect(Current.IsValid(), "完整世界、玩家和生成代次应形成合法幂等键");
    Expect(Current == FSpawnOperationKey{101, 7, 3},
        "同玩家同次生成应命中同一操作");
    Expect(!(Current == FSpawnOperationKey{102, 7, 3}),
        "新世界运行不能复用旧出生操作");
    Expect(!(Current == FSpawnOperationKey{101, 7, 4}),
        "新生成代次必须产生新操作");
    Expect(!FSpawnOperationKey{0, 7, 3}.IsValid(),
        "缺少世界运行身份的出生键必须拒绝");
}

void TestPreparationToken()
{
    const FPreparationToken Current{44, 8, 12, 2, 777};
    Expect(Current.IsValid(), "完整体验、玩家、Pawn代次及随机质询构成合法令牌");
    Expect(IsCurrentPreparation(Current, FPreparationToken{44, 8, 12, 2, 777}),
        "当前拥有者的完整令牌应被接受");
    Expect(!IsCurrentPreparation(Current, FPreparationToken{43, 8, 12, 2, 777}),
        "旧体验代次报告不得激活当前玩家");
    Expect(!IsCurrentPreparation(Current, FPreparationToken{44, 8, 11, 2, 777}),
        "旧Pawn代次报告不得激活新Pawn");
    Expect(!IsCurrentPreparation(Current, FPreparationToken{44, 8, 12, 2, 778}),
        "猜测或未来质询不得被接受");
    Expect(!IsCurrentPreparation(Current, FPreparationToken{44, 8, 12, 0, 777}),
        "零状态修订令牌必须拒绝");
}

void TestBoundedQueue()
{
    FWaitingQueue Queue(2);
    Expect(Queue.TryAdd(1001, 10.0), "队列应接纳第一个合法玩家");
    Expect(Queue.TryAdd(1002, 20.0), "队列应接纳容量内的第二个玩家");
    Expect(!Queue.TryAdd(1003, 30.0), "队列满时必须明确拒绝而非无限增长");
    Expect(!Queue.TryAdd(1001, 40.0), "重复玩家不得占用第二个槽位");
    Expect(Queue.Remove(1001), "离开应精确移除自己的等待记录");
    Expect(!Queue.Remove(1001), "重复移除必须幂等且不影响其他玩家");
    Expect(Queue.TryAdd(1003, 30.0), "释放槽位后应允许另一个合法玩家进入");

    const std::vector<std::uint64_t> Expired = Queue.CollectExpired(25.0);
    Expect(Expired.size() == 1 && Expired[0] == 1002,
        "截止扫描只返回已到期记录并保持入队顺序");
    Expect(Queue.Size() == 1, "到期记录应从队列清理");
    Expect(Queue.CollectExpired(25.0).empty(), "同一到期记录不得重复完成");
    Expect(!Queue.TryAdd(0, 50.0), "零玩家代次必须拒绝");
    Expect(!Queue.TryAdd(1004, 0.0), "非正截止时间必须拒绝");
}

void TestDeterministicSpawnOrdering()
{
    std::vector<FSpawnCandidate> Candidates{
        {5, 3, true, true, false},
        {2, 1, true, true, false},
        {8, 0, false, true, false},
        {7, 0, true, false, false},
        {3, 2, true, true, true},
        {1, 1, true, true, false}
    };
    const std::vector<std::uint64_t> Ordered = OrderEligibleSpawnCandidates(Candidates);
    Expect(Ordered == std::vector<std::uint64_t>({1, 2, 5}),
        "合法候选应先按优先级、再按稳定身份确定排序");
}
}

int main()
{
    TestExperienceTransitions();
    TestPlayerTransitions();
    TestAdmissionGeneration();
    TestSpawnIdempotency();
    TestPreparationToken();
    TestBoundedQueue();
    TestDeterministicSpawnOrdering();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " gameplay policy assertion(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All gameplay policy tests passed.\n";
    return EXIT_SUCCESS;
}
#endif
