// 本文件属于GamePlatform平台层 GamePlatformSFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 平台客户端World音频服务：持有自有Data租约和AudioComponent，事件驱动播放/取消/终态及世界退出回收。
// 仅Game/PIE/GamePreview非专服世界运行；可选表现失败不改变网络权威事实。
#include "Subsystems/GamePlatformSFXWorldSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Definitions/GamePlatformSFXDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "AudioDevice.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Policy/GamePlatformSFXPolicy.h"
#include "Policy/GamePlatformSFXBudgetPolicy.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSFX, Log, All);

namespace
{
const FName SFXRuntimeBundle(TEXT("SFXRuntime")); // FName依赖运行时名称池，不能声明为constexpr常量。
constexpr int32 MaxTerminalSFXRequests = 512;
constexpr double TerminalSFXRetentionSeconds = 30.0;

bool IsSupportedWorldType(const EWorldType::Type WorldType)
{
    return WorldType == EWorldType::Game ||
           WorldType == EWorldType::PIE ||
           WorldType == EWorldType::GamePreview;
}
}

bool UGamePlatformSFXWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) &&
           IsSupportedWorldType(World->WorldType) &&
           World->GetNetMode() != NM_DedicatedServer &&
           !IsRunningCommandlet();
}

void UGamePlatformSFXWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    Generation = FMath::Max(1, Generation);
}

void UGamePlatformSFXWorldSubsystem::Deinitialize()
{
    if (bClosing) return; // 终态监听可以重入关闭，第一次清理栈负责清理全部自有账本。
    bClosing = true;
    ++Generation;

    TArray<FGuid> ActiveIds;
    ActiveInstances.GetKeys(ActiveIds);
    for (const FGuid& Id : ActiveIds)
    {
        CleanupActive(Id, true);
    }

    TArray<FGuid> PendingIds; PendingPlays.GetKeys(PendingIds);
    for (const auto& Id : PendingIds) FailPending(Id, EGamePlatformSFXResultCode::InvalidWorld);
    PlaybackCompleted.Clear();
    RequestHandles.Reset();
    TerminalOccurrences.Reset();

    Super::Deinitialize();
}

