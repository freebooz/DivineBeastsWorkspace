// 本文件属于GamePlatform平台层 GamePlatformVFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 世界VFX执行：定义元数据缓存与实例选中资源独立Data租约；有限基础回退，终态一次，关停撤销全部自有需求。
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"

#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
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
#include "UObject/StrongObjectPtr.h"

namespace
{

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
    ++WorldLifecycleGeneration;
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
    if (bClosing) return; // 终态观察者可以重入关闭，第一次清理栈仍负责全部自有账本。
    bClosing = true;
    ++WorldLifecycleGeneration;

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

    TArray<FGamePlatformVFXHandle> ClosingHandles;
    for (const auto& Pair : PlaybackSnapshots)
        if (Pair.Value.State == EGamePlatformVFXPlaybackState::Loading || Pair.Value.State == EGamePlatformVFXPlaybackState::Playing)
            ClosingHandles.Add(Pair.Value.Handle);
    for (const auto& Handle : ClosingHandles) CleanupInstance(Handle, true, EGamePlatformVFXPlaybackState::WorldDestroyed,
        EGamePlatformVFXResultCode::InvalidWorld, TEXT("所属世界正在退出。"));
    PlaybackCompleted.Clear();
    TArray<FGamePlatformVFXPreloadHandle> ClosingPreloads;
    for (const auto& Pair : PreloadRecords) ClosingPreloads.Add(Pair.Value.Handle);
    for (const auto& Handle : ClosingPreloads) FinishPreload(Handle, EGamePlatformVFXPreloadState::WorldDestroyed, TEXT("预载所属世界退出。"));
    // 先停实例，再一次性释放World共享Definition租约，避免每个历史实例产生独立Data释放记录。
    InstanceRegistry.Reset();
    DefinitionIdByHandle.Reset();
    DedupeHandles.Reset();
    DedupeKeysByHandle.Reset();
    TerminalOccurrences.Reset();
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
            const auto ResourceResult = QueueSelectedResources(*Definition, Request, ReservedHandle);
            if (ResourceResult != EGamePlatformVFXDefinitionQueueResult::Failed) return ResourceResult;
            return TryDefinitionFallback(Request, ReservedHandle) ? EGamePlatformVFXDefinitionQueueResult::Queued : EGamePlatformVFXDefinitionQueueResult::Failed;
        }

        // Lease仍在但对象不可读属于异常状态；仅在没有活跃使用者时清掉并重新加载。
        if (Existing->ActiveUsers > 0 ||
            !Existing->PendingRequests.IsEmpty())
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
        {},
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
        Entry->Lease.LeaseId != Lease.LeaseId || Entry->Lease.Generation != Lease.Generation ||
        Entry->Lease.ScopeId != Lease.ScopeId || Entry->Lease.IssuerProof != Lease.IssuerProof || Entry->Lease.DefinitionId != Lease.DefinitionId)
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
        const FGamePlatformDataLease FailedLease = Entry->Lease;

        PendingInstanceCount = FMath::Max(
            0,
            PendingInstanceCount - FailedPending.Num());
        DefinitionCache.Remove(DefinitionId);

        ReleaseLease(FailedLease);

        for (const TPair<FGuid, FGamePlatformVFXPendingDefinitionRequest>& Pair : FailedPending)
        {
            CleanupInstance(Pair.Value.Handle, true, EGamePlatformVFXPlaybackState::Failed,
                EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("定义元数据加载失败、未配置或结构无效。"));
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
        if (bClosing || !World || World->bIsTearingDown) return;
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

        if (QueueSelectedResources(*Definition, Pending.Request, Pending.Handle) == EGamePlatformVFXDefinitionQueueResult::Failed &&
            !TryDefinitionFallback(Pending.Request, Pending.Handle))
            CleanupInstance(Pending.Handle, true, EGamePlatformVFXPlaybackState::Failed,
                EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("选中资源不可用且基础回退未配置或回退链失败。"));
    }

    UpdateRuntimeDiagnostics();
}

