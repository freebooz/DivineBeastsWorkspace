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
    }
    LifetimeTimers.Reset();

    for (const TPair<FGuid, FGamePlatformDataLease>& Pair : PendingDefinitionLeases)
    {
        ReleaseLease(Pair.Value);
    }
    for (const TPair<FGuid, FGamePlatformDataLease>& Pair : ActiveDefinitionLeases)
    {
        ReleaseLease(Pair.Value);
    }
    for (const TPair<FGuid, FGamePlatformDataLease>& Pair : ExplicitPreloadLeases)
    {
        ReleaseLease(Pair.Value);
    }

    PendingDefinitionLeases.Reset();
    ActiveDefinitionLeases.Reset();
    ExplicitPreloadLeases.Reset();
    DedupeHandles.Reset();
    InstanceRegistry.Reset();
    CatalogRegistry.Reset();
    StartupCatalogHandles.Reset();
    RegisteredCatalogObjects.Reset();

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

FString UGamePlatformVFXWorldSubsystem::MakeDedupeKey(
    const FGamePlatformVFXRequest& Request) const
{
    // Presentation RequestId在预测/确认/纠正之间保持同一身份，优先用于幂等与纠正。
    if (Request.RequestId.IsValid())
    {
        return FString::Printf(
            TEXT("Request:%s"),
            *Request.RequestId.ToString(EGuidFormats::Digits));
    }
    if (Request.ActivationId.IsValid())
    {
        return FString::Printf(
            TEXT("Activation:%s:%lld"),
            *Request.ActivationId.ToString(EGuidFormats::Digits),
            Request.PredictionKey);
    }
    return FString();
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

bool UGamePlatformVFXWorldSubsystem::QueueDefinitionLoad(
    const FName DefinitionId,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ReservedHandle)
{
    check(IsInGameThread());

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    if (!Data)
    {
        return false;
    }

    FPrimaryAssetId DefinitionAssetId;
    if (!BuildDefinitionAssetId(DefinitionId, DefinitionAssetId))
    {
        return false;
    }

    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        DefinitionAssetId,
        UGamePlatformVFXDefinition::StaticClass(),
        { VFXRuntimeBundle },
        EGamePlatformDataLifetime::World,
        this,
        [WeakThis, ReservedHandle, Request](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& Result)
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                Self->HandleDefinitionLoaded(
                    ReservedHandle,
                    Request,
                    CompletedLease,
                    Result);
            }
        },
        Accepted);

    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        return false;
    }

    PendingDefinitionLeases.Add(ReservedHandle.Id, Lease);
    return true;
}