FGamePlatformSFXResult UGamePlatformSFXWorldSubsystem::Play(
    const FGamePlatformSFXRequest& Request)
{
    check(IsInGameThread());
    ++Diagnostics.PlayRequests;
    const TStrongObjectPtr<UGamePlatformSFXWorldSubsystem> KeepService(this);

    FGamePlatformSFXResult Output;
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown)
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::InvalidWorld;
        return Output;
    }

    const int32 RequestGeneration = Generation;
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const auto IsRequestScopeCurrent = [this, World, RequestGeneration]()
    {
        return !bClosing && IsValid(World) && !World->bIsTearingDown && GetWorld() == World && Generation == RequestGeneration;
    };
    PruneTerminalOccurrences();
    if (Request.RequestId.IsValid())
    {
        if (Request.PredictionState == EGamePlatformSFXPredictionState::Cancelled)
        {
            StopByRequestId(Request.RequestId);
            Output.Code=EGamePlatformSFXResultCode::Cancelled;
            return Output;
        }
        if (const auto* Terminal=TerminalOccurrences.Find(Request.RequestId))
        {
            if (Terminal->bCancelled || Request.PredictionState != EGamePlatformSFXPredictionState::Corrected)
            {
                Output.Code=Terminal->bCancelled ? EGamePlatformSFXResultCode::Cancelled : Terminal->CompletionCode;
                return Output;
            }
            TerminalOccurrences.Remove(Request.RequestId);
        }
        if (Request.PredictionState == EGamePlatformSFXPredictionState::Corrected)
        {
            if (const auto* Existing=RequestHandles.Find(Request.RequestId)) { const auto Handle=*Existing; Stop(Handle,0.0f); }
            // 停旧音效会发送公开终态；监听关闭后不能申请替换租约或补回旧请求历史。
            if (!IsRequestScopeCurrent())
            { ++Diagnostics.RejectedRequests; Output.Code = EGamePlatformSFXResultCode::InvalidWorld; return Output; }
            TerminalOccurrences.Remove(Request.RequestId);
        }
    }

    const FGamePlatformResult Validation = FGamePlatformSFXPolicy::ValidateRequest(Request);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::InvalidRequest;
        UE_LOG(LogGamePlatformSFX, Verbose, TEXT("拒绝SFX请求：%s"), *Validation.Message);
        return Output;
    }

    if (Request.RequestId.IsValid())
    {
        if (const FGamePlatformSFXHandle* Existing = RequestHandles.Find(Request.RequestId))
        {
            if (IsHandleCurrent(*Existing) &&
                (PendingPlays.Contains(Existing->Id) || ActiveInstances.Contains(Existing->Id)))
            {
                Output.Handle = *Existing;
                Output.Code = PendingPlays.Contains(Existing->Id)
                    ? EGamePlatformSFXResultCode::Queued
                    : EGamePlatformSFXResultCode::Played;
                return Output;
            }
            RequestHandles.Remove(Request.RequestId);
        }
    }

    if (!FGamePlatformSFXBudgetPolicy::CanReserve(PendingPlays.Num(), ActiveInstances.Num()))
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::UnsupportedEnvironment;
        UE_LOG(LogGamePlatformSFX, Warning, TEXT("SFX本地追踪上限已达到，拒绝继续创建实例。"));
        return Output;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    if (!Data)
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::DataUnavailable;
        return Output;
    }

    FPrimaryAssetId DefinitionAssetId;
    const FGamePlatformResult IdResult =
        FGamePlatformSFXPolicy::BuildDefinitionAssetId(Request.DefinitionId, DefinitionAssetId);
    if (!IdResult.IsSuccess())
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::InvalidRequest;
        return Output;
    }

    FGamePlatformSFXHandle Handle;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = Generation;
    Handle.World = World;

    FPendingPlay Pending;
    Pending.Handle = Handle;
    Pending.Request = Request;
    PendingPlays.Add(Handle.Id, Pending);
    if (Request.RequestId.IsValid())
    {
        RequestHandles.Add(Request.RequestId, Handle);
    }

    FGamePlatformResult Accepted;
    const TWeakObjectPtr<UGamePlatformSFXWorldSubsystem> WeakThis(this);
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        DefinitionAssetId,
        UGamePlatformSFXDefinition::StaticClass(),
        { SFXRuntimeBundle },
        EGamePlatformDataLifetime::World,
        this,
        [WeakThis, Handle](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        {
            if (UGamePlatformSFXWorldSubsystem* Self = WeakThis.Get())
            {
                Self->HandleDefinitionLoaded(Handle, CompletedLease, Result);
            }
        },
        Accepted);

    FPendingPlay* StoredPending = PendingPlays.Find(Handle.Id);
    if (!Accepted.IsSuccess() || !Lease.IsValid() || !StoredPending)
    {
        PendingPlays.Remove(Handle.Id);
        RemoveRequestMapping(Request.RequestId, Handle.Id);
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::LoadRejected;
        return Output;
    }

    StoredPending->Lease = Lease;
    FGamePlatformSFXPlaybackSnapshot Snapshot; Snapshot.Handle = Handle;
    Snapshot.State = EGamePlatformSFXPlaybackState::Loading; Snapshot.Code = EGamePlatformSFXResultCode::Queued;
    Snapshot.DefinitionId = Request.DefinitionId; Snapshot.RequestId = Request.RequestId; PlaybackSnapshots.Add(Handle.Id, Snapshot);
    Output.Handle = Handle;
    Output.Code = EGamePlatformSFXResultCode::Queued;
    return Output;
}

