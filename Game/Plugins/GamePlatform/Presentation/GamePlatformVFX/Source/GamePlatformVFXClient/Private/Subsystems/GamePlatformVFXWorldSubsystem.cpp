#include "Subsystems/GamePlatformVFXWorldSubsystem.h"

#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "NiagaraComponent.h"
#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Resolution/GamePlatformVFXResolver.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "Types/GamePlatformId.h"

namespace
{
const FName VFXRuntimeBundle(TEXT("VFXRuntime"));

bool IsSupportedWorldType(const EWorldType::Type WorldType)
{
    return WorldType == EWorldType::Game ||
           WorldType == EWorldType::PIE ||
           WorldType == EWorldType::GamePreview;
}

bool BuildDefinitionAssetId(const FName DefinitionId, FPrimaryAssetId& OutAssetId)
{
    OutAssetId = FPrimaryAssetId();

    FGamePlatformId LogicalId;
    if (DefinitionId.IsNone() ||
        !FGamePlatformId::TryParse(DefinitionId.ToString(), LogicalId))
    {
        return false;
    }

    const FString Canonical = LogicalId.ToString();
    if (Canonical.IsEmpty())
    {
        return false;
    }

    OutAssetId = FPrimaryAssetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(),
        FName(*Canonical));
    return true;
}
}

bool UGamePlatformVFXWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) &&
           IsSupportedWorldType(World->WorldType) &&
           World->GetNetMode() != NM_DedicatedServer &&
           !IsRunningCommandlet() &&
           Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformVFXWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    PendingInstanceCount = 0;
    PeakTrackedInstances = 0;
    DefinitionCacheSerial = 0;
    UpdateRuntimeDiagnostics();

    // StartupCatalogs仅保留给旧低层工具兼容；正常Gameplay由GamePlatformPresentation完成唯一语义解析。
    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    TArray<FSoftObjectPath> StartupCatalogPaths;
    StartupCatalogPaths.Reserve(FMath::Min(Settings->StartupCatalogs.Num(), Settings->MaxStartupCatalogs));
    for (const TSoftObjectPtr<UGamePlatformVFXCatalog>& CatalogRef : Settings->StartupCatalogs)
    {
        if (StartupCatalogPaths.Num() >= Settings->MaxStartupCatalogs)
        {
            break;
        }
        if (const FSoftObjectPath Path = CatalogRef.ToSoftObjectPath(); Path.IsValid())
        {
            StartupCatalogPaths.AddUnique(Path);
        }
    }

    if (!StartupCatalogPaths.IsEmpty())
    {
        StartupCatalogLoadLease = FGamePlatformAssetLoader::RequestAsyncLoad(
            StartupCatalogPaths,
            FStreamableDelegate::CreateUObject(
                this,
                &UGamePlatformVFXWorldSubsystem::HandleStartupCatalogsLoaded));
    }
}

void UGamePlatformVFXWorldSubsystem::Deinitialize()
{
    check(IsInGameThread());
    bClosing = true;

    FGamePlatformAssetLoader::Cancel(StartupCatalogLoadLease);
    StartupCatalogLoadLease.Reset();

    if (UWorld* World = GetWorld())
    {
        for (TPair<FGuid, FTimerHandle>& Pair : LifetimeTimers)
        {
            World->GetTimerManager().ClearTimer(Pair.Value);
        }
        for (TPair<FGuid, TArray<FTimerHandle>>& Pair : CompositeStepTimers)
        {
            for (FTimerHandle& Timer : Pair.Value)
            {
                World->GetTimerManager().ClearTimer(Timer);
            }
        }
    }

    LifetimeTimers.Reset();
    CompositeStepTimers.Reset();

    // 先停实例，再一次性释放World共享Definition租约，避免每个历史实例产生独立Data释放记录。
    InstanceRegistry.Reset();
    DefinitionIdByHandle.Reset();
    PreloadDefinitionIds.Reset();
    DedupeHandles.Reset();
    DedupeKeysByHandle.Reset();
    PendingInstanceCount = 0;

    ReleaseAllCachedDefinitions();

    CatalogRegistry.Reset();
    StartupCatalogHandles.Reset();
    RegisteredCatalogObjects.Reset();

    UpdateRuntimeDiagnostics();
    Super::Deinitialize();
}

void UGamePlatformVFXWorldSubsystem::HandleStartupCatalogsLoaded()
{
    check(IsInGameThread());
    if (bClosing)
    {
        return;
    }

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    for (const TSoftObjectPtr<UGamePlatformVFXCatalog>& CatalogRef : Settings->StartupCatalogs)
    {
        if (UGamePlatformVFXCatalog* Catalog = CatalogRef.Get())
        {
            const FGamePlatformVFXRegistrationHandle Handle = RegisterCatalog(Catalog);
            if (Handle.IsValid())
            {
                StartupCatalogHandles.Add(Handle);
            }
        }
    }
}

