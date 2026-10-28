// 双端世界天气服务：服务器写入、客户端读取，定时器只在自动天气开启期间使用。
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Actors/GamePlatformWeatherReplicator.h"
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "API/GamePlatformWeatherWindSource.h"
#include "Engine/WindDirectionalSource.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
    ScheduleGeneration = 0;
    CurrentSnapshot = FGamePlatformWeatherSnapshot();
    InitialSceneWind.Reset(); bSceneWindCaptured = false; bInitialPresetPending = false;
}

void UGamePlatformWeatherWorldSubsystem::Deinitialize()
{
    bClosing = true;
    StopScheduleInternal();
    SnapshotChanged.Clear();
    Schedule.Empty();
    Replicator.Reset();
    InitialSceneWind.Reset(); bInitialPresetPending = false;
    Super::Deinitialize();
}

bool UGamePlatformWeatherWorldSubsystem::IsAuthoritativeWeatherWorld() const
{
    const UWorld* World = GetWorld();
    return !bClosing && IsValid(World) && !World->bIsTearingDown &&
        World->GetNetMode() != NM_Client && !IsRunningCommandlet();
}

bool UGamePlatformWeatherWorldSubsystem::ActivateWeatherOwned(const FGuid& Scope,const FGamePlatformWeatherState& InitialState)
{
    if(bEndingDevelopment||Scope!=DevelopmentScope)return false;
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || !InitialState.IsValid()) return false;
    if (Replicator.IsValid()) return true;
    UWorld* World = GetWorld();
    auto Initial = InitialState;
    if (!bSceneWindCaptured)
    {
        AWindDirectionalSource* Source = nullptr; FString Error;
        const auto Result = GamePlatformWeatherWindSource::Read(*World, Initial, Source, Error);
        if (Result == EGamePlatformSceneWindResult::Invalid)
        { UE_LOG(LogTemp, Error, TEXT("InitialWeatherSceneWindRejected: %s"), *Error); return false; }
        bSceneWindCaptured = true;
        if (Result == EGamePlatformSceneWindResult::Applied)
        {
            InitialSceneWind = Initial;
            UE_LOG(LogTemp, Display, TEXT("SceneWindInitialPreset source=%s yaw=%.3f strength=%.3f gust=%.3f frequencyHz=%.3f"),
                *Source->GetPathName(), Initial.WindYawDegrees, Initial.WindIntensity, Initial.WindGustStrength, Initial.WindGustFrequencyHz);
        }
    }
    if (InitialSceneWind.IsSet())
    {
        const auto& Authored = InitialSceneWind.GetValue();
        Initial.WindYawDegrees = Authored.WindYawDegrees; Initial.WindIntensity = Authored.WindIntensity;
        Initial.WindGustStrength = Authored.WindGustStrength; Initial.WindGustFrequencyHz = Authored.WindGustFrequencyHz;
    }
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGamePlatformWeatherReplicator* Created = World->SpawnActor<AGamePlatformWeatherReplicator>(
        AGamePlatformWeatherReplicator::StaticClass(), FTransform::Identity, Parameters);
    if (!IsValid(Created)) return false;
    if(Scope!=DevelopmentScope||bEndingDevelopment||!IsAuthoritativeWeatherWorld()||Replicator.IsValid()){Created->Destroy();return false;}
    Replicator = Created;
    const FGamePlatformWeatherQuantizedState Quantized = FGamePlatformWeatherQuantizedState::Encode(Initial);
    bInitialPresetPending = true;
    FGamePlatformWeatherSnapshot Snapshot;
    Snapshot.From = Quantized;
    Snapshot.To = Quantized;
    Snapshot.StartedAtServerSeconds = World->GetTimeSeconds();
    Snapshot.TransitionSeconds = 0.f;
    Snapshot.Revision = 1;
    if (!Created->PublishAuthoritativeSnapshot(Snapshot))
    {
        Created->Destroy();
        if(Replicator.Get()==Created)Replicator.Reset();
        return false;
    }
    return Scope==DevelopmentScope&&Replicator.Get()==Created&&IsAuthoritativeWeatherWorld();
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
    Next.WindSeed = CurrentSnapshot.WindSeed;
    Next.WindPhaseAtStartCycles = CurrentSnapshot.SampleWindPhaseCycles(CurrentServerTime);
    Next.Revision = CurrentSnapshot.Revision + 1;
    bInitialPresetPending = false; // 显式天气/调度也消费初始化优先级，先于同步通知重入。
    return Replicator->PublishAuthoritativeSnapshot(Next);
}

