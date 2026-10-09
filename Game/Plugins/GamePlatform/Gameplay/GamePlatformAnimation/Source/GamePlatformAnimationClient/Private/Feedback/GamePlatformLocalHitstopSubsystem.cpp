#include "Feedback/GamePlatformLocalHitstopSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"

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
        !EventId.IsValid() || Frames <= 0 || RecentEventIds.Contains(EventId))
    {
        return false;
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
    const int32 SafeFrames = FMath::Clamp(Frames, 0, MaxVisualFrames);
    // 局部时钟使用单调实时时间，避免其他Gameplay慢动作或暂停修改本次顿帧的时长。
    const double DeadlineSeconds = FPlatformTime::Seconds() + SafeFrames / ReferenceFps;
    if (IsValid(SourceMesh) && SourceMesh->GetWorld() == World)
    {
        ApplyToMesh(SourceMesh, *World, DeadlineSeconds);
    }
    if (IsValid(TargetMesh) && TargetMesh != SourceMesh &&
        TargetMesh->GetWorld() == World)
    {
        ApplyToMesh(TargetMesh, *World, DeadlineSeconds);
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

void UGamePlatformLocalHitstopSubsystem::ApplyToMesh(
    USkeletalMeshComponent* Mesh,
    UWorld& World,
    double DeadlineSeconds)
{
    if (!IsValid(Mesh))
    {
        return;
    }
    const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(Mesh);
    FPausedMeshRecord* Existing = ActiveMeshes.Find(MeshKey);
    if (Existing && Existing->DeadlineSeconds >= DeadlineSeconds)
    {
        return; // 新命中不能缩短已有剩余时间，也不能将时长逐次相加。
    }

    if (!Existing)
    {
        FPausedMeshRecord Record;
        Record.Mesh = Mesh;
        Record.World = &World;
        Record.bWasAnimsPaused = Mesh->bPauseAnims;
        ActiveMeshes.Add(MeshKey, MoveTemp(Record));
        Existing = ActiveMeshes.Find(MeshKey);
        Mesh->bPauseAnims = true;
    }

    Existing->DeadlineSeconds = DeadlineSeconds;

    // CoreTicker依据真实经过时间驱动，仅在活动顿帧期间工作。
    // 使用每Mesh截止时间而非TimerManager的世界时间，可避免全局时间膨胀导致的停顿拖长。
    if (!ActiveTickHandle.IsValid())
    {
        ActiveTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this, &UGamePlatformLocalHitstopSubsystem::TickVisualHitstop));
    }
}

void UGamePlatformLocalHitstopSubsystem::RestoreMesh(
    TWeakObjectPtr<USkeletalMeshComponent> MeshKey)
{
    FPausedMeshRecord* Record = ActiveMeshes.Find(MeshKey);
    if (!Record)
    {
        return;
    }

    // 只恢复本世界仍存在的原始网格；销毁或跨世界网格仅回收弱引用。
    if (USkeletalMeshComponent* Mesh = Record->Mesh.Get())
    {
        if (Mesh->GetWorld() == Record->World.Get())
        {
            Mesh->bPauseAnims = Record->bWasAnimsPaused;
        }
    }
    ActiveMeshes.Remove(MeshKey);
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
        RestoreMesh(MeshKey);
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