EGamePlatformVFXDefinitionQueueResult UGamePlatformVFXWorldSubsystem::QueueSelectedResources(
    UGamePlatformVFXDefinition& Definition, const FGamePlatformVFXRequest& Request, const FGamePlatformVFXHandle& Handle)
{
    UWorld* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data || bClosing || !World || World->bIsTearingDown || !InstanceRegistry.IsActive(Handle)) return EGamePlatformVFXDefinitionQueueResult::Failed;
    TArray<FSoftObjectPath> Paths;
    if (Definition.GetBehavior() != EGamePlatformVFXBehavior::Composite)
    {
        const auto System = Request.bUseBaseNiagaraSystem ? Definition.GetNiagaraSystem() : Definition.ResolveNiagaraSystem(Request.PlatformId, Request.QualityTier);
        if (System.IsNull()) return EGamePlatformVFXDefinitionQueueResult::Failed;
        Paths.Add(System.ToSoftObjectPath());
    }
    if (!Definition.GetEffectType().IsNull()) Paths.AddUnique(Definition.GetEffectType().ToSoftObjectPath());
    for (const auto& Asset : Definition.GetPreloadAssets()) if (!Asset.IsNull()) Paths.AddUnique(Asset.ToSoftObjectPath());
    if (Paths.IsEmpty()) return ExecuteLoadedDefinition(Definition, Request, Handle) ? EGamePlatformVFXDefinitionQueueResult::Executed : EGamePlatformVFXDefinitionQueueResult::Failed;
    if (PendingInstanceCount >= GetDefault<UGamePlatformVFXSettings>()->MaxPendingInstancePreloads) return EGamePlatformVFXDefinitionQueueResult::Failed;
    FInstanceResourceRequest Pending; Pending.Handle = Handle; Pending.Request = Request; Pending.bLoading = true;
    InstanceResources.Add(Handle.Id, Pending); ++PendingInstanceCount;
    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const auto Lease = Data->AcquireResources(Paths, EGamePlatformDataLifetime::World, this,
        [WeakThis, Handle](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        { if (auto* Self = WeakThis.Get()) Self->HandleSelectedResourcesLoaded(Handle, CompletedLease, Result); }, Accepted);
    if (!Accepted.IsSuccess() || !Lease.IsValid()) { ReleaseInstanceResources(Handle); return EGamePlatformVFXDefinitionQueueResult::Failed; }
    InstanceResources.FindChecked(Handle.Id).Lease = Lease;
    return EGamePlatformVFXDefinitionQueueResult::Queued;
}

void UGamePlatformVFXWorldSubsystem::HandleSelectedResourcesLoaded(const FGamePlatformVFXHandle Handle,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    auto* Stored = InstanceResources.Find(Handle.Id);
    auto* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (bClosing || !World || World->bIsTearingDown || !InstanceRegistry.IsActive(Handle) || !Stored || !Stored->bLoading ||
        Stored->Handle.Generation != Handle.Generation || Stored->Lease.LeaseId != Lease.LeaseId || Stored->Lease.Generation != Lease.Generation ||
        Stored->Lease.ScopeId != Lease.ScopeId || Stored->Lease.IssuerProof != Lease.IssuerProof || Stored->Lease.ResourcePaths != Lease.ResourcePaths) return;
    Stored->bLoading = false; PendingInstanceCount = FMath::Max(0, PendingInstanceCount - 1);
    const auto Pending = *Stored;
    if (Pending.bFallbackMetadata)
    {
        const auto* Fallback = Result.IsSuccess() && Data && Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded && Lease.ResourcePaths.Num() == 1
            ? Cast<UGamePlatformVFXDefinition>(Lease.ResourcePaths[0].ResolveObject()) : nullptr;
        const FName FallbackId = Fallback ? Fallback->GetDefinitionId() : NAME_None;
        auto& Visited = FallbackVisitedDefinitions.FindOrAdd(Handle.Id);
        FPrimaryAssetId AssetId;
        const bool bCanTry = BuildDefinitionAssetId(FallbackId, AssetId) && !Visited.Contains(FallbackId) &&
            Visited.Num() < FMath::Max(1, FMath::Clamp(GetDefault<UGamePlatformVFXSettings>()->MaxDefinitionFallbackDepth, 1, 64));
        ReleaseInstanceResources(Handle);
        if (bCanTry)
        {
            Visited.Add(FallbackId); RemoveDefinitionUse(Handle);
            auto Request = Pending.Request; Request.DefinitionId = FallbackId; Request.bUseBaseNiagaraSystem = false;
            if (QueueDefinitionLoad(FallbackId, Request, Handle) != EGamePlatformVFXDefinitionQueueResult::Failed) return;
        }
        CleanupInstance(Handle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::DefinitionLoadFailed,
            TEXT("回退定义缺失、非法逻辑身份、检测到环或超过配置深度。"));
        return;
    }
    const auto* DefinitionId = DefinitionIdByHandle.Find(Handle.Id);
    const auto* Cached = DefinitionId ? DefinitionCache.Find(*DefinitionId) : nullptr;
    auto* Definition = Cached ? Cached->Definition.Get() : nullptr;
    if (!Result.IsSuccess() || !Data || Data->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded || !Definition)
    {
        if (!TryDefinitionFallback(Pending.Request, Handle)) CleanupInstance(Handle, true, EGamePlatformVFXPlaybackState::Failed,
            EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("选中资源和基础回退不可用。"));
        return;
    }
    if (!ExecuteLoadedDefinition(*Definition, Pending.Request, Handle))
        CleanupInstance(Handle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::InvalidRequest,
            TEXT("资源已加载但参数、附着目标或Niagara执行失败。"));
}

