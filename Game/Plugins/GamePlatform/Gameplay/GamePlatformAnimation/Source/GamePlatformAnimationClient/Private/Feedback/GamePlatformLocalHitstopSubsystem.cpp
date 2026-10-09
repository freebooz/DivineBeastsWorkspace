#include "Feedback/GamePlatformLocalHitstopSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"

namespace
{
// 纯客户端测试旋钮：-1使用数据资产，0关闭，3/6用于相同技能的AB手感对比；绝不修改服务器时间或伤害。
TAutoConsoleVariable<int32> CVarGamePlatformHitstopOverrideFrames(
    TEXT("gp.Combat.HitstopOverrideFrames"),
    -1,
    TEXT("客户端打击感顿帧覆盖帧数：-1=Profile，0=关闭，3/6=对照；60Hz参考帧，上限10，不影响Gameplay。"),
    ECVF_Cheat);
}

int32 UGamePlatformLocalHitstopSubsystem::ResolveVisualHitstopFrames(
    int32 ConfiguredFrames, int32 OverrideFrames)
{
    return FMath::Clamp(OverrideFrames < 0 ? ConfiguredFrames : OverrideFrames, 0, MaxVisualFrames);
}


void UGamePlatformLocalHitstopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &UGamePlatformLocalHitstopSubsystem::HandleWorldCleanup);
}

void UGamePlatformLocalHitstopSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
    CancelAllVisualHitstops();
    Super::Deinitialize();
}

bool UGamePlatformLocalHitstopSubsystem::ApplyVisualHitstop(
    const FGuid& EventId,
    USkeletalMeshComponent* SourceMesh,
    USkeletalMeshComponent* TargetMesh,
    int32 Frames)
{
    // 局部效果必须由游戏线程的本地玩家发起；专服及ListenServer的权威网格不可冻结。
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (!IsInGameThread() || !World ||
        (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone) ||
        !EventId.IsValid() || RecentEventIds.Contains(EventId))
    {
        return false;
    }

    const int32 SafeFrames = ResolveVisualHitstopFrames(
        Frames, CVarGamePlatformHitstopOverrideFrames.GetValueOnGameThread());
    if (SafeFrames <= 0)
    {
        return false; // 0帧仅关闭该表现层；VFX、SFX、镜头和GAS仍继续执行。
    }

    if ((!IsValid(SourceMesh) || SourceMesh->GetWorld() != World) &&
        (!IsValid(TargetMesh) || TargetMesh->GetWorld() != World))
    {
        return false;
    }

    if (BoundWorld.IsValid() && BoundWorld.Get() != World)
    {
        // 旧世界定时器不得在新世界中重新恢复或控制同名/复用网格。
        CancelAllVisualHitstops();
    }
    BoundWorld = World;

    // 帧数只作为60Hz设计单位；独立于显示器渲染帧率。
    // 局部时钟使用单调实时时间，避免其他Gameplay慢动作或暂停修改本次顿帧的时长。
    const double DeadlineSeconds = FPlatformTime::Seconds() + SafeFrames / ReferenceFps;
    bool bAccepted = false;
    if (IsValid(SourceMesh) && SourceMesh->GetWorld() == World)
    {
        bAccepted = ApplyToMesh(SourceMesh, *World, DeadlineSeconds);
    }
    if (IsValid(TargetMesh) && TargetMesh != SourceMesh &&
        TargetMesh->GetWorld() == World)
    {
        bAccepted = ApplyToMesh(TargetMesh, *World, DeadlineSeconds) || bAccepted;
    }
    if (!bAccepted)
    {
        return false; // 未实际暂停任何网格，不伪称已完成表现。
    }

    // 有界事实缓存防止预测确认、多次通知或多段装配误触发同一次视觉顿帧。
    RecentEventIds.Add(EventId);
    RecentEventOrder.Add(EventId);
    if (RecentEventOrder.Num() > MaxRecentEvents)
    {
        RecentEventIds.Remove(RecentEventOrder[0]);
        RecentEventOrder.RemoveAt(0);
    }
    return true;
}