bool UGamePlatformSFXWorldSubsystem::Stop(
    const FGamePlatformSFXHandle& Handle,
    const float FadeOutSeconds)
{
    check(IsInGameThread());
    if (!IsHandleCurrent(Handle))
    {
        return false;
    }

    if (const auto* Pending=PendingPlays.Find(Handle.Id)) RecordTerminalOccurrence(Pending->Request.RequestId,true);
    if (const auto* Active=ActiveInstances.Find(Handle.Id)) RecordTerminalOccurrence(Active->RequestId,true);
    if (FPendingPlay* Pending = PendingPlays.Find(Handle.Id))
    {
        const FGuid RequestId = Pending->Request.RequestId;
        const FGamePlatformDataLease Lease = Pending->Lease;
        PendingPlays.Remove(Handle.Id);
        RemoveRequestMapping(RequestId, Handle.Id);
        ReleaseLease(Lease);
        ++Diagnostics.StoppedInstances;
        CompletePlayback(Handle, EGamePlatformSFXPlaybackState::Cancelled, EGamePlatformSFXResultCode::Cancelled);
        return true;
    }

    FActiveInstance* Active = ActiveInstances.Find(Handle.Id);
    if (!Active)
    {
        return false;
    }
    if (Active->bStopRequested)
    {
        return true;
    }

    Active->bStopRequested = true;
    const float EffectiveFadeOut = FadeOutSeconds >= 0.0f
        ? FMath::Clamp(FadeOutSeconds, 0.0f, 10.0f)
        : Active->DefaultFadeOutSeconds;

    UAudioComponent* Component = Active->Component.Get();
    if (!IsValid(Component))
    {
        CleanupActive(Handle.Id, false);
        return true;
    }

    ++Diagnostics.StoppedInstances;
    if (EffectiveFadeOut > 0.0f && Component->IsPlaying())
    {
        Component->FadeOut(EffectiveFadeOut, 0.0f);
    }
    else
    {
        // Stop在已非活动组件上不会承诺完成事件；显式回收避免历史失败实例占槽。
        CleanupActive(Handle.Id, true);
    }
    return true;
}

bool UGamePlatformSFXWorldSubsystem::StopByRequestId(
    const FGuid& RequestId,
    const float FadeOutSeconds)
{
    check(IsInGameThread());
    if (!RequestId.IsValid())
    {
        return false;
    }

    RecordTerminalOccurrence(RequestId, true);
    const FGamePlatformSFXHandle* Found = RequestHandles.Find(RequestId);
    if (!Found)
    {
        return true; // 取消已被接纳，墓碑阻断迟到的首次播放。
    }
    const FGamePlatformSFXHandle Handle = *Found;
    return Stop(Handle, FadeOutSeconds);
}

bool UGamePlatformSFXWorldSubsystem::IsActive(const FGamePlatformSFXHandle& Handle) const
{
    check(IsInGameThread());
    return IsHandleCurrent(Handle) &&
           (PendingPlays.Contains(Handle.Id) || ActiveInstances.Contains(Handle.Id));
}

bool UGamePlatformSFXWorldSubsystem::SetFloatParameter(
    const FGamePlatformSFXHandle& Handle,
    const FName Name,
    const float Value)
{
    check(IsInGameThread());
    if (!IsHandleCurrent(Handle) || Name.IsNone() || !FMath::IsFinite(Value))
    {
        return false;
    }

    FActiveInstance* Active = ActiveInstances.Find(Handle.Id);
    UAudioComponent* Component = Active ? Active->Component.Get() : nullptr;
    if (!Active || !Active->AllowedFloatParameters.Contains(Name) || !IsValid(Component))
    {
        return false;
    }

    Component->SetFloatParameter(Name, Value);
    return true;
}

bool UGamePlatformSFXWorldSubsystem::SetVolumeMultiplier(
    const FGamePlatformSFXHandle& Handle,
    const float Value)
{
    check(IsInGameThread());
    if (!IsHandleCurrent(Handle) || !FMath::IsFinite(Value) || Value < 0.0f || Value > 4.0f)
    {
        return false;
    }

    FActiveInstance* Active = ActiveInstances.Find(Handle.Id);
    UAudioComponent* Component = Active ? Active->Component.Get() : nullptr;
    if (!IsValid(Component))
    {
        return false;
    }

    Component->SetVolumeMultiplier(Value);
    return true;
}