FGamePlatformVFXDedupeKey UGamePlatformVFXWorldSubsystem::MakeDedupeKey(
    const FGamePlatformVFXRequest& Request) const
{
    // Presentation RequestId在预测/确认/纠正之间保持同一身份，优先用于幂等与纠正。
    FGamePlatformVFXDedupeKey Key;
    if (Request.RequestId.IsValid())
    {
        Key.Kind = EGamePlatformVFXDedupeKind::Request;
        Key.Id = Request.RequestId;
        return Key;
    }
    if (Request.ActivationId.IsValid())
    {
        Key.Kind = EGamePlatformVFXDedupeKind::Activation;
        Key.Id = Request.ActivationId;
        Key.PredictionKey = Request.PredictionKey;
    }
    return Key;
}

FName UGamePlatformVFXWorldSubsystem::ResolveDefinitionId(
    const FGamePlatformVFXRequest& Request,
    bool& bOutAmbiguous) const
{
    bOutAmbiguous = false;
    if (!Request.DefinitionId.IsNone())
    {
        return Request.DefinitionId;
    }

    // 兼容旧低层工具：没有Presentation解析结果时才允许走旧VFX Catalog。
    const FGamePlatformVFXResolvedDefinition Resolved =
        FGamePlatformVFXResolver(CatalogRegistry).Resolve(Request);
    bOutAmbiguous = Resolved.bAmbiguous;
    return Resolved.IsValid() ? Resolved.DefinitionId : NAME_None;
}

EGamePlatformVFXDefinitionQueueResult
UGamePlatformVFXWorldSubsystem::QueueDefinitionLoad(
    const FName DefinitionId,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ReservedHandle)
{
    check(IsInGameThread());

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    if (FGamePlatformVFXCachedDefinitionEntry* Existing = DefinitionCache.Find(DefinitionId))
    {
        TouchCachedDefinition(DefinitionId);
        FGamePlatformVFXDiagnostics::DefinitionCacheHit();

        if (Existing->bLoading)
        {
            if (PendingInstanceCount >= Settings->MaxPendingInstancePreloads)
            {
                return EGamePlatformVFXDefinitionQueueResult::Failed;
            }

            FGamePlatformVFXPendingDefinitionRequest Pending;
            Pending.Handle = ReservedHandle;
            Pending.Request = Request;
            Existing->PendingRequests.Add(ReservedHandle.Id, MoveTemp(Pending));
            DefinitionIdByHandle.Add(ReservedHandle.Id, DefinitionId);
            ++PendingInstanceCount;
            UpdateRuntimeDiagnostics();
            return EGamePlatformVFXDefinitionQueueResult::Queued;
        }

        if (UGamePlatformVFXDefinition* Definition = Existing->Definition.Get())
        {
            DefinitionIdByHandle.Add(ReservedHandle.Id, DefinitionId);
            ++Existing->ActiveUsers;
            if (ExecuteLoadedDefinition(*Definition, Request, ReservedHandle))
            {
                UpdateRuntimeDiagnostics();
                return EGamePlatformVFXDefinitionQueueResult::Executed;
            }
            return EGamePlatformVFXDefinitionQueueResult::Failed;
        }

        // Lease仍在但对象不可读属于异常状态；仅在没有活跃使用者时清掉并重新加载。
        if (Existing->ActiveUsers > 0 ||
            !Existing->PendingRequests.IsEmpty() ||
            !Existing->PreloadHandles.IsEmpty())
        {
            return EGamePlatformVFXDefinitionQueueResult::Failed;
        }

        const FGamePlatformDataLease StaleLease = Existing->Lease;
        DefinitionCache.Remove(DefinitionId);
        ReleaseLease(StaleLease);
    }

    if (PendingInstanceCount >= Settings->MaxPendingInstancePreloads ||
        !EnsureDefinitionCacheCapacity())
    {
        return EGamePlatformVFXDefinitionQueueResult::Failed;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    if (!Data)
    {
        return EGamePlatformVFXDefinitionQueueResult::Failed;
    }

    FPrimaryAssetId DefinitionAssetId;
    if (!BuildDefinitionAssetId(DefinitionId, DefinitionAssetId))
    {
        return EGamePlatformVFXDefinitionQueueResult::Failed;
    }

    FGamePlatformVFXCachedDefinitionEntry NewEntry;
    NewEntry.bLoading = true;
    NewEntry.LastUsedSerial = ++DefinitionCacheSerial;

    FGamePlatformVFXPendingDefinitionRequest Pending;
    Pending.Handle = ReservedHandle;
    Pending.Request = Request;
    NewEntry.PendingRequests.Add(ReservedHandle.Id, MoveTemp(Pending));

    DefinitionCache.Add(DefinitionId, MoveTemp(NewEntry));
    DefinitionIdByHandle.Add(ReservedHandle.Id, DefinitionId);
    ++PendingInstanceCount;
    FGamePlatformVFXDiagnostics::DefinitionCacheMiss();
    UpdateRuntimeDiagnostics();

    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        DefinitionAssetId,
        UGamePlatformVFXDefinition::StaticClass(),
        { VFXRuntimeBundle },
        EGamePlatformDataLifetime::World,
        this,
        [WeakThis, DefinitionId](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& Result)
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                Self->HandleCachedDefinitionLoaded(
                    DefinitionId,
                    CompletedLease,
                    Result);
            }
        },
        Accepted);

    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        DefinitionCache.Remove(DefinitionId);
        DefinitionIdByHandle.Remove(ReservedHandle.Id);
        PendingInstanceCount = FMath::Max(0, PendingInstanceCount - 1);
        UpdateRuntimeDiagnostics();
        return EGamePlatformVFXDefinitionQueueResult::Failed;
    }

    if (FGamePlatformVFXCachedDefinitionEntry* Stored = DefinitionCache.Find(DefinitionId))
    {
        Stored->Lease = Lease;
    }
    return EGamePlatformVFXDefinitionQueueResult::Queued;
}