FGamePlatformVFXResult UGamePlatformVFXWorldSubsystem::Play(
    const FGamePlatformVFXRequest& Request)
{
    check(IsInGameThread());

    FGamePlatformVFXResult Result;
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown)
    {
        Result.Code = EGamePlatformVFXResultCode::InvalidWorld;
        return Result;
    }

    PruneDedupeHandles();
    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    const FString DedupeKey = MakeDedupeKey(Request);

    if (!DedupeKey.IsEmpty())
    {
        if (Request.PredictionState == EGamePlatformVFXPredictionState::Cancelled)
        {
            if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
            {
                Result.Handle = *Existing;
                Stop(*Existing);
            }
            Result.Code = EGamePlatformVFXResultCode::Cancelled;
            return Result;
        }

        if (const FGamePlatformVFXHandle* Existing = DedupeHandles.Find(DedupeKey))
        {
            if (Request.PredictionState == EGamePlatformVFXPredictionState::Corrected)
            {
                Stop(*Existing);
            }
            else if (InstanceRegistry.IsActive(*Existing))
            {
                Result.Handle = *Existing;
                Result.Code = EGamePlatformVFXResultCode::Played;
                return Result;
            }
            DedupeHandles.Remove(DedupeKey);
        }
    }

    if (!Request.IsStructurallyValid())
    {
        Result.Code = EGamePlatformVFXResultCode::InvalidRequest;
        return Result;
    }

    if (!DedupeKey.IsEmpty() &&
        !DedupeHandles.Contains(DedupeKey) &&
        DedupeHandles.Num() >= Settings->MaxDedupeEntries)
    {
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    const int32 TrackedInstances = InstanceRegistry.Num();
    if (!FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
            Request,
            TrackedInstances,
            *Settings))
    {
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    if (PendingDefinitionLeases.Num() >= Settings->MaxPendingInstancePreloads)
    {
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    bool bAmbiguous = false;
    const FName DefinitionId = ResolveDefinitionId(Request, bAmbiguous);
    if (bAmbiguous)
    {
        FGamePlatformVFXDiagnostics::CatalogAmbiguous(
            Request.SemanticTag,
            Request.ContextId);
        Result.Code = EGamePlatformVFXResultCode::CatalogAmbiguous;
        return Result;
    }
    if (DefinitionId.IsNone())
    {
        FGamePlatformVFXDiagnostics::CatalogMiss(
            Request.SemanticTag,
            Request.ContextId);
        Result.Code = Request.DefinitionId.IsNone()
            ? EGamePlatformVFXResultCode::CatalogMiss
            : EGamePlatformVFXResultCode::InvalidRequest;
        return Result;
    }

    FPrimaryAssetId DefinitionAssetId;
    if (!BuildDefinitionAssetId(DefinitionId, DefinitionAssetId))
    {
        Result.Code = EGamePlatformVFXResultCode::InvalidRequest;
        return Result;
    }

    FGamePlatformVFXRequest EffectiveRequest = Request;
    EffectiveRequest.DefinitionId = DefinitionId;

    Result.Handle = InstanceRegistry.Reserve(World, &EffectiveRequest);
    if (!DedupeKey.IsEmpty())
    {
        DedupeHandles.Add(DedupeKey, Result.Handle);
    }

    if (!QueueDefinitionLoad(DefinitionId, EffectiveRequest, Result.Handle))
    {
        CleanupInstance(Result.Handle, true);
        Result.Code = EGamePlatformVFXResultCode::DefinitionLoadFailed;
        return Result;
    }

    Result.Code = EGamePlatformVFXResultCode::Queued;
    return Result;
}

void UGamePlatformVFXWorldSubsystem::HandleDefinitionLoaded(
    const FGamePlatformVFXHandle ReservedHandle,
    FGamePlatformVFXRequest Request,
    const FGamePlatformDataLease Lease,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());

    const FGamePlatformDataLease* Pending =
        PendingDefinitionLeases.Find(ReservedHandle.Id);
    if (bClosing || !Pending || Pending->LeaseId != Lease.LeaseId)
    {
        return;
    }

    PendingDefinitionLeases.Remove(ReservedHandle.Id);

    if (!Result.IsSuccess() || !InstanceRegistry.IsActive(ReservedHandle))
    {
        ReleaseLease(Lease);
        CleanupInstance(ReservedHandle, true);
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    UGamePlatformVFXDefinition* Definition = Data
        ? const_cast<UGamePlatformVFXDefinition*>(
            Cast<UGamePlatformVFXDefinition>(
                Data->GetLoadedDefinition(Lease)))
        : nullptr;

    if (!IsValid(Definition) || !Definition->ValidateDefinition().IsSuccess())
    {
        FGamePlatformVFXDiagnostics::DefinitionLoadFailed(
            FSoftObjectPath(Lease.DefinitionId.ToString()));
        ReleaseLease(Lease);
        CleanupInstance(ReservedHandle, true);
        return;
    }

    ActiveDefinitionLeases.Add(ReservedHandle.Id, Lease);
    if (!ExecuteLoadedDefinition(*Definition, Request, ReservedHandle))
    {
        CleanupInstance(ReservedHandle, true);
    }
}

bool UGamePlatformVFXWorldSubsystem::Stop(
    const FGamePlatformVFXHandle& Handle)
{
    check(IsInGameThread());

    const bool bKnown =
        InstanceRegistry.IsActive(Handle) ||
        PendingDefinitionLeases.Contains(Handle.Id) ||
        ActiveDefinitionLeases.Contains(Handle.Id);
    if (!bKnown)
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
        // 自然结束后只回收平台记录/租约/去重状态，不再次Deactivate已结束组件。
        // 非池化组件按其AutoDestroy语义由Niagara处理，池化组件由Niagara原生池回收。
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

    FGamePlatformDataLease Lease;
    if (PendingDefinitionLeases.RemoveAndCopyValue(Handle.Id, Lease))
    {
        ReleaseLease(Lease);
    }
    if (ActiveDefinitionLeases.RemoveAndCopyValue(Handle.Id, Lease))
    {
        ReleaseLease(Lease);
    }

    for (auto It = DedupeHandles.CreateIterator(); It; ++It)
    {
        if (It.Value() == Handle)
        {
            It.RemoveCurrent();
        }
    }

    InstanceRegistry.Stop(Handle, bStopComponent);
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

void UGamePlatformVFXWorldSubsystem::PruneDedupeHandles()
{
    for (auto It = DedupeHandles.CreateIterator(); It; ++It)
    {
        if (!InstanceRegistry.IsActive(It.Value()))
        {
            It.RemoveCurrent();
        }
    }
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
    if (ExplicitPreloadLeases.Num() >= Settings->MaxPendingInstancePreloads)
    {
        return Handle;
    }

    bool bAmbiguous = false;
    const FName DefinitionId = ResolveDefinitionId(Request, bAmbiguous);
    FPrimaryAssetId DefinitionAssetId;
    if (bAmbiguous ||
        !BuildDefinitionAssetId(DefinitionId, DefinitionAssetId))
    {
        return Handle;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = GameInstance
        ? IGamePlatformDataService::Get(*GameInstance)
        : nullptr;
    if (!Data)
    {
        return Handle;
    }

    Handle.Id = FGuid::NewGuid();
    const FGamePlatformVFXPreloadHandle ExternalHandle = Handle;
    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        DefinitionAssetId,
        UGamePlatformVFXDefinition::StaticClass(),
        { VFXRuntimeBundle },
        EGamePlatformDataLifetime::World,
        this,
        [WeakThis, ExternalHandle](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& Result)
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                if (!Result.IsSuccess())
                {
                    FGamePlatformDataLease Stored;
                    if (Self->ExplicitPreloadLeases.RemoveAndCopyValue(
                            ExternalHandle.Id,
                            Stored))
                    {
                        Self->ReleaseLease(Stored);
                    }
                }
            }
        },
        Accepted);

    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        Handle.Id.Invalidate();
        return Handle;
    }

    ExplicitPreloadLeases.Add(Handle.Id, Lease);
    return Handle;
}