FGamePlatformSFXDiagnostics UGamePlatformSFXWorldSubsystem::GetDiagnostics() const
{
    check(IsInGameThread());
    FGamePlatformSFXDiagnostics Result = Diagnostics;
    Result.ActiveInstances = ActiveInstances.Num();
    Result.PendingLoads = PendingPlays.Num();
    return Result;
}

bool UGamePlatformSFXWorldSubsystem::IsHandleCurrent(
    const FGamePlatformSFXHandle& Handle) const
{
    return Handle.IsValid() &&
           Handle.Generation == Generation &&
           Handle.BelongsToWorld(GetWorld());
}

void UGamePlatformSFXWorldSubsystem::HandleDefinitionLoaded(
    const FGamePlatformSFXHandle Handle,
    const FGamePlatformDataLease& Lease,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bClosing || !IsHandleCurrent(Handle))
    {
        return;
    }

    FPendingPlay* Pending = PendingPlays.Find(Handle.Id);
    if (!Pending || Pending->Lease.LeaseId != Lease.LeaseId || Pending->Lease.Generation != Lease.Generation ||
        Pending->Lease.ScopeId != Lease.ScopeId || Pending->Lease.IssuerProof != Lease.IssuerProof || Pending->Lease.DefinitionId != Lease.DefinitionId)
    {
        return;
    }

    if (!Result.IsSuccess())
    {
        ++Diagnostics.LoadFailures;
        FailPending(Handle.Id, EGamePlatformSFXResultCode::LoadFailed);
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    const UGamePlatformSFXDefinition* Definition = Data
        ? Cast<UGamePlatformSFXDefinition>(Data->GetLoadedDefinition(Lease))
        : nullptr;
    if (!IsValid(Definition) || !Definition->ValidateDefinition().IsSuccess())
    {
        ++Diagnostics.LoadFailures;
        FailPending(Handle.Id, EGamePlatformSFXResultCode::DefinitionInvalid);
        return;
    }

    USoundBase* Sound = Definition->GetSound().Get();
    USoundAttenuation* Attenuation = Definition->GetAttenuation().IsNull()
        ? nullptr
        : Definition->GetAttenuation().Get();
    USoundConcurrency* Concurrency = Definition->GetConcurrency().IsNull()
        ? nullptr
        : Definition->GetConcurrency().Get();
    if (!IsValid(Sound) ||
        (!Definition->GetAttenuation().IsNull() && !IsValid(Attenuation)) ||
        (!Definition->GetConcurrency().IsNull() && !IsValid(Concurrency)))
    {
        ++Diagnostics.LoadFailures;
        FailPending(Handle.Id, EGamePlatformSFXResultCode::AssetUnavailable);
        return;
    }

    for (const TPair<FName, float>& Pair : Pending->Request.FloatParameters)
    {
        if (!Definition->IsFloatParameterAllowed(Pair.Key))
        {
            ++Diagnostics.RejectedRequests;
            FailPending(Handle.Id, EGamePlatformSFXResultCode::InvalidRequest);
            return;
        }
    }

    const FGamePlatformSFXRequest Request = Pending->Request;
    const float TargetVolume = Definition->GetVolumeMultiplier() * Request.VolumeMultiplier;
    const float TargetPitch = Definition->GetPitchMultiplier() * Request.PitchMultiplier;
    const float InitialVolume = Definition->GetFadeInSeconds() > 0.0f ? 0.0f : TargetVolume;

    USceneComponent* AttachTo = nullptr;
    if (Definition->GetPlaybackSpace() == EGamePlatformSFXPlaybackSpace::Attached)
    {
        AttachTo = Request.AttachComponent.Get();
        if (!IsValid(AttachTo) || AttachTo->GetWorld() != World ||
            !IsValid(AttachTo->GetOwner()) || AttachTo->GetOwner()->IsActorBeingDestroyed())
        {
            ++Diagnostics.RejectedRequests;
            FailPending(Handle.Id, EGamePlatformSFXResultCode::OwnerInvalid);
            return;
        }
    }
    FAudioDevice::FCreateComponentParams Params(World, AttachTo ? AttachTo->GetOwner() : nullptr);
    Params.bPlay = false;
    Params.bAutoDestroy = false;
    Params.bStopWhenOwnerDestroyed = AttachTo && Definition->ShouldStopWhenOwnerDestroyed();
    Params.AttenuationSettings = Attenuation;
    if (Concurrency) Params.ConcurrencySet.Add(Concurrency);
    if (Definition->GetPlaybackSpace() != EGamePlatformSFXPlaybackSpace::TwoD)
        Params.SetLocation(AttachTo ? AttachTo->GetComponentLocation() : Request.Location);
    UAudioComponent* Component = FAudioDevice::CreateComponent(Sound, Params);
    if (!IsValid(Component))
    {
        ++Diagnostics.SpawnFailures;
        FailPending(Handle.Id, EGamePlatformSFXResultCode::SpawnFailed);
        return;
    }

    Component->SetVolumeMultiplier(InitialVolume);
    Component->SetPitchMultiplier(TargetPitch);
    Component->bAllowSpatialization = Definition->GetPlaybackSpace() != EGamePlatformSFXPlaybackSpace::TwoD && Params.ShouldUseAttenuation();
    if (AttachTo)
    {
        Component->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.AttachPointName);
        Component->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    }
    else if (Definition->GetPlaybackSpace() == EGamePlatformSFXPlaybackSpace::World)
    {
        Component->SetWorldLocationAndRotation(Request.Location, Request.Rotation);
    }
    Component->SetUISound(Definition->GetPlaybackSpace() == EGamePlatformSFXPlaybackSpace::TwoD);
    for (const TPair<FName, float>& Pair : Definition->GetDefaultFloatParameters())
    {
        Component->SetFloatParameter(Pair.Key, Pair.Value);
    }
    for (const TPair<FName, float>& Pair : Request.FloatParameters)
    {
        Component->SetFloatParameter(Pair.Key, Pair.Value);
    }

    FActiveInstance Active;
    Active.Handle = Handle;
    Active.RequestId = Request.RequestId;
    Active.Component = Component;
    Active.Lease = Lease;
    Active.AllowedFloatParameters = Definition->GetAllowedFloatParameters();
    Active.DefaultFadeOutSeconds = Definition->GetFadeOutSeconds();
    ActiveInstances.Add(Handle.Id, MoveTemp(Active));
    PendingPlays.Remove(Handle.Id);

    const TStrongObjectPtr<UAudioComponent> KeepComponent(Component);
    const TStrongObjectPtr<UGamePlatformSFXWorldSubsystem> KeepService(this);
    Component->OnAudioFinishedNative.AddUObject(
        this,
        &UGamePlatformSFXWorldSubsystem::HandleAudioFinished);
    // UE失败启动不发AudioFinished，但仍广播Stopped；先绑定再播放能覆盖异步Concurrency拒绝。
    Component->OnAudioPlayStateChangedNative.AddUObject(this,
        &UGamePlatformSFXWorldSubsystem::HandleAudioPlayStateChanged);
    if (Definition->GetFadeInSeconds() > 0.0f)
        Component->FadeIn(Definition->GetFadeInSeconds(), TargetVolume, Request.StartTimeSeconds);
    else
        Component->Play(Request.StartTimeSeconds);
    if (!ActiveInstances.Contains(Handle.Id)) return;
    if (!Component->IsPlaying())
    {
        ++Diagnostics.SpawnFailures;
        CleanupActive(Handle.Id, true, EGamePlatformSFXPlaybackState::Failed, EGamePlatformSFXResultCode::SpawnFailed);
        return;
    }

    if (auto* Snapshot = PlaybackSnapshots.Find(Handle.Id))
    { Snapshot->State = EGamePlatformSFXPlaybackState::Playing; Snapshot->Code = EGamePlatformSFXResultCode::Played; }
    ++Diagnostics.PlayedInstances;
    Diagnostics.PeakActiveInstances = FMath::Max(
        Diagnostics.PeakActiveInstances,
        ActiveInstances.Num());
}