void UGamePlatformVFXWorldSubsystem::HandleCachedDefinitionLoaded(
    const FName DefinitionId,
    const FGamePlatformDataLease Lease,
    const FGamePlatformResult& Result)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_DefinitionLoaded);
    check(IsInGameThread());

    if (bClosing)
    {
        return;
    }

    FGamePlatformVFXCachedDefinitionEntry* Entry = DefinitionCache.Find(DefinitionId);
    if (!Entry ||
        !Entry->Lease.IsValid() ||
        Entry->Lease.LeaseId != Lease.LeaseId)
    {
        return;
    }

    Entry->bLoading = false;

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    UGamePlatformVFXDefinition* Definition =
        Result.IsSuccess() && Data
            ? const_cast<UGamePlatformVFXDefinition*>(
                Cast<UGamePlatformVFXDefinition>(
                    Data->GetLoadedDefinition(Lease)))
            : nullptr;

    const bool bDefinitionValid =
        IsValid(Definition) &&
        Definition->ValidateDefinition().IsSuccess();

    if (!Result.IsSuccess() || !bDefinitionValid)
    {
        FGamePlatformVFXDiagnostics::DefinitionLoadFailed(
            FSoftObjectPath(Lease.DefinitionId.ToString()));

        TMap<FGuid, FGamePlatformVFXPendingDefinitionRequest> FailedPending =
            MoveTemp(Entry->PendingRequests);
        TSet<FGuid> FailedPreloads = MoveTemp(Entry->PreloadHandles);
        const FGamePlatformDataLease FailedLease = Entry->Lease;

        PendingInstanceCount = FMath::Max(
            0,
            PendingInstanceCount - FailedPending.Num());
        DefinitionCache.Remove(DefinitionId);

        for (const FGuid& PreloadId : FailedPreloads)
        {
            PreloadDefinitionIds.Remove(PreloadId);
        }

        ReleaseLease(FailedLease);

        for (const TPair<FGuid, FGamePlatformVFXPendingDefinitionRequest>& Pair : FailedPending)
        {
            CleanupInstance(Pair.Value.Handle, true);
        }

        UpdateRuntimeDiagnostics();
        return;
    }

    Entry->Definition = Definition;
    Entry->LastUsedSerial = ++DefinitionCacheSerial;

    TMap<FGuid, FGamePlatformVFXPendingDefinitionRequest> ReadyPending =
        MoveTemp(Entry->PendingRequests);
    Entry->PendingRequests.Reset();
    PendingInstanceCount = FMath::Max(
        0,
        PendingInstanceCount - ReadyPending.Num());
    UpdateRuntimeDiagnostics();

    for (const TPair<FGuid, FGamePlatformVFXPendingDefinitionRequest>& Pair : ReadyPending)
    {
        const FGamePlatformVFXPendingDefinitionRequest& Pending = Pair.Value;
        if (!InstanceRegistry.IsActive(Pending.Handle))
        {
            DefinitionIdByHandle.Remove(Pending.Handle.Id);
            continue;
        }

        if (FGamePlatformVFXCachedDefinitionEntry* Current = DefinitionCache.Find(DefinitionId))
        {
            ++Current->ActiveUsers;
            Current->LastUsedSerial = ++DefinitionCacheSerial;
        }

        if (!ExecuteLoadedDefinition(
                *Definition,
                Pending.Request,
                Pending.Handle))
        {
            CleanupInstance(Pending.Handle, true);
        }
    }

    UpdateRuntimeDiagnostics();
}

