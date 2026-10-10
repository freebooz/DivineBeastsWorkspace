// 双端世界天气服务：服务器写入、客户端读取，定时器只在自动天气开启期间使用。
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Actors/GamePlatformWeatherReplicator.h"
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"

bool UGamePlatformWeatherWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && !IsRunningCommandlet();
}

void UGamePlatformWeatherWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    bScheduleRunning = false;
    CurrentSnapshot = FGamePlatformWeatherSnapshot();
}

void UGamePlatformWeatherWorldSubsystem::Deinitialize()
{
    bClosing = true;
    StopSchedule();
    SnapshotChanged.Clear();
    Schedule.Empty();
    Replicator.Reset();
    Super::Deinitialize();
}

bool UGamePlatformWeatherWorldSubsystem::IsAuthoritativeWeatherWorld() const
{
    const UWorld* World = GetWorld();
    return !bClosing && IsValid(World) && !World->bIsTearingDown &&
        World->GetNetMode() != NM_Client && !IsRunningCommandlet();
}

bool UGamePlatformWeatherWorldSubsystem::ActivateWeather(const FGamePlatformWeatherState& InitialState)
{
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || !InitialState.IsValid()) return false;
    if (Replicator.IsValid()) return true;
    UWorld* World = GetWorld();
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGamePlatformWeatherReplicator* Created = World->SpawnActor<AGamePlatformWeatherReplicator>(
        AGamePlatformWeatherReplicator::StaticClass(), FTransform::Identity, Parameters);
    if (!IsValid(Created)) return false;
    Replicator = Created;
    const FGamePlatformWeatherQuantizedState Quantized = FGamePlatformWeatherQuantizedState::Encode(InitialState);
    FGamePlatformWeatherSnapshot Snapshot;
    Snapshot.From = Quantized;
    Snapshot.To = Quantized;
    Snapshot.StartedAtServerSeconds = World->GetTimeSeconds();
    Snapshot.TransitionSeconds = 0.f;
    Snapshot.Revision = 1;
    if (!Created->PublishAuthoritativeSnapshot(Snapshot))
    {
        Created->Destroy();
        Replicator.Reset();
        return false;
    }
    return true;
}

bool UGamePlatformWeatherWorldSubsystem::PublishTransition(
    const FGamePlatformWeatherState& TargetState, const float TransitionSeconds)
{
    if (!IsAuthoritativeWeatherWorld() || !Replicator.IsValid() || !TargetState.IsValid() ||
        !FMath::IsFinite(TransitionSeconds) || TransitionSeconds < 0.f || TransitionSeconds > 3600.f ||
        CurrentSnapshot.Revision == MAX_int32) return false;
    const float CurrentServerTime = GetWorld()->GetTimeSeconds();
    FGamePlatformWeatherSnapshot Next;
    Next.From = FGamePlatformWeatherQuantizedState::Encode(CurrentSnapshot.Sample(CurrentServerTime));
    Next.To = FGamePlatformWeatherQuantizedState::Encode(TargetState);
    Next.StartedAtServerSeconds = CurrentServerTime;
    Next.TransitionSeconds = TransitionSeconds;
    Next.Revision = CurrentSnapshot.Revision + 1;
    return Replicator->PublishAuthoritativeSnapshot(Next);
}

bool UGamePlatformWeatherWorldSubsystem::SetWeather(
    const FGamePlatformWeatherState& TargetState, const float TransitionSeconds)
{
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || !TargetState.IsValid() ||
        !FMath::IsFinite(TransitionSeconds) || TransitionSeconds < 0.f || TransitionSeconds > 3600.f)
        return false;
    if (!Replicator.IsValid() && !ActivateWeather(FGamePlatformWeatherState())) return false;
    StopSchedule(); // 人工明确指令优先于自动周期。
    return PublishTransition(TargetState, TransitionSeconds);
}

bool UGamePlatformWeatherWorldSubsystem::ApplyPreset(const UGamePlatformWeatherPresetDefinition* Preset)
{
    check(IsInGameThread());
    if (!IsValid(Preset) || !Preset->ValidateDefinition().IsSuccess()) return false;
    return SetWeather(Preset->Weather.State, Preset->Weather.TransitionSeconds);
}

bool UGamePlatformWeatherWorldSubsystem::ConfigureSchedule(
    const TArray<FGamePlatformWeatherScheduleEntry>& Entries, const int32 RandomSeed)
{
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || Entries.IsEmpty() || Entries.Num() > 32) return false;
    for (const FGamePlatformWeatherScheduleEntry& Entry : Entries)
        if (!Entry.IsValid()) return false;
    StopSchedule();
    Schedule = Entries;
    Random.Initialize(RandomSeed);
    return true;
}