bool UGamePlatformVFXWorldSubsystem::TryDefinitionFallback(const FGamePlatformVFXRequest& Request, const FGamePlatformVFXHandle& Handle)
{
    const auto* Id = DefinitionIdByHandle.Find(Handle.Id); const auto* Cached = Id ? DefinitionCache.Find(*Id) : nullptr;
    auto* Definition = Cached ? Cached->Definition.Get() : nullptr;
    if (!Request.bAllowFallback || !Definition || !InstanceRegistry.IsActive(Handle) || bClosing) return false;
    ReleaseInstanceResources(Handle);
    const auto Selected = Definition->ResolveNiagaraSystem(Request.PlatformId, Request.QualityTier);
    if (!Request.bUseBaseNiagaraSystem && !Definition->GetNiagaraSystem().IsNull() && Selected != Definition->GetNiagaraSystem())
    {
        auto BaseRequest = Request; BaseRequest.bUseBaseNiagaraSystem = true;
        if (auto* Snapshot = PlaybackSnapshots.Find(Handle.Id)) Snapshot->Diagnostic = TEXT("可选变体资源失败，正在尝试同定义基础Niagara。");
        if (QueueSelectedResources(*Definition, BaseRequest, Handle) != EGamePlatformVFXDefinitionQueueResult::Failed) return true;
        // 基础资源的同步拒绝仍可尝试明确声明的回退定义；不假装基础资源成功。
    }
    const auto Path = Definition->GetFallbackDefinition().ToSoftObjectPath();
    if (!Path.IsValid()) return false;
    auto* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (!Data || PendingInstanceCount >= GetDefault<UGamePlatformVFXSettings>()->MaxPendingInstancePreloads) return false;
    FInstanceResourceRequest Pending; Pending.Handle = Handle; Pending.Request = Request;
    Pending.bFallbackMetadata = true; Pending.bLoading = true;
    InstanceResources.Add(Handle.Id, Pending); ++PendingInstanceCount;
    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this); FGamePlatformResult Accepted;
    const auto Lease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, this,
        [WeakThis, Handle](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        { if (auto* Self = WeakThis.Get()) Self->HandleSelectedResourcesLoaded(Handle, CompletedLease, Result); }, Accepted);
    if (!Accepted.IsSuccess() || !Lease.IsValid()) { ReleaseInstanceResources(Handle); return false; }
    InstanceResources.FindChecked(Handle.Id).Lease = Lease;
    if (auto* Snapshot = PlaybackSnapshots.Find(Handle.Id)) Snapshot->Diagnostic = TEXT("基础资源失败，正在加载声明的FallbackDefinition。");
    return true;
}

void UGamePlatformVFXWorldSubsystem::ReleaseInstanceResources(const FGamePlatformVFXHandle& Handle)
{
    FInstanceResourceRequest Removed;
    if (!InstanceResources.RemoveAndCopyValue(Handle.Id, Removed)) return;
    if (Removed.bLoading) PendingInstanceCount = FMath::Max(0, PendingInstanceCount - 1);
    auto* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr)
        if (Removed.Lease.IsValid()) Data->ReleaseResources(Removed.Lease);
}