bool UGamePlatformLocalHitstopSubsystem::ApplyToMesh(
    USkeletalMeshComponent* Mesh,
    UWorld& World,
    double DeadlineSeconds)
{
    if (!IsValid(Mesh) || Mesh->GetWorld() != &World)
    {
        return false;
    }
    const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(Mesh);
    FPausedMeshRecord* Existing = ActiveMeshes.Find(MeshKey);
    if (!Existing)
    {
        // 当前动画驱动真实RootMotion时，暂停网格会使客户端预测与服务器权威位移分离。
        // 在独立视觉代理与RootMotion同步方案实际联机验收前，保守跳过该次停顿。
        if (const ACharacter* Character = Cast<ACharacter>(Mesh->GetOwner()))
        {
            if (Character->IsPlayingRootMotion())
            {
                return false;
            }
        }
        if (Mesh->bPauseAnims)
        {
            return false; // 该动画已由其他系统暂停，不能冒领其状态所有权。
        }

        FPausedMeshRecord Record;
        Record.Mesh = Mesh;
        Record.World = &World;
        Record.FirstPausedAtSeconds = FPlatformTime::Seconds();
        Record.bWasAnimsPaused = Mesh->bPauseAnims;
        ActiveMeshes.Add(MeshKey, MoveTemp(Record));
        Existing = ActiveMeshes.Find(MeshKey);
        Mesh->bPauseAnims = true;
    }

    // 新命中仅延长到本次连续暂停窗口的上限，不将多个命中帧数累加。
    // 避免高频多段攻击不断刷新，导致动画永久无法恢复。
    const double AbsoluteWindowLimit =
        Existing->FirstPausedAtSeconds + MaxVisualFrames / ReferenceFps;
    const double SafeDeadline = FMath::Min(DeadlineSeconds, AbsoluteWindowLimit);
    if (Existing->DeadlineSeconds >= SafeDeadline)
    {
        return true; // 已有视觉停顿覆盖本次时长，当前网格仍属于本子系统。
    }
    Existing->DeadlineSeconds = SafeDeadline;

    // CoreTicker依据真实经过时间驱动，仅在活动顿帧期间工作。
    // 使用每Mesh截止时间而非TimerManager的世界时间，可避免全局时间膨胀导致的停顿拖长。
    if (!ActiveTickHandle.IsValid())
    {
        ActiveTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this, &UGamePlatformLocalHitstopSubsystem::TickVisualHitstop));
    }
    return true;
}

void UGamePlatformLocalHitstopSubsystem::RestoreMesh(
    TWeakObjectPtr<USkeletalMeshComponent> MeshKey, bool bBroadcastFinished)
{
    FPausedMeshRecord* Record = ActiveMeshes.Find(MeshKey);
    if (!Record)
    {
        return;
    }

    // 只恢复本世界仍存在的原始网格；销毁或跨世界网格仅回收弱引用。
    USkeletalMeshComponent* ValidMesh = Record->Mesh.Get();
    const bool bSameWorld = IsValid(ValidMesh) &&
        ValidMesh->GetWorld() == Record->World.Get();
    const bool bRestoreOriginalState = Record->bWasAnimsPaused;
    ActiveMeshes.Remove(MeshKey);
    if (bSameWorld)
    {
        ValidMesh->bPauseAnims = bRestoreOriginalState;
        if (bBroadcastFinished)
        {
            // 记录已移除后再通知上层Input；事件消费者不能恢复/移除本条记录。
            VisualHitstopFinished.Broadcast(ValidMesh);
        }
    }
}

/**
 * CoreTicker是按需启用的局部视觉时钟；不暂停Actor、World或GAS，
 * 回调末尾返回false会注销自身，防止留下常驻Tick。
 */
bool UGamePlatformLocalHitstopSubsystem::TickVisualHitstop(float /*DeltaSeconds*/)
{
    const double NowSeconds = FPlatformTime::Seconds();
    TArray<TWeakObjectPtr<USkeletalMeshComponent>> ExpiredMeshes;
    for (const TPair<TWeakObjectPtr<USkeletalMeshComponent>, FPausedMeshRecord>& Pair : ActiveMeshes)
    {
        if (!Pair.Key.IsValid() || !Pair.Value.World.IsValid() ||
            NowSeconds >= Pair.Value.DeadlineSeconds)
        {
            ExpiredMeshes.Add(Pair.Key);
        }
    }
    for (const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey : ExpiredMeshes)
    {
        RestoreMesh(MeshKey);
    }
    if (ActiveMeshes.IsEmpty())
    {
        ActiveTickHandle.Reset();
        return false; // 停顿结束后不保留帧更新。
    }
    return true;
}

bool UGamePlatformLocalHitstopSubsystem::IsVisualHitstopActive(
    const USkeletalMeshComponent* Mesh) const
{
    return IsValid(Mesh) &&
        ActiveMeshes.Contains(TWeakObjectPtr<USkeletalMeshComponent>(
            const_cast<USkeletalMeshComponent*>(Mesh)));
}

void UGamePlatformLocalHitstopSubsystem::CancelAllVisualHitstops()
{
    if (ActiveTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ActiveTickHandle);
        ActiveTickHandle.Reset();
    }
    // 拷贝键列表，避免恢复时修改正在遍历的容器。
    TArray<TWeakObjectPtr<USkeletalMeshComponent>> MeshKeys;
    ActiveMeshes.GetKeys(MeshKeys);
    for (const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey : MeshKeys)
    {
        RestoreMesh(MeshKey, false); // 世界清理/取消不回放技能输入。
    }
    ActiveMeshes.Reset();
    RecentEventIds.Reset();
    RecentEventOrder.Reset();
    BoundWorld.Reset();
}

void UGamePlatformLocalHitstopSubsystem::HandleWorldCleanup(
    UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (World && World == BoundWorld.Get())
    {
        CancelAllVisualHitstops();
    }
}