void UGamePlatformSFXWorldSubsystem::HandleAudioFinished(UAudioComponent* Component)
{
    check(IsInGameThread());
    if (!Component)
    {
        return;
    }

    FGuid FoundId;
    for (const TPair<FGuid, FActiveInstance>& Pair : ActiveInstances)
    {
        if (Pair.Value.Component.Get() == Component)
        {
            FoundId = Pair.Key;
            break;
        }
    }

    if (FoundId.IsValid())
    {
        CleanupActive(FoundId, false);
    }
}

void UGamePlatformSFXWorldSubsystem::HandleAudioPlayStateChanged(
    const UAudioComponent* Component, const EAudioComponentPlayState PlayState)
{
    if (PlayState != EAudioComponentPlayState::Stopped) return;
    // 记录先移出再销毁组件，解绑回调保证Stop/Finished/FailedToStart交错仅释放一次。
    FGuid HandleId;
    for (const auto& Pair : ActiveInstances)
        if (Pair.Value.Component.Get() == Component) { HandleId = Pair.Key; break; }
    if (ActiveInstances.Contains(HandleId))
        CleanupActive(HandleId, false, EGamePlatformSFXPlaybackState::Failed, EGamePlatformSFXResultCode::SpawnFailed);
}

void UGamePlatformSFXWorldSubsystem::FailPending(
    const FGuid HandleId,
    const EGamePlatformSFXResultCode Code)
{
    FPendingPlay Pending;
    if (!PendingPlays.RemoveAndCopyValue(HandleId, Pending))
    {
        return;
    }
    RemoveRequestMapping(Pending.Request.RequestId, HandleId);
    ReleaseLease(Pending.Lease);
    RecordTerminalOccurrence(Pending.Request.RequestId, false);
    if (auto* Terminal = TerminalOccurrences.Find(Pending.Request.RequestId)) if (!Terminal->bCancelled) Terminal->CompletionCode = Code;
    CompletePlayback(Pending.Handle, bClosing ? EGamePlatformSFXPlaybackState::WorldDestroyed : EGamePlatformSFXPlaybackState::Failed, Code);

    UE_LOG(
        LogGamePlatformSFX,
        Verbose,
        TEXT("SFX异步播放未完成，ResultCode=%d Definition=%s"),
        static_cast<int32>(Code),
        *Pending.Request.DefinitionId.ToString());
}