FGamePlatformVFXPlaybackSnapshot UGamePlatformVFXWorldSubsystem::GetPlaybackSnapshot(const FGamePlatformVFXHandle& Handle) const
{
    check(IsInGameThread());
    if (const auto* Snapshot = PlaybackSnapshots.Find(Handle.Id))
        if (Snapshot->Handle.Generation == Handle.Generation && Snapshot->Handle.World == Handle.World) return *Snapshot;
    return {};
}
FDelegateHandle UGamePlatformVFXWorldSubsystem::AddCompletionHandler(const FGamePlatformVFXPlaybackCompleted::FDelegate& Handler)
{
    check(IsInGameThread()); return Handler.IsBound() ? PlaybackCompleted.Add(Handler) : FDelegateHandle();
}
void UGamePlatformVFXWorldSubsystem::RemoveCompletionHandler(FDelegateHandle Handle)
{
    check(IsInGameThread()); PlaybackCompleted.Remove(Handle);
}
void UGamePlatformVFXWorldSubsystem::CompletePlayback(const FGamePlatformVFXHandle& Handle,
    EGamePlatformVFXPlaybackState State, EGamePlatformVFXResultCode Code, const FString& Diagnostic)
{
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> KeepService(this);
    if (bClosing) { State = EGamePlatformVFXPlaybackState::WorldDestroyed; Code = EGamePlatformVFXResultCode::InvalidWorld; }
    auto* Stored = PlaybackSnapshots.Find(Handle.Id);
    if (Stored && Stored->State != EGamePlatformVFXPlaybackState::Loading && Stored->State != EGamePlatformVFXPlaybackState::Playing) return;
    auto Snapshot = Stored ? *Stored : FGamePlatformVFXPlaybackSnapshot();
    Snapshot.Handle = Handle; Snapshot.State = State; Snapshot.Code = Code;
    if (!Diagnostic.IsEmpty()) Snapshot.Diagnostic = Diagnostic;
    if (CompletedPlaybackOrder.Num() >= 512) { PlaybackSnapshots.Remove(CompletedPlaybackOrder[0]); CompletedPlaybackOrder.RemoveAt(0); }
    CompletedPlaybackOrder.Add(Handle.Id); PlaybackSnapshots.Add(Handle.Id, Snapshot);
    PlaybackCompleted.Broadcast(Snapshot);
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
            !Entry.PendingRequests.IsEmpty())
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
        Entry->PendingRequests.IsEmpty())
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
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> KeepService(this);

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
    const uint64 RequestLifecycleGeneration = WorldLifecycleGeneration;
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const auto IsRequestScopeCurrent = [this, World, RequestLifecycleGeneration]()
    {
        return !bClosing && IsValid(World) && !World->bIsTearingDown && GetWorld() == World &&
            WorldLifecycleGeneration == RequestLifecycleGeneration;
    };

    PruneTerminalOccurrences();
    if (DedupeKey.IsValid())
    {
        if (Request.PredictionState == EGamePlatformVFXPredictionState::Cancelled)
        {
            if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
            {
                const FGamePlatformVFXHandle ExistingHandle = *Existing;
                Result.Handle = ExistingHandle;
                Stop(ExistingHandle);
                if (!IsRequestScopeCurrent()) return Reject(EGamePlatformVFXResultCode::InvalidWorld);
                FGamePlatformVFXDiagnostics::DedupeHit();
            }
            RecordTerminalOccurrence(DedupeKey, true);
            Result.Code = EGamePlatformVFXResultCode::Cancelled;
            return Result;
        }

        if (const FTerminalOccurrence* Terminal = TerminalOccurrences.Find(DedupeKey))
        {
            if (Terminal->bCancelled || Request.PredictionState != EGamePlatformVFXPredictionState::Corrected)
            {
                Result.Code = Terminal->bCancelled ? EGamePlatformVFXResultCode::Cancelled : Terminal->CompletionCode;
                return Result;
            }
            // 显式纠正可替换已正常完成的预测，权威取消在保留期内不可复活。
            TerminalOccurrences.Remove(DedupeKey);
        }

        if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
        {
            const FGamePlatformVFXHandle ExistingHandle = *Existing;
            if (Request.PredictionState == EGamePlatformVFXPredictionState::Corrected)
            {
                Stop(ExistingHandle);
                // Stop公开完成通知允许关闭或旅行；原World/服务世代仍存活才能启动替换。
                if (!IsRequestScopeCurrent()) return Reject(EGamePlatformVFXResultCode::InvalidWorld);
                TerminalOccurrences.Remove(DedupeKey);
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
    FGamePlatformVFXPlaybackSnapshot Snapshot; Snapshot.Handle = Result.Handle;
    Snapshot.State = EGamePlatformVFXPlaybackState::Loading; Snapshot.Code = EGamePlatformVFXResultCode::Queued;
    Snapshot.DefinitionId = DefinitionId; Snapshot.RequestId = Request.RequestId; PlaybackSnapshots.Add(Result.Handle.Id, Snapshot);
    FallbackVisitedDefinitions.FindOrAdd(Result.Handle.Id).Add(DefinitionId);
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

    if (!IsRequestScopeCurrent()) return Reject(EGamePlatformVFXResultCode::InvalidWorld);

    if (QueueResult == EGamePlatformVFXDefinitionQueueResult::Failed)
    {
        CleanupInstance(Result.Handle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("定义ID非法、Data未就绪或加载申请被拒绝。"));
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

    if (!InstanceRegistry.OwnsHandle(Handle))
    {
        return false;
    }

    if (const auto* Key = DedupeKeysByHandle.Find(Handle.Id)) RecordTerminalOccurrence(*Key, true);
    CleanupInstance(Handle, true, EGamePlatformVFXPlaybackState::Cancelled, EGamePlatformVFXResultCode::Cancelled);
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
    const bool bStopComponent, const EGamePlatformVFXPlaybackState State,
    const EGamePlatformVFXResultCode Code, const FString& Diagnostic)
{
    // Niagara自然结束先清Active再广播Finished；清理资格只能来自完整句柄账本。
    if (!InstanceRegistry.OwnsHandle(Handle) || CleaningInstances.Contains(Handle.Id)) return;
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> KeepService(this);
    CleaningInstances.Add(Handle.Id);
    const FGuid RootId = CompositeRootByInstance.FindRef(Handle.Id);
    const auto* RootBudget = CompositeRootBudgets.Find(RootId);
    const auto RootHandle = RootBudget ? RootBudget->Handle : FGamePlatformVFXHandle();
    ClearCompositeTimers(Handle);

    const TArray<FGamePlatformVFXHandle> Children =
        InstanceRegistry.GetChildren(Handle);
    for (const FGamePlatformVFXHandle& Child : Children)
    {
        CleanupInstance(Child, true, State == EGamePlatformVFXPlaybackState::Completed ? EGamePlatformVFXPlaybackState::Cancelled : State,
            State == EGamePlatformVFXPlaybackState::Completed ? EGamePlatformVFXResultCode::Cancelled : Code, Diagnostic);
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

    if (const auto* Key = DedupeKeysByHandle.Find(Handle.Id))
    {
        RecordTerminalOccurrence(*Key, State == EGamePlatformVFXPlaybackState::Cancelled);
        if (auto* Terminal = TerminalOccurrences.Find(*Key)) if (!Terminal->bCancelled) Terminal->CompletionCode = Code;
    }
    RemoveDedupeHandle(Handle);
    RemoveDefinitionUse(Handle);
    ReleaseInstanceResources(Handle);
    FallbackVisitedDefinitions.Remove(Handle.Id);

    // 子实例已经由上面的Cleanup递归处理，Registry不再重复递归Stop。
    InstanceRegistry.Stop(Handle, bStopComponent, false);
    CompositeRootByInstance.Remove(Handle.Id);
    if (Handle.Id == RootId) CompositeRootBudgets.Remove(RootId);
    CleaningInstances.Remove(Handle.Id);
    UpdateRuntimeDiagnostics();
    CompletePlayback(Handle, State, Code, Diagnostic);
    if (State == EGamePlatformVFXPlaybackState::Failed && RootHandle.IsValid() && RootHandle.Id != Handle.Id &&
        !bClosing && InstanceRegistry.OwnsHandle(RootHandle) && !CleaningInstances.Contains(RootHandle.Id))
        CleanupInstance(RootHandle, true, State, Code, TEXT("复合必需子实例失败，已取消根请求及全部兄弟实例。"));
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
    if (Seconds <= 0.0f || !FMath::IsFinite(Seconds) || !InstanceRegistry.IsActive(Handle))
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

void UGamePlatformVFXWorldSubsystem::PruneTerminalOccurrences()
{
    const double Now = FPlatformTime::Seconds();
    for (auto It=TerminalOccurrences.CreateIterator(); It; ++It)
        if (It.Value().ExpiresAtSeconds <= Now) It.RemoveCurrent();
}

void UGamePlatformVFXWorldSubsystem::RecordTerminalOccurrence(const FGamePlatformVFXDedupeKey& Key, const bool bCancelled)
{
    if (!Key.IsValid()) return;
    PruneTerminalOccurrences();
    if (auto* Existing=TerminalOccurrences.Find(Key))
    {
        Existing->bCancelled |= bCancelled;
        return; // 重复通知不延长终态期限，避免攻击式保持历史。
    }
    const int32 Limit=FMath::Max(1, GetDefault<UGamePlatformVFXSettings>()->MaxDedupeEntries);
    if (TerminalOccurrences.Num() >= Limit)
    {
        FGamePlatformVFXDedupeKey Oldest; double Time=TNumericLimits<double>::Max();
        for (const auto& Pair:TerminalOccurrences)
            if (Pair.Value.ExpiresAtSeconds < Time) { Time=Pair.Value.ExpiresAtSeconds; Oldest=Pair.Key; }
        TerminalOccurrences.Remove(Oldest);
    }
    FTerminalOccurrence Entry; Entry.ExpiresAtSeconds=FPlatformTime::Seconds()+30.0; Entry.bCancelled=bCancelled;
    TerminalOccurrences.Add(Key, Entry);
}

bool UGamePlatformVFXWorldSubsystem::RegisterCompositeTimer(
    const FGamePlatformVFXHandle& ParentHandle,
    const FTimerHandle& TimerHandle)
{
    if (!ParentHandle.IsValid() || !TimerHandle.IsValid())
    {
        return false;
    }

    if (bClosing || !InstanceRegistry.OwnsHandle(ParentHandle) || CleaningInstances.Contains(ParentHandle.Id))
    { if (auto* World = GetWorld()) { auto TimerCopy = TimerHandle; World->GetTimerManager().ClearTimer(TimerCopy); } return false; }
    CompositeStepTimers.FindOrAdd(ParentHandle.Id).Add(TimerHandle);
    return true;
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

FGamePlatformVFXPreloadHandle UGamePlatformVFXWorldSubsystem::Preload(const FGamePlatformVFXRequest& Request)
{
    check(IsInGameThread());
    auto* World = GetWorld();
    if (bClosing || !World || World->bIsTearingDown || PreloadRecords.Num() >= GetDefault<UGamePlatformVFXSettings>()->MaxPendingInstancePreloads) return {};
    bool bAmbiguous = false; const FName DefinitionId = ResolveDefinitionId(Request, bAmbiguous);
    if (bAmbiguous || DefinitionId.IsNone()) return {};
    FPreloadRecord Record; Record.Handle.Id = FGuid::NewGuid(); Record.Handle.World = World;
    Record.Handle.Generation = ++NextPreloadGeneration; Record.Request = Request;
    const auto Handle = Record.Handle; PreloadRecords.Add(Handle.Id, MoveTemp(Record));
    if (!QueuePreloadDefinition(Handle, DefinitionId, 0))
    { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("预载Data申请被拒绝或定义ID非法。")); return {}; }
    return Handle;
}

bool UGamePlatformVFXWorldSubsystem::QueuePreloadDefinition(const FGamePlatformVFXPreloadHandle& Handle,
    const FName DefinitionId, const int32 Depth)
{
    auto* Record = PreloadRecords.Find(Handle.Id); auto* World = GetWorld();
    auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    FPrimaryAssetId AssetId;
    if (!Record || !Data || !BuildDefinitionAssetId(DefinitionId, AssetId) || Depth > Record->MaxDepth) return false;
    if (Record->Definitions.Contains(DefinitionId)) return true; // Data已校验依赖图无环，重复需求仅持有一次。
    if (Record->Definitions.Num() >= Record->MaxDefinitions) return false;
    Record->Definitions.Add(DefinitionId, {}); ++Record->PendingLoads;
    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this); FGamePlatformResult Accepted;
    const auto Lease = Data->AcquireDefinition(AssetId, UGamePlatformVFXDefinition::StaticClass(), {}, EGamePlatformDataLifetime::World,
        this, [WeakThis, Handle, DefinitionId, Depth](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        { if (auto* Self = WeakThis.Get()) Self->HandlePreloadDefinitionLoaded(Handle, DefinitionId, Depth, CompletedLease, Result); }, Accepted);
    if (!Accepted.IsSuccess() || !Lease.IsValid()) return false;
    PreloadRecords.FindChecked(Handle.Id).Definitions[DefinitionId] = Lease;
    return true;
}

void UGamePlatformVFXWorldSubsystem::HandlePreloadDefinitionLoaded(const FGamePlatformVFXPreloadHandle Handle,
    const FName DefinitionId, const int32 Depth, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* Record = PreloadRecords.Find(Handle.Id); auto* World = GetWorld();
    auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    const auto* Expected = Record ? Record->Definitions.Find(DefinitionId) : nullptr;
    if (bClosing || !World || World->bIsTearingDown || Handle.World.Get() != World || !Record || Record->Handle.Generation != Handle.Generation ||
        !Expected || Expected->LeaseId != Lease.LeaseId || Expected->Generation != Lease.Generation ||
        Expected->ScopeId != Lease.ScopeId || Expected->IssuerProof != Lease.IssuerProof || Expected->DefinitionId != Lease.DefinitionId || Record->CompletedLeases.Contains(Lease.LeaseId)) return;
    const auto* Definition = Result.IsSuccess() && Data && Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded
        ? Cast<UGamePlatformVFXDefinition>(Data->GetLoadedDefinition(Lease)) : nullptr;
    if (!Definition)
    { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("预载必需定义未配置、加载失败或不符VFX类型。")); return; }
    Record->CompletedLeases.Add(Lease.LeaseId); --Record->PendingLoads;
    TArray<FSoftObjectPath> Paths;
    if (const auto* Composite = Cast<UGamePlatformVFXCompositeDefinition>(Definition))
    {
        if (Depth == 0) { Record->MaxDepth = Composite->MaxDepth; Record->MaxDefinitions = Composite->MaxChildren + 1; }
        const auto Steps = Composite->Steps;
        for (const auto& Step : Steps)
            if (!QueuePreloadDefinition(Handle, Step.DefinitionId, Depth + 1))
            { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("复合预载超过根配置的总子定义/深度界限，或子定义申请失败。")); return; }
    }
    else
    {
        const auto System = Record->Request.bUseBaseNiagaraSystem ? Definition->GetNiagaraSystem() :
            Definition->ResolveNiagaraSystem(Record->Request.PlatformId, Record->Request.QualityTier);
        if (System.IsNull()) { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("预载选中Niagara未配置。")); return; }
        Paths.AddUnique(System.ToSoftObjectPath());
    }
    if (!Definition->GetEffectType().IsNull()) Paths.AddUnique(Definition->GetEffectType().ToSoftObjectPath());
    for (const auto& Asset : Definition->GetPreloadAssets()) if (!Asset.IsNull()) Paths.AddUnique(Asset.ToSoftObjectPath());
    if (!Paths.IsEmpty())
    {
        ++PreloadRecords.FindChecked(Handle.Id).PendingLoads;
        const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this); FGamePlatformResult Accepted;
        const auto ResourceLease = Data->AcquireResources(Paths, EGamePlatformDataLifetime::World, this,
            [WeakThis, Handle](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& ResourceResult)
            { if (auto* Self = WeakThis.Get()) Self->HandlePreloadResourcesLoaded(Handle, CompletedLease, ResourceResult); }, Accepted);
        if (!Accepted.IsSuccess() || !ResourceLease.IsValid())
        { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("预载选中资源申请被拒绝。")); return; }
        PreloadRecords.FindChecked(Handle.Id).Resources.Add(ResourceLease);
    }
    if (auto* Current = PreloadRecords.Find(Handle.Id))
        if (Current->PendingLoads == 0) Current->State = EGamePlatformVFXPreloadState::Ready;
}

