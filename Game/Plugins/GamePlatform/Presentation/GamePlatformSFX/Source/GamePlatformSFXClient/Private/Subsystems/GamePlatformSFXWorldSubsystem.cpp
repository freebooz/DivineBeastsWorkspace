#include "Subsystems/GamePlatformSFXWorldSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Definitions/GamePlatformSFXDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Policy/GamePlatformSFXPolicy.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSFX, Log, All);

namespace
{
constexpr FName SFXRuntimeBundle(TEXT("SFXRuntime"));
constexpr int32 MaxPendingSFXLoads = 128;
constexpr int32 MaxTrackedSFXInstances = 256;

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
    bClosing = true;
    ++Generation;

    TArray<FGuid> ActiveIds;
    ActiveInstances.GetKeys(ActiveIds);
    for (const FGuid& Id : ActiveIds)
    {
        CleanupActive(Id, true);
    }

    for (const TPair<FGuid, FPendingPlay>& Pair : PendingPlays)
    {
        ReleaseLease(Pair.Value.Lease);
    }
    PendingPlays.Reset();
    RequestHandles.Reset();

    Super::Deinitialize();
}

FGamePlatformSFXResult UGamePlatformSFXWorldSubsystem::Play(
    const FGamePlatformSFXRequest& Request)
{
    check(IsInGameThread());
    ++Diagnostics.PlayRequests;

    FGamePlatformSFXResult Output;
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown)
    {
        ++Diagnostics.RejectedRequests;
        Output.Code = EGamePlatformSFXResultCode::InvalidWorld;
        return Output;
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

    if (PendingPlays.Num() >= MaxPendingSFXLoads ||
        ActiveInstances.Num() >= MaxTrackedSFXInstances)
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

    if (FPendingPlay* Pending = PendingPlays.Find(Handle.Id))
    {
        const FGuid RequestId = Pending->Request.RequestId;
        const FGamePlatformDataLease Lease = Pending->Lease;
        PendingPlays.Remove(Handle.Id);
        RemoveRequestMapping(RequestId, Handle.Id);
        ReleaseLease(Lease);
        ++Diagnostics.StoppedInstances;
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
        Component->Stop();
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

    const FGamePlatformSFXHandle* Found = RequestHandles.Find(RequestId);
    if (!Found)
    {
        return false;
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
    if (!Pending || Pending->Lease.LeaseId != Lease.LeaseId)
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

    UAudioComponent* Component = nullptr;
    switch (Definition->GetPlaybackSpace())
    {
    case EGamePlatformSFXPlaybackSpace::TwoD:
        Component = UGameplayStatics::SpawnSound2D(
            World,
            Sound,
            InitialVolume,
            TargetPitch,
            Request.StartTimeSeconds,
            Concurrency,
            false,
            false);
        break;

    case EGamePlatformSFXPlaybackSpace::World:
        Component = UGameplayStatics::SpawnSoundAtLocation(
            World,
            Sound,
            Request.Location,
            Request.Rotation,
            InitialVolume,
            TargetPitch,
            Request.StartTimeSeconds,
            Attenuation,
            Concurrency,
            false);
        break;

    case EGamePlatformSFXPlaybackSpace::Attached:
    {
        USceneComponent* AttachTo = Request.AttachComponent.Get();
        if (!IsValid(AttachTo) || AttachTo->GetWorld() != World)
        {
            ++Diagnostics.RejectedRequests;
            FailPending(Handle.Id, EGamePlatformSFXResultCode::OwnerInvalid);
            return;
        }
        Component = UGameplayStatics::SpawnSoundAttached(
            Sound,
            AttachTo,
            Request.AttachPointName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            Definition->ShouldStopWhenOwnerDestroyed(),
            InitialVolume,
            TargetPitch,
            Request.StartTimeSeconds,
            Attenuation,
            Concurrency,
            false);
        break;
    }

    default:
        break;
    }

    if (!IsValid(Component))
    {
        ++Diagnostics.SpawnFailures;
        FailPending(Handle.Id, EGamePlatformSFXResultCode::SpawnFailed);
        return;
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

    Component->OnAudioFinishedNative.AddUObject(
        this,
        &UGamePlatformSFXWorldSubsystem::HandleAudioFinished);
    if (Definition->GetFadeInSeconds() > 0.0f)
    {
        Component->AdjustVolume(Definition->GetFadeInSeconds(), TargetVolume);
    }

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

    UE_LOG(
        LogGamePlatformSFX,
        Verbose,
        TEXT("SFX异步播放未完成，ResultCode=%d Definition=%s"),
        static_cast<int32>(Code),
        *Pending.Request.DefinitionId.ToString());
}

void UGamePlatformSFXWorldSubsystem::CleanupActive(
    const FGuid HandleId,
    const bool bStopComponent)
{
    FActiveInstance Active;
    if (!ActiveInstances.RemoveAndCopyValue(HandleId, Active))
    {
        return;
    }

    RemoveRequestMapping(Active.RequestId, HandleId);
    UAudioComponent* Component = Active.Component.Get();
    if (IsValid(Component))
    {
        Component->OnAudioFinishedNative.RemoveAll(this);
        if (bStopComponent && Component->IsPlaying())
        {
            Component->Stop();
        }
        Component->DestroyComponent();
    }
    ReleaseLease(Active.Lease);
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