bool UGamePlatformVFXWorldSubsystem::EnsureDefinitionCacheCapacity()
{
    const int32 Limit =
        FMath::Max(1, GetDefault<UGamePlatformVFXSettings>()->MaxCachedDefinitions);
    while (DefinitionCache.Num() >= Limit)
    {
        if (!EvictOneCachedDefinition())
        {
            return false;
        }
    }
    return true;
}

bool UGamePlatformVFXWorldSubsystem::EvictOneCachedDefinition()
{
    FName Candidate = NAME_None;
    uint64 OldestSerial = MAX_uint64;

    for (const TPair<FName, FGamePlatformVFXCachedDefinitionEntry>& Pair : DefinitionCache)
    {
        const FGamePlatformVFXCachedDefinitionEntry& Entry = Pair.Value;
        if (Entry.bLoading ||
            Entry.ActiveUsers > 0 ||
            !Entry.PendingRequests.IsEmpty() ||
            !Entry.PreloadHandles.IsEmpty())
        {
            continue;
        }

        if (Entry.LastUsedSerial < OldestSerial)
        {
            OldestSerial = Entry.LastUsedSerial;
            Candidate = Pair.Key;
        }
    }

    if (Candidate.IsNone())
    {
        return false;
    }

    FGamePlatformVFXCachedDefinitionEntry Removed;
    if (!DefinitionCache.RemoveAndCopyValue(Candidate, Removed))
    {
        return false;
    }

    ReleaseLease(Removed.Lease);
    return true;
}

void UGamePlatformVFXWorldSubsystem::ReleaseAllCachedDefinitions()
{
    for (const TPair<FName, FGamePlatformVFXCachedDefinitionEntry>& Pair : DefinitionCache)
    {
        ReleaseLease(Pair.Value.Lease);
    }
    DefinitionCache.Reset();
}

void UGamePlatformVFXWorldSubsystem::RemoveDefinitionUse(
    const FGamePlatformVFXHandle& Handle)
{
    FName DefinitionId = NAME_None;
    if (!DefinitionIdByHandle.RemoveAndCopyValue(Handle.Id, DefinitionId))
    {
        return;
    }

    FGamePlatformVFXCachedDefinitionEntry* Entry = DefinitionCache.Find(DefinitionId);
    if (!Entry)
    {
        return;
    }

    if (Entry->PendingRequests.Remove(Handle.Id) > 0)
    {
        PendingInstanceCount = FMath::Max(0, PendingInstanceCount - 1);
    }
    else if (Entry->ActiveUsers > 0)
    {
        --Entry->ActiveUsers;
    }

    Entry->LastUsedSerial = ++DefinitionCacheSerial;

    if (Entry->bLoading &&
        Entry->ActiveUsers == 0 &&
        Entry->PendingRequests.IsEmpty() &&
        Entry->PreloadHandles.IsEmpty())
    {
        const FGamePlatformDataLease UnusedLease = Entry->Lease;
        DefinitionCache.Remove(DefinitionId);
        ReleaseLease(UnusedLease);
    }
}

void UGamePlatformVFXWorldSubsystem::TouchCachedDefinition(
    const FName DefinitionId)
{
    if (FGamePlatformVFXCachedDefinitionEntry* Entry = DefinitionCache.Find(DefinitionId))
    {
        Entry->LastUsedSerial = ++DefinitionCacheSerial;
    }
}