void UGamePlatformVFXWorldSubsystem::HandlePreloadResourcesLoaded(const FGamePlatformVFXPreloadHandle Handle,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* Record = PreloadRecords.Find(Handle.Id); auto* World = GetWorld();
    auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (bClosing || !World || World->bIsTearingDown || Handle.World.Get() != World || !Record || Record->Handle.Generation != Handle.Generation ||
        !Record->Resources.ContainsByPredicate([&](const auto& Item) { return Item.LeaseId == Lease.LeaseId && Item.Generation == Lease.Generation &&
            Item.ScopeId == Lease.ScopeId && Item.IssuerProof == Lease.IssuerProof && Item.ResourcePaths == Lease.ResourcePaths; }) ||
        Record->CompletedLeases.Contains(Lease.LeaseId)) return;
    if (!Result.IsSuccess() || !Data || Data->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded)
    { FinishPreload(Handle, EGamePlatformVFXPreloadState::Failed, TEXT("预载选中Niagara/EffectType/必需软资源实际加载失败。")); return; }
    Record->CompletedLeases.Add(Lease.LeaseId); --Record->PendingLoads;
    if (Record->PendingLoads == 0) Record->State = EGamePlatformVFXPreloadState::Ready;
}

void UGamePlatformVFXWorldSubsystem::FinishPreload(const FGamePlatformVFXPreloadHandle& Handle,
    const EGamePlatformVFXPreloadState State, const FString& Error)
{
    FPreloadRecord Removed;
    if (!PreloadRecords.RemoveAndCopyValue(Handle.Id, Removed)) return;
    auto* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr)
    {
        for (const auto& Pair : Removed.Definitions) if (Pair.Value.IsValid()) Data->ReleaseDefinition(Pair.Value);
        for (const auto& Lease : Removed.Resources) if (Lease.IsValid()) Data->ReleaseResources(Lease);
    }
    FPreloadTerminal Terminal; Terminal.Handle = Handle; Terminal.State = State; Terminal.Error = Error;
    if (PreloadTerminalOrder.Num() >= 512) { PreloadTerminals.Remove(PreloadTerminalOrder[0]); PreloadTerminalOrder.RemoveAt(0); }
    PreloadTerminalOrder.Add(Handle.Id); PreloadTerminals.Add(Handle.Id, MoveTemp(Terminal));
}