bool UGamePlatformVFXWorldSubsystem::CancelPreload(
    const FGamePlatformVFXPreloadHandle& Handle)
{
    check(IsInGameThread());

    FGamePlatformDataLease Lease;
    if (!Handle.IsValid() ||
        !ExplicitPreloadLeases.RemoveAndCopyValue(Handle.Id, Lease))
    {
        return false;
    }

    ReleaseLease(Lease);
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
    check(IsInGameThread());

    UWorld* World = GetWorld();
    if (!IsValid(World) ||
        !Definition.ValidateDefinition().IsSuccess())
    {
        return false;
    }

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
            *Settings) ||
        PendingDefinitionLeases.Num() >=
            Settings->MaxPendingInstancePreloads)
    {
        return;
    }

    FPrimaryAssetId DefinitionAssetId;
    if (!BuildDefinitionAssetId(
            DefinitionId,
            DefinitionAssetId))
    {
        return;
    }

    FGamePlatformVFXRequest ChildRequest = Request;
    ChildRequest.DefinitionId = DefinitionId;

    const FGamePlatformVFXHandle ChildHandle =
        InstanceRegistry.Reserve(World, &ChildRequest);
    if (!InstanceRegistry.AddChild(
            ParentHandle,
            ChildHandle))
    {
        InstanceRegistry.Stop(ChildHandle);
        return;
    }

    if (!QueueDefinitionLoad(
            DefinitionId,
            ChildRequest,
            ChildHandle))
    {
        CleanupInstance(ChildHandle, true);
    }
}