void UGamePlatformSFXWorldSubsystem::CleanupActive(
    const FGuid HandleId,
    const bool bStopComponent, EGamePlatformSFXPlaybackState State, EGamePlatformSFXResultCode Code)
{
    FActiveInstance Active;
    if (!ActiveInstances.RemoveAndCopyValue(HandleId, Active))
    {
        return;
    }

    RecordTerminalOccurrence(Active.RequestId, false);
    RemoveRequestMapping(Active.RequestId, HandleId);
    UAudioComponent* Component = Active.Component.Get();
    if (IsValid(Component))
    {
        Component->OnAudioFinishedNative.RemoveAll(this);
        Component->OnAudioPlayStateChangedNative.RemoveAll(this);
        if (bStopComponent && Component->IsPlaying())
        {
            Component->Stop();
        }
        Component->DestroyComponent();
    }
    ReleaseLease(Active.Lease);
    if (bClosing) { State = EGamePlatformSFXPlaybackState::WorldDestroyed; Code = EGamePlatformSFXResultCode::InvalidWorld; }
    else if (Active.bStopRequested) { State = EGamePlatformSFXPlaybackState::Cancelled; Code = EGamePlatformSFXResultCode::Cancelled; }
    if (auto* Terminal = TerminalOccurrences.Find(Active.RequestId)) if (!Terminal->bCancelled) Terminal->CompletionCode = Code;
    CompletePlayback(Active.Handle, State, Code);
}