bool UGamePlatformVFXWorldSubsystem::CancelPreload(const FGamePlatformVFXPreloadHandle& Handle)
{
    check(IsInGameThread());
    const auto* Record = PreloadRecords.Find(Handle.Id);
    if (!Record || Record->Handle.Generation != Handle.Generation || Record->Handle.World != Handle.World) return false;
    FinishPreload(Handle, EGamePlatformVFXPreloadState::Cancelled, TEXT("本调用者取消预载，自有定义和资源租约已释放。"));
    return true;
}

EGamePlatformVFXPreloadState UGamePlatformVFXWorldSubsystem::GetPreloadState(const FGamePlatformVFXPreloadHandle& Handle, FString& OutError) const
{
    check(IsInGameThread()); OutError.Reset();
    if (const auto* Record = PreloadRecords.Find(Handle.Id))
        if (Record->Handle.Generation == Handle.Generation && Record->Handle.World == Handle.World) return Record->State;
    if (const auto* Terminal = PreloadTerminals.Find(Handle.Id))
        if (Terminal->Handle.Generation == Handle.Generation && Terminal->Handle.World == Handle.World)
        { OutError = Terminal->Error; return Terminal->State; }
    return EGamePlatformVFXPreloadState::Invalid;
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
    const TStrongObjectPtr<UGamePlatformVFXDefinition> KeepDefinition(&Definition);
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> KeepService(this);

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    // 取消及作用域在每次执行核对，不能因Definition缓存命中而跳过弱附着目标检查。
    if (bClosing || World->bIsTearingDown || !InstanceRegistry.IsActive(ReservedHandle) ||
        !FGamePlatformVFXNiagaraExecutor::IsAttachmentValid(*World, Definition, Request.SpawnContext))
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

        if (!CompositeRootByInstance.Contains(ReservedHandle.Id))
        {
            FCompositeRootBudget Budget; Budget.Handle = ReservedHandle; Budget.MaxChildren = Composite->MaxChildren; Budget.MaxDepth = Composite->MaxDepth;
            CompositeRootBudgets.Add(ReservedHandle.Id, Budget); CompositeRootByInstance.Add(ReservedHandle.Id, ReservedHandle.Id);
        }
        const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
        const bool bStartedComposite = FGamePlatformVFXCompositeRunner::Run(
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
                    return Self->PlayDefinitionId(
                        ChildDefinitionId,
                        ChildRequest,
                        ParentHandle);
                }
                return false;
            },
            [WeakThis](
                const FGamePlatformVFXHandle& ParentHandle,
                const FTimerHandle& TimerHandle)
            {
                if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
                {
                    return Self->RegisterCompositeTimer(
                        ParentHandle,
                        TimerHandle);
                }
                return false;
            });

        if (!bStartedComposite)
        {
            CleanupInstance(ReservedHandle, true, EGamePlatformVFXPlaybackState::Failed,
                EGamePlatformVFXResultCode::InvalidRequest, TEXT("Composite必需步骤未通过边界检查或未实际受理，已撤销整树。"));
            return false;
        }
        if (bClosing || World->bIsTearingDown || !InstanceRegistry.OwnsHandle(ReservedHandle)) return false;
        ScheduleLifetime(
            ReservedHandle,
            Composite->MaxTotalLifetimeSeconds);
        if (auto* Snapshot = PlaybackSnapshots.Find(ReservedHandle.Id))
        { Snapshot->State = EGamePlatformVFXPlaybackState::Playing; Snapshot->Code = EGamePlatformVFXResultCode::Played; Snapshot->DefinitionId = Definition.GetDefinitionId(); }
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
    if (auto* Snapshot = PlaybackSnapshots.Find(ReservedHandle.Id))
        if (InstanceRegistry.IsActive(ReservedHandle))
        { Snapshot->State = EGamePlatformVFXPlaybackState::Playing; Snapshot->Code = EGamePlatformVFXResultCode::Played; Snapshot->DefinitionId = Definition.GetDefinitionId(); }
    return true;
}