bool UGamePlatformWeatherWorldSubsystem::SetWeatherOwned(
    const FGuid& Scope,const FGamePlatformWeatherState& TargetState, const float TransitionSeconds)
{
    if(bEndingDevelopment||Scope!=DevelopmentScope)return false;
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || !TargetState.IsValid() ||
        !FMath::IsFinite(TransitionSeconds) || TransitionSeconds < 0.f || TransitionSeconds > 3600.f)
        return false;
    if (!Replicator.IsValid() && !ActivateWeatherOwned(Scope,FGamePlatformWeatherState())) return false;
    StopScheduleInternal(); // 人工明确指令优先于自动周期。
    return PublishTransition(TargetState, TransitionSeconds);
}

bool UGamePlatformWeatherWorldSubsystem::ApplyPresetOwned(const FGuid& Scope,const UGamePlatformWeatherPresetDefinition* Preset)
{
    if(bEndingDevelopment||Scope!=DevelopmentScope)return false;
    check(IsInGameThread());
    if (!IsValid(Preset) || !Preset->ValidateDefinition().IsSuccess()) return false;
    if (!IsAuthoritativeWeatherWorld()) return false;
    if (!Replicator.IsValid() && !ActivateWeatherOwned(Scope,FGamePlatformWeatherState())) return false;
    auto Target = Preset->Weather.State;
    if (bInitialPresetPending && InitialSceneWind.IsSet())
    {
        const auto& Authored = InitialSceneWind.GetValue();
        Target.WindYawDegrees = Authored.WindYawDegrees; Target.WindIntensity = Authored.WindIntensity;
        Target.WindGustStrength = Authored.WindGustStrength; Target.WindGustFrequencyHz = Authored.WindGustFrequencyHz;
    }
    return SetWeatherOwned(Scope,Target, Preset->Weather.TransitionSeconds);
}

bool UGamePlatformWeatherWorldSubsystem::ConfigureScheduleOwned(
    const FGuid& Scope,const TArray<FGamePlatformWeatherScheduleEntry>& Entries, const int32 RandomSeed)
{
    if(bEndingDevelopment||Scope!=DevelopmentScope)return false;
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || Entries.IsEmpty() || Entries.Num() > 32) return false;
    for (const FGamePlatformWeatherScheduleEntry& Entry : Entries)
        if (!Entry.IsValid()) return false;
    StopScheduleInternal();
    Schedule = Entries;
    Random.Initialize(RandomSeed);
    return true;
}

bool UGamePlatformWeatherWorldSubsystem::StartScheduleOwned(const FGuid& Scope)
{
    if(bEndingDevelopment||Scope!=DevelopmentScope)return false;
    check(IsInGameThread());
    if (!IsAuthoritativeWeatherWorld() || Schedule.IsEmpty()) return false;
    if (!Replicator.IsValid() && !ActivateWeatherOwned(Scope,FGamePlatformWeatherState())) return false;
    if (bScheduleRunning) return true;
    bScheduleRunning = true;
    RunNextSchedule();
    return bScheduleRunning;
}

void UGamePlatformWeatherWorldSubsystem::StopScheduleInternal()
{
    // 发布天气时SnapshotChanged可能同步调用停止/重配/重启；旧调度栈必须以代次失效。
    ++ScheduleGeneration;
    bScheduleRunning = false;
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(ScheduleTimer);
}

void UGamePlatformWeatherWorldSubsystem::RunNextSchedule()
{
    if (!bScheduleRunning || !IsAuthoritativeWeatherWorld() || Schedule.IsEmpty())
    {
        StopScheduleInternal();
        return;
    }
    int32 WeightSum = 0;
    for (const auto& Entry : Schedule) WeightSum += Entry.Weight;
    if (WeightSum <= 0) { StopScheduleInternal(); return; }
    int32 Choice = Random.RandRange(1, WeightSum);
    const FGamePlatformWeatherScheduleEntry* Selected = nullptr;
    for (const auto& Entry : Schedule)
    {
        Choice -= Entry.Weight;
        if (Choice <= 0) { Selected = &Entry; break; }
    }
    if (!Selected) { StopScheduleInternal(); return; }
    // 广播会同步调用外部观察者：发布前复制所选值并取随机延迟，禁止持有Schedule元素指针跨过回调。
    const FGamePlatformWeatherScheduleEntry SelectedEntry = *Selected;
    const float HoldSeconds = Random.FRandRange(SelectedEntry.MinHoldSeconds, SelectedEntry.MaxHoldSeconds);
    const float DelaySeconds = FMath::Max(HoldSeconds, SelectedEntry.TransitionSeconds + 0.1f);
    const uint64 ExpectedGeneration = ScheduleGeneration;
    if (!PublishTransition(SelectedEntry.State, SelectedEntry.TransitionSeconds))
    {
        if (bScheduleRunning && ScheduleGeneration == ExpectedGeneration) StopScheduleInternal();
        return;
    }
    // 回调可能已配置并启动另一轮计划；不能把旧计划的定时器覆盖新定时器。
    if (!bScheduleRunning || ScheduleGeneration != ExpectedGeneration || !IsAuthoritativeWeatherWorld()) return;
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
    if(DevelopmentScope.IsValid()&&(!Replicator.IsValid()||Replicator.Get()!=Source))return;
    check(IsInGameThread());
    if (bClosing || !IsValid(Source) || Source->GetWorld() != GetWorld() ||
        (Replicator.IsValid() && Replicator.Get() != Source) || Snapshot.Revision <= CurrentSnapshot.Revision ||
        !FMath::IsFinite(Snapshot.StartedAtServerSeconds) ||
        !FMath::IsFinite(Snapshot.TransitionSeconds) ||
        Snapshot.TransitionSeconds < 0.f || Snapshot.TransitionSeconds > 3600.f ||
        !FMath::IsFinite(Snapshot.WindPhaseAtStartCycles) || Snapshot.WindPhaseAtStartCycles < 0.f ||
        Snapshot.WindPhaseAtStartCycles >= 1.f ||
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
        StopScheduleInternal();
        Replicator.Reset();
        CurrentSnapshot = FGamePlatformWeatherSnapshot();
        // 同一世界Actor意外销毁时广播显式空状态，避免新Actor从Revision=1重启却被客户端旧代次拒绝。
        const FGamePlatformWeatherSnapshot ResetSnapshot;
        SnapshotChanged.Broadcast(ResetSnapshot);
    }
}