FGamePlatformSFXPlaybackSnapshot UGamePlatformSFXWorldSubsystem::GetPlaybackSnapshot(const FGamePlatformSFXHandle& Handle) const
{
    check(IsInGameThread());
    if (const auto* Snapshot = PlaybackSnapshots.Find(Handle.Id))
        if (Snapshot->Handle.Generation == Handle.Generation && Snapshot->Handle.World == Handle.World) return *Snapshot;
    return {};
}
FDelegateHandle UGamePlatformSFXWorldSubsystem::AddCompletionHandler(const FGamePlatformSFXPlaybackCompleted::FDelegate& Handler)
{
    check(IsInGameThread()); return Handler.IsBound() ? PlaybackCompleted.Add(Handler) : FDelegateHandle();
}
void UGamePlatformSFXWorldSubsystem::RemoveCompletionHandler(FDelegateHandle Handle)
{
    check(IsInGameThread()); PlaybackCompleted.Remove(Handle);
}
void UGamePlatformSFXWorldSubsystem::CompletePlayback(const FGamePlatformSFXHandle& Handle,
    EGamePlatformSFXPlaybackState State, EGamePlatformSFXResultCode Code)
{
    const TStrongObjectPtr<UGamePlatformSFXWorldSubsystem> KeepService(this);
    auto* Stored = PlaybackSnapshots.Find(Handle.Id);
    if (Stored && Stored->State != EGamePlatformSFXPlaybackState::Loading && Stored->State != EGamePlatformSFXPlaybackState::Playing) return;
    auto Snapshot = Stored ? *Stored : FGamePlatformSFXPlaybackSnapshot();
    Snapshot.Handle = Handle; Snapshot.State = State; Snapshot.Code = Code;
    if (CompletedPlaybackOrder.Num() >= 512) { PlaybackSnapshots.Remove(CompletedPlaybackOrder[0]); CompletedPlaybackOrder.RemoveAt(0); }
    CompletedPlaybackOrder.Add(Handle.Id); PlaybackSnapshots.Add(Handle.Id, Snapshot);
    PlaybackCompleted.Broadcast(Snapshot); // 账本/组件/租约已经清理，观察者可同步停止/退出。
}

void UGamePlatformSFXWorldSubsystem::ReleaseLease(
    const FGamePlatformDataLease& Lease) const
{
    if (!Lease.IsValid())
    {
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr)
    {
        Data->ReleaseDefinition(Lease);
    }
}

void UGamePlatformSFXWorldSubsystem::RemoveRequestMapping(
    const FGuid& RequestId,
    const FGuid& HandleId)
{
    if (!RequestId.IsValid())
    {
        return;
    }

    const FGamePlatformSFXHandle* Existing = RequestHandles.Find(RequestId);
    if (Existing && Existing->Id == HandleId)
    {
        RequestHandles.Remove(RequestId);
    }
}

// 历史表只保证30秒/512项范围内的去重；达到容量淘汰最早期限，不作持久化或跨世界记录。
void UGamePlatformSFXWorldSubsystem::PruneTerminalOccurrences()
{
    const double Now=FPlatformTime::Seconds();
    for (auto It=TerminalOccurrences.CreateIterator(); It; ++It)
        if (It.Value().ExpiresAtSeconds <= Now) It.RemoveCurrent();
}
void UGamePlatformSFXWorldSubsystem::RecordTerminalOccurrence(const FGuid& RequestId, const bool bCancelled)
{
    if (!RequestId.IsValid()) return;
    PruneTerminalOccurrences();
    if (auto* Existing=TerminalOccurrences.Find(RequestId)) { Existing->bCancelled |= bCancelled; return; }
    if (TerminalOccurrences.Num() >= MaxTerminalSFXRequests)
    {
        FGuid Oldest; double Time=TNumericLimits<double>::Max();
        for (const auto& Pair:TerminalOccurrences)
            if (Pair.Value.ExpiresAtSeconds < Time) { Time=Pair.Value.ExpiresAtSeconds; Oldest=Pair.Key; }
        TerminalOccurrences.Remove(Oldest);
    }
    FTerminalOccurrence Entry; Entry.ExpiresAtSeconds=FPlatformTime::Seconds()+TerminalSFXRetentionSeconds; Entry.bCancelled=bCancelled;
    TerminalOccurrences.Add(RequestId, Entry);
}