FGamePlatformVFXResult UGamePlatformVFXWorldSubsystem::Play(
    const FGamePlatformVFXRequest& Request)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_Play);
    check(IsInGameThread());
    FGamePlatformVFXDiagnostics::PlayRequested();

    FGamePlatformVFXResult Result;
    const auto Reject = [&Result](const EGamePlatformVFXResultCode Code)
    {
        Result.Code = Code;
        FGamePlatformVFXDiagnostics::RequestRejected();
        return Result;
    };

    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown)
    {
        return Reject(EGamePlatformVFXResultCode::InvalidWorld);
    }

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    const FGamePlatformVFXDedupeKey DedupeKey = MakeDedupeKey(Request);

    if (DedupeKey.IsValid())
    {
        if (Request.PredictionState == EGamePlatformVFXPredictionState::Cancelled)
        {
            if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
            {
                const FGamePlatformVFXHandle ExistingHandle = *Existing;
                Result.Handle = ExistingHandle;
                Stop(ExistingHandle);
                FGamePlatformVFXDiagnostics::DedupeHit();
            }
            Result.Code = EGamePlatformVFXResultCode::Cancelled;
            return Result;
        }

        if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
        {
            const FGamePlatformVFXHandle ExistingHandle = *Existing;
            if (Request.PredictionState == EGamePlatformVFXPredictionState::Corrected)
            {
                Stop(ExistingHandle);
                FGamePlatformVFXDiagnostics::DedupeHit();
            }
            else if (InstanceRegistry.IsActive(ExistingHandle))
            {
                Result.Handle = ExistingHandle;
                Result.Code = EGamePlatformVFXResultCode::Played;
                FGamePlatformVFXDiagnostics::DedupeHit();
                return Result;
            }
            else
            {
                RemoveDedupeHandle(ExistingHandle);
            }
        }
    }

    if (!Request.IsStructurallyValid())
    {
        return Reject(EGamePlatformVFXResultCode::InvalidRequest);
    }

    if (DedupeKey.IsValid() &&
        !DedupeHandles.Contains(DedupeKey) &&
        DedupeHandles.Num() >= Settings->MaxDedupeEntries)
    {
        return Reject(EGamePlatformVFXResultCode::RejectedByScalability);
    }

    if (!FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
            Request,
            InstanceRegistry.Num(),
            *Settings))
    {
        return Reject(EGamePlatformVFXResultCode::RejectedByScalability);
    }

    bool bAmbiguous = false;
    const FName DefinitionId = ResolveDefinitionId(Request, bAmbiguous);
    if (bAmbiguous)
    {
        FGamePlatformVFXDiagnostics::CatalogAmbiguous(
            Request.SemanticTag,
            Request.ContextId);
        return Reject(EGamePlatformVFXResultCode::CatalogAmbiguous);
    }
    if (DefinitionId.IsNone())
    {
        FGamePlatformVFXDiagnostics::CatalogMiss(
            Request.SemanticTag,
            Request.ContextId);
        return Reject(
            Request.DefinitionId.IsNone()
                ? EGamePlatformVFXResultCode::CatalogMiss
                : EGamePlatformVFXResultCode::InvalidRequest);
    }

    FGamePlatformVFXRequest EffectiveRequest = Request;
    EffectiveRequest.DefinitionId = DefinitionId;

    Result.Handle = InstanceRegistry.Reserve(World);
    PeakTrackedInstances = FMath::Max(PeakTrackedInstances, InstanceRegistry.Num());
    UpdateRuntimeDiagnostics();

    if (DedupeKey.IsValid())
    {
        AddDedupeHandle(DedupeKey, Result.Handle);
    }

    const EGamePlatformVFXDefinitionQueueResult QueueResult =
        QueueDefinitionLoad(
            DefinitionId,
            EffectiveRequest,
            Result.Handle);

    if (QueueResult == EGamePlatformVFXDefinitionQueueResult::Failed)
    {
        CleanupInstance(Result.Handle, true);
        return Reject(EGamePlatformVFXResultCode::DefinitionLoadFailed);
    }

    Result.Code =
        QueueResult == EGamePlatformVFXDefinitionQueueResult::Executed
            ? EGamePlatformVFXResultCode::Played
            : EGamePlatformVFXResultCode::Queued;
    return Result;
}

bool UGamePlatformVFXWorldSubsystem::Stop(
    const FGamePlatformVFXHandle& Handle)
{
    check(IsInGameThread());

    if (!InstanceRegistry.IsActive(Handle) &&
        !DefinitionIdByHandle.Contains(Handle.Id))
    {
        return false;
    }

    CleanupInstance(Handle, true);
    return true;
}

bool UGamePlatformVFXWorldSubsystem::IsActive(
    const FGamePlatformVFXHandle& Handle) const
{
    check(IsInGameThread());
    return InstanceRegistry.IsActive(Handle);
}

void UGamePlatformVFXWorldSubsystem::HandleSystemFinished(
    UNiagaraComponent* Component)
{
    check(IsInGameThread());
    if (bClosing || !IsValid(Component))
    {
        return;
    }

    const FGamePlatformVFXHandle Handle =
        InstanceRegistry.FindByComponent(Component);
    if (Handle.IsValid())
    {
        // 自然结束后只回收平台记录、共享Definition使用计数和去重状态，不再次Deactivate已结束组件。
        CleanupInstance(Handle, false);
    }
}