bool UGamePlatformVFXWorldSubsystem::PlayDefinitionId(
    const FName DefinitionId,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ParentHandle)
{
    check(IsInGameThread());

    if (!InstanceRegistry.OwnsHandle(ParentHandle))
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World) || World->bIsTearingDown || bClosing)
    {
        return false;
    }

    const FGuid RootId = CompositeRootByInstance.FindRef(ParentHandle.Id);
    auto* Budget = CompositeRootBudgets.Find(RootId);
    const auto RootHandle = Budget ? Budget->Handle : ParentHandle;

    const UGamePlatformVFXSettings* Settings =
        GetDefault<UGamePlatformVFXSettings>();
    if (!FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
            Request,
            InstanceRegistry.Num(),
            *Settings))
    {
        FGamePlatformVFXDiagnostics::RequestRejected();
        CleanupInstance(RootHandle, true, EGamePlatformVFXPlaybackState::Failed,
            EGamePlatformVFXResultCode::RejectedByScalability, TEXT("Composite必需子请求被容量/伸缩限制拒绝，整树不能宣称已播放。"));
        return false;
    }

    if (!Budget || Budget->CreatedChildren >= Budget->MaxChildren || Request.CompositeDepth > Budget->MaxDepth)
    {
        CleanupInstance(RootHandle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::InvalidRequest,
            TEXT("Composite超过根定义批准的总后代数量或深度，已撤销整树。"));
        return false;
    }
    ++Budget->CreatedChildren;
    FGamePlatformVFXRequest ChildRequest = Request;
    ChildRequest.DefinitionId = DefinitionId;
    ChildRequest.bUseBaseNiagaraSystem = false;

    const FGamePlatformVFXHandle ChildHandle =
        InstanceRegistry.Reserve(World);
    CompositeRootByInstance.Add(ChildHandle.Id, RootId);
    FGamePlatformVFXPlaybackSnapshot Snapshot; Snapshot.Handle = ChildHandle; Snapshot.DefinitionId = DefinitionId;
    Snapshot.RequestId = ChildRequest.RequestId; Snapshot.ParentHandle = ParentHandle;
    Snapshot.State = EGamePlatformVFXPlaybackState::Loading; Snapshot.Code = EGamePlatformVFXResultCode::Queued;
    PlaybackSnapshots.Add(ChildHandle.Id, Snapshot); FallbackVisitedDefinitions.FindOrAdd(ChildHandle.Id).Add(DefinitionId);
    PeakTrackedInstances = FMath::Max(PeakTrackedInstances, InstanceRegistry.Num());
    UpdateRuntimeDiagnostics();

    if (!InstanceRegistry.AddChild(
            ParentHandle,
            ChildHandle))
    {
        CleanupInstance(ChildHandle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::InvalidRequest, TEXT("父实例在子请求登记前已经结束。"));
        return false;
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
        CleanupInstance(ChildHandle, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("复合子定义申请失败。"));
        return false;
    }
    return !bClosing && InstanceRegistry.OwnsHandle(ParentHandle);
}