bool UGamePlatformWeatherWorldSubsystem::ActivateWeather(const FGamePlatformWeatherState& V){return ActivateWeatherOwned({},V);}
bool UGamePlatformWeatherWorldSubsystem::SetWeather(const FGamePlatformWeatherState& V,float S){return SetWeatherOwned({},V,S);}
bool UGamePlatformWeatherWorldSubsystem::ApplyPreset(const UGamePlatformWeatherPresetDefinition* V){return ApplyPresetOwned({},V);}
bool UGamePlatformWeatherWorldSubsystem::ConfigureSchedule(const TArray<FGamePlatformWeatherScheduleEntry>& V,int32 S){return ConfigureScheduleOwned({},V,S);}
bool UGamePlatformWeatherWorldSubsystem::StartSchedule(){return StartScheduleOwned({});}
void UGamePlatformWeatherWorldSubsystem::StopSchedule(){if(!DevelopmentScope.IsValid())StopScheduleInternal();}
bool UGamePlatformWeatherWorldSubsystem::BeginDevelopmentWeather(FGuid Owner,FGuid& OutScope){
 OutScope.Invalidate();
#if UE_BUILD_SHIPPING
 return false;
#else
 auto* W=GetWorld();if(!W||W->GetNetMode()!=NM_Standalone||W->WorldType!=EWorldType::Game||W->bIsTearingDown||!FParse::Param(FCommandLine::Get(),TEXT("DBAPluginDemo"))||!FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")))return false;
#endif

 if(!Owner.IsValid()||bClosing||bEndingDevelopment||DevelopmentScope.IsValid()||Replicator.IsValid()||CurrentSnapshot.Revision!=0||!Schedule.IsEmpty()||bScheduleRunning||bSceneWindCaptured||InitialSceneWind.IsSet())return false;
 DevelopmentScope=FGuid::NewGuid();OutScope=DevelopmentScope;return true;
}
bool UGamePlatformWeatherWorldSubsystem::DevelopmentWeather(const FGuid& Scope,FName Op,const FGamePlatformWeatherState& V,const TArray<FGamePlatformWeatherScheduleEntry>& Entries,int32 Seed,float Seconds){
 if(!Scope.IsValid()||Scope!=DevelopmentScope||bEndingDevelopment)return false;
 if(Op==TEXT("Activate"))return ActivateWeatherOwned(Scope,V);
 if(Op==TEXT("SetWeather")||Op==TEXT("ApplyPreset"))return SetWeatherOwned(Scope,V,Seconds);
 if(Op==TEXT("ConfigureSchedule"))return ConfigureScheduleOwned(Scope,Entries,Seed);
 if(Op==TEXT("StartSchedule"))return StartScheduleOwned(Scope);
 if(Op==TEXT("StopSchedule")){StopScheduleInternal();return true;}return false;
}
bool UGamePlatformWeatherWorldSubsystem::EndDevelopmentWeather(const FGuid& Scope){
 if(!Scope.IsValid()||Scope!=DevelopmentScope||bEndingDevelopment)return false;TGuardValue<bool> Guard(bEndingDevelopment,true);
 StopScheduleInternal();Schedule.Reset();
 if(auto* Own=Replicator.Get()){if(!Own->Destroy())return false;}
 if(Replicator.IsValid())return false;
 CurrentSnapshot={};InitialSceneWind.Reset();bSceneWindCaptured=false;bInitialPresetPending=false;DevelopmentScope.Invalidate();return true;
}