void UGamePlatformVFXWorldSubsystem::CleanupInstance(
    const FGamePlatformVFXHandle& Handle,
    const bool bStopComponent)
{
    if (!Handle.IsValid())
    {
        return;
    }

    ClearCompositeTimers(Handle);

    const TArray<FGamePlatformVFXHandle> Children =
        InstanceRegistry.GetChildren(Handle);
    for (const FGamePlatformVFXHandle& Child : Children)
    {
        CleanupInstance(Child, true);
    }

    if (FTimerHandle* Timer = LifetimeTimers.Find(Handle.Id))
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(*Timer);
        }
        LifetimeTimers.Remove(Handle.Id);
    }

    if (UNiagaraComponent* Component = InstanceRegistry.GetComponent(Handle))
    {
        Component->OnSystemFinished.RemoveDynamic(
            this,
            &UGamePlatformVFXWorldSubsystem::HandleSystemFinished);
    }

    RemoveDedupeHandle(Handle);
    RemoveDefinitionUse(Handle);

    // 子实例已经由上面的Cleanup递归处理，Registry不再重复递归Stop。
    InstanceRegistry.Stop(Handle, bStopComponent, false);
    UpdateRuntimeDiagnostics();
}

void UGamePlatformVFXWorldSubsystem::ReleaseLease(
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

void UGamePlatformVFXWorldSubsystem::ScheduleLifetime(
    const FGamePlatformVFXHandle& Handle,
    const float Seconds)
{
    if (Seconds <= 0.0f || !FMath::IsFinite(Seconds))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    FTimerHandle& Timer = LifetimeTimers.FindOrAdd(Handle.Id);
    TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    World->GetTimerManager().SetTimer(
        Timer,
        FTimerDelegate::CreateLambda([WeakThis, Handle]()
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                if (!Self->bClosing)
                {
                    Self->Stop(Handle);
                }
            }
        }),
        Seconds,
        false);
}

void UGamePlatformVFXWorldSubsystem::AddDedupeHandle(
    const FGamePlatformVFXDedupeKey& Key,
    const FGamePlatformVFXHandle& Handle)
{
    if (!Key.IsValid() || !Handle.IsValid())
    {
        return;
    }

    DedupeHandles.Add(Key, Handle);
    DedupeKeysByHandle.Add(Handle.Id, Key);
}

void UGamePlatformVFXWorldSubsystem::RemoveDedupeHandle(
    const FGamePlatformVFXHandle& Handle)
{
    FGamePlatformVFXDedupeKey Key;
    if (!DedupeKeysByHandle.RemoveAndCopyValue(Handle.Id, Key))
    {
        return;
    }

    if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(Key);
        Existing && *Existing == Handle)
    {
        DedupeHandles.Remove(Key);
    }
}

void UGamePlatformVFXWorldSubsystem::RegisterCompositeTimer(
    const FGamePlatformVFXHandle& ParentHandle,
    const FTimerHandle& TimerHandle)
{
    if (!ParentHandle.IsValid() || !TimerHandle.IsValid())
    {
        return;
    }

    CompositeStepTimers.FindOrAdd(ParentHandle.Id).Add(TimerHandle);
}

void UGamePlatformVFXWorldSubsystem::ClearCompositeTimers(
    const FGamePlatformVFXHandle& ParentHandle)
{
    TArray<FTimerHandle> Timers;
    if (!CompositeStepTimers.RemoveAndCopyValue(ParentHandle.Id, Timers))
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        for (FTimerHandle& Timer : Timers)
        {
            World->GetTimerManager().ClearTimer(Timer);
        }
    }
}

void UGamePlatformVFXWorldSubsystem::UpdateRuntimeDiagnostics()
{
    const int32 TrackedInstances = InstanceRegistry.Num();
    PeakTrackedInstances = FMath::Max(PeakTrackedInstances, TrackedInstances);
    FGamePlatformVFXDiagnostics::UpdateRuntimeCounts(
        TrackedInstances,
        PendingInstanceCount,
        PeakTrackedInstances);
}