bool UGamePlatformWeatherWorldSubsystem::StartSchedule()
{
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || Schedule.IsEmpty()) return false;
    if (!Replicator.IsValid() && !ActivateWeather(FGamePlatformWeatherState())) return false;
    if (bScheduleRunning) return true;
    bScheduleRunning = true;
    RunNextSchedule();
    return bScheduleRunning;
}

void UGamePlatformWeatherWorldSubsystem::StopSchedule()
{
    bScheduleRunning = false;
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(ScheduleTimer);
}

void UGamePlatformWeatherWorldSubsystem::RunNextSchedule()
{
    if (!bScheduleRunning || !IsAuthoritativeWeatherWorld() || Schedule.IsEmpty())
    {
        StopSchedule();
        return;
    }
    int32 WeightSum = 0;
    for (const auto& Entry : Schedule) WeightSum += Entry.Weight;
    if (WeightSum <= 0) { StopSchedule(); return; }
    int32 Choice = Random.RandRange(1, WeightSum);
    const FGamePlatformWeatherScheduleEntry* Selected = nullptr;
    for (const auto& Entry : Schedule)
    {
        Choice -= Entry.Weight;
        if (Choice <= 0) { Selected = &Entry; break; }
    }
    if (!Selected || !PublishTransition(Selected->State, Selected->TransitionSeconds))
    {
        StopSchedule();
        return;
    }
    const float HoldSeconds = Random.FRandRange(Selected->MinHoldSeconds, Selected->MaxHoldSeconds);
    const float DelaySeconds = FMath::Max(HoldSeconds, Selected->TransitionSeconds + 0.1f);
    GetWorld()->GetTimerManager().SetTimer(ScheduleTimer, this,
        &UGamePlatformWeatherWorldSubsystem::RunNextSchedule, DelaySeconds, false);
}

FDelegateHandle UGamePlatformWeatherWorldSubsystem::AddSnapshotHandler(FGamePlatformWeatherSnapshotChanged::FDelegate Handler)
{
    check(IsInGameThread());
    return !bClosing && Handler.IsBound() ? SnapshotChanged.Add(MoveTemp(Handler)) : FDelegateHandle();
}

void UGamePlatformWeatherWorldSubsystem::RemoveSnapshotHandler(const FDelegateHandle Handle)
{
    check(IsInGameThread());
    if (Handle.IsValid()) SnapshotChanged.Remove(Handle);
}

void UGamePlatformWeatherWorldSubsystem::ObserveReplicator(
    const AGamePlatformWeatherReplicator* Source, const FGamePlatformWeatherSnapshot& Snapshot)
{
    check(IsInGameThread());
    if (bClosing || !IsValid(Source) || Source->GetWorld() != GetWorld() ||
        (Replicator.IsValid() && Replicator.Get() != Source) || Snapshot.Revision <= CurrentSnapshot.Revision ||
        !FMath::IsFinite(Snapshot.StartedAtServerSeconds) ||
        !FMath::IsFinite(Snapshot.TransitionSeconds) ||
        Snapshot.TransitionSeconds < 0.f || Snapshot.TransitionSeconds > 3600.f ||
        !Snapshot.From.Decode().IsValid() || !Snapshot.To.Decode().IsValid())
        return;
    Replicator = const_cast<AGamePlatformWeatherReplicator*>(Source);
    CurrentSnapshot = Snapshot;
    // 事件按值持有当前快照，重入回调再次改变天气时不污染当前通知的事实。
    const FGamePlatformWeatherSnapshot PublishedSnapshot = CurrentSnapshot;
    SnapshotChanged.Broadcast(PublishedSnapshot);
}

void UGamePlatformWeatherWorldSubsystem::ForgetReplicator(const AGamePlatformWeatherReplicator* Source)
{
    check(IsInGameThread());
    if (!bClosing && Replicator.Get() == Source)
    {
        StopSchedule();
        Replicator.Reset();
        CurrentSnapshot = FGamePlatformWeatherSnapshot();
        // 同一世界Actor意外销毁时广播显式空状态，避免新Actor从Revision=1重启却被客户端旧代次拒绝。
        const FGamePlatformWeatherSnapshot ResetSnapshot;
        SnapshotChanged.Broadcast(ResetSnapshot);
    }
}