FGamePlatformVFXPreloadHandle UGamePlatformVFXWorldSubsystem::Preload(
    const FGamePlatformVFXRequest& Request)
{
    check(IsInGameThread());

    FGamePlatformVFXPreloadHandle Handle;
    if (bClosing)
    {
        return Handle;
    }

    const UGamePlatformVFXSettings* Settings =
        GetDefault<UGamePlatformVFXSettings>();
    if (PreloadDefinitionIds.Num() >= Settings->MaxPendingInstancePreloads)
    {
        return Handle;
    }

    bool bAmbiguous = false;
    const FName DefinitionId = ResolveDefinitionId(Request, bAmbiguous);
    if (bAmbiguous || DefinitionId.IsNone())
    {
        return Handle;
    }

    Handle.Id = FGuid::NewGuid();

    if (FGamePlatformVFXCachedDefinitionEntry* Existing = DefinitionCache.Find(DefinitionId))
    {
        Existing->PreloadHandles.Add(Handle.Id);
        Existing->LastUsedSerial = ++DefinitionCacheSerial;
        PreloadDefinitionIds.Add(Handle.Id, DefinitionId);
        FGamePlatformVFXDiagnostics::DefinitionCacheHit();
        return Handle;
    }

    if (!EnsureDefinitionCacheCapacity())
    {
        Handle.Id.Invalidate();
        return Handle;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    if (!Data)
    {
        Handle.Id.Invalidate();
        return Handle;
    }

    FPrimaryAssetId DefinitionAssetId;
    if (!BuildDefinitionAssetId(DefinitionId, DefinitionAssetId))
    {
        Handle.Id.Invalidate();
        return Handle;
    }

    FGamePlatformVFXCachedDefinitionEntry NewEntry;
    NewEntry.bLoading = true;
    NewEntry.LastUsedSerial = ++DefinitionCacheSerial;
    NewEntry.PreloadHandles.Add(Handle.Id);
    DefinitionCache.Add(DefinitionId, MoveTemp(NewEntry));
    PreloadDefinitionIds.Add(Handle.Id, DefinitionId);
    FGamePlatformVFXDiagnostics::DefinitionCacheMiss();

    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        DefinitionAssetId,
        UGamePlatformVFXDefinition::StaticClass(),
        { VFXRuntimeBundle },
        EGamePlatformDataLifetime::World,
        this,
        [WeakThis, DefinitionId](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& Result)
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                Self->HandleCachedDefinitionLoaded(
                    DefinitionId,
                    CompletedLease,
                    Result);
            }
        },
        Accepted);

    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        DefinitionCache.Remove(DefinitionId);
        PreloadDefinitionIds.Remove(Handle.Id);
        Handle.Id.Invalidate();
        return Handle;
    }

    if (FGamePlatformVFXCachedDefinitionEntry* Stored = DefinitionCache.Find(DefinitionId))
    {
        Stored->Lease = Lease;
    }
    return Handle;
}

bool UGamePlatformVFXWorldSubsystem::CancelPreload(
    const FGamePlatformVFXPreloadHandle& Handle)
{
    check(IsInGameThread());

    FName DefinitionId = NAME_None;
    if (!Handle.IsValid() ||
        !PreloadDefinitionIds.RemoveAndCopyValue(Handle.Id, DefinitionId))
    {
        return false;
    }

    FGamePlatformVFXCachedDefinitionEntry* Entry = DefinitionCache.Find(DefinitionId);
    if (!Entry)
    {
        return false;
    }

    Entry->PreloadHandles.Remove(Handle.Id);
    Entry->LastUsedSerial = ++DefinitionCacheSerial;

    // 已加载项作为有界World热缓存保留；仍在加载且已无人使用时立即取消并释放。
    if (Entry->bLoading &&
        Entry->ActiveUsers == 0 &&
        Entry->PendingRequests.IsEmpty() &&
        Entry->PreloadHandles.IsEmpty())
    {
        const FGamePlatformDataLease UnusedLease = Entry->Lease;
        DefinitionCache.Remove(DefinitionId);
        ReleaseLease(UnusedLease);
    }

    return true;
}

FGamePlatformVFXRegistrationHandle
UGamePlatformVFXWorldSubsystem::RegisterCatalog(
    UGamePlatformVFXCatalog* Catalog)
{
    check(IsInGameThread());

    if (!IsValid(Catalog))
    {
        return {};
    }

    const UGamePlatformVFXSettings* Settings =
        GetDefault<UGamePlatformVFXSettings>();
    if (CatalogRegistry.Num() >= Settings->MaxRegisteredCatalogs)
    {
        return {};
    }

    const FGamePlatformVFXRegistrationHandle Handle =
        CatalogRegistry.Register(Catalog);
    if (Handle.IsValid())
    {
        RegisteredCatalogObjects.Add(Handle.Id, Catalog);
    }
    return Handle;
}

bool UGamePlatformVFXWorldSubsystem::UnregisterCatalog(
    const FGamePlatformVFXRegistrationHandle& Handle)
{
    check(IsInGameThread());

    if (!CatalogRegistry.Unregister(Handle))
    {
        return false;
    }

    RegisteredCatalogObjects.Remove(Handle.Id);
    return true;
}

bool UGamePlatformVFXWorldSubsystem::ExecuteLoadedDefinition(
    UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ReservedHandle)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_ExecuteDefinition);
    check(IsInGameThread());

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    // Definition结构校验已在共享缓存首次加载完成时执行一次；热路径只校验本次动态参数。
    FText ValidationReason;
    if (!Definition.ValidateRequestParameters(
            Request.Parameters,
            ValidationReason))
    {
        return false;
    }

    if (!ReservedHandle.BelongsToWorld(World) ||
        !InstanceRegistry.SetDefinition(ReservedHandle, &Definition))
    {
        return false;
    }

    if (UGamePlatformVFXCompositeDefinition* Composite =
        Cast<UGamePlatformVFXCompositeDefinition>(&Definition))
    {
        if (Composite->Steps.IsEmpty())
        {
            return false;
        }

        const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
        FGamePlatformVFXCompositeRunner::Run(
            *World,
            *Composite,
            Request,
            ReservedHandle,
            [WeakThis](
                const FName ChildDefinitionId,
                const FGamePlatformVFXRequest& ChildRequest,
                const FGamePlatformVFXHandle& ParentHandle)
            {
                if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
                {
                    Self->PlayDefinitionId(
                        ChildDefinitionId,
                        ChildRequest,
                        ParentHandle);
                }
            },
            [WeakThis](
                const FGamePlatformVFXHandle& ParentHandle,
                const FTimerHandle& TimerHandle)
            {
                if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
                {
                    Self->RegisterCompositeTimer(
                        ParentHandle,
                        TimerHandle);
                }
            });

        ScheduleLifetime(
            ReservedHandle,
            Composite->MaxTotalLifetimeSeconds);
        return true;
    }

    const UGamePlatformVFXSettings* Settings =
        GetDefault<UGamePlatformVFXSettings>();
    const bool bUsePool =
        FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(
            Definition,
            Request,
            *Settings);

    UNiagaraComponent* Component =
        FGamePlatformVFXNiagaraExecutor::Spawn(
            *World,
            Definition,
            Request,
            bUsePool);
    if (!IsValid(Component) ||
        !InstanceRegistry.AttachComponent(
            ReservedHandle,
            Component,
            bUsePool))
    {
        return false;
    }

    // Spawn阶段禁止自动激活；先建立平台生命周期监听，再允许Niagara开始运行。
    Component->OnSystemFinished.AddUniqueDynamic(
        this,
        &UGamePlatformVFXWorldSubsystem::HandleSystemFinished);
    ScheduleLifetime(
        ReservedHandle,
        Definition.GetMaxLifetimeSeconds());
    Component->Activate(true);
    return true;
}

void UGamePlatformVFXWorldSubsystem::PlayDefinitionId(
    const FName DefinitionId,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ParentHandle)
{
    check(IsInGameThread());

    if (!InstanceRegistry.IsActive(ParentHandle))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World) || bClosing)
    {
        return;
    }

    const UGamePlatformVFXSettings* Settings =
        GetDefault<UGamePlatformVFXSettings>();
    if (!FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
            Request,
            InstanceRegistry.Num(),
            *Settings))
    {
        FGamePlatformVFXDiagnostics::RequestRejected();
        return;
    }

    FGamePlatformVFXRequest ChildRequest = Request;
    ChildRequest.DefinitionId = DefinitionId;

    const FGamePlatformVFXHandle ChildHandle =
        InstanceRegistry.Reserve(World);
    PeakTrackedInstances = FMath::Max(PeakTrackedInstances, InstanceRegistry.Num());
    UpdateRuntimeDiagnostics();

    if (!InstanceRegistry.AddChild(
            ParentHandle,
            ChildHandle))
    {
        InstanceRegistry.Stop(ChildHandle);
        UpdateRuntimeDiagnostics();
        return;
    }

    FGamePlatformVFXDiagnostics::CompositeChild();

    const EGamePlatformVFXDefinitionQueueResult QueueResult =
        QueueDefinitionLoad(
            DefinitionId,
            ChildRequest,
            ChildHandle);
    if (QueueResult == EGamePlatformVFXDefinitionQueueResult::Failed)
    {
        FGamePlatformVFXDiagnostics::RequestRejected();
        CleanupInstance(ChildHandle, true);
    }
}
