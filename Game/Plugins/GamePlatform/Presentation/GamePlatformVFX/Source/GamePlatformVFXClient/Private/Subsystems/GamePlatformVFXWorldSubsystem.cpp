#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "Resolution/GamePlatformVFXResolver.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "NiagaraComponent.h"

namespace
{
bool IsDefinitionReadyForExecution(
    const UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request)
{
    if (Definition.GetBehavior() == EGamePlatformVFXBehavior::Composite)
    {
        return true;
    }

    const TSoftObjectPtr<UNiagaraSystem> NiagaraSystem = Definition.ResolveNiagaraSystem(
        Request.PlatformId,
        Request.QualityTier);
    return !NiagaraSystem.IsNull() && NiagaraSystem.IsValid();
}
}

bool UGamePlatformVFXWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) && World->GetNetMode() != NM_DedicatedServer;
}

void UGamePlatformVFXWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

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
    FGamePlatformAssetLoader::Cancel(StartupCatalogLoadLease);
    StartupCatalogLoadLease.Reset();
    PreloadCoordinator.Reset();
    PendingInstancePreloads.Reset();
    DedupeHandles.Reset();
    InstanceRegistry.Reset();
    CatalogRegistry.Reset();
    StartupCatalogHandles.Reset();
    RegisteredCatalogObjects.Reset();

    Super::Deinitialize();
}

void UGamePlatformVFXWorldSubsystem::HandleStartupCatalogsLoaded()
{
    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    for (const TSoftObjectPtr<UGamePlatformVFXCatalog>& CatalogRef : Settings->StartupCatalogs)
    {
        if (UGamePlatformVFXCatalog* Catalog = CatalogRef.Get())
        {
            StartupCatalogHandles.Add(RegisterCatalog(Catalog));
        }
    }
}

FGamePlatformVFXResult UGamePlatformVFXWorldSubsystem::Play(const FGamePlatformVFXRequest& Request)
{
    FGamePlatformVFXResult Result;
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        Result.Code = EGamePlatformVFXResultCode::InvalidWorld;
        return Result;
    }

    PruneDedupeHandles();
    PruneInstanceLeases();
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
            if (InstanceRegistry.IsActive(*Existing))
            {
                Result.Handle = *Existing;
                Result.Code = EGamePlatformVFXResultCode::Played;
                return Result;
            }
            DedupeHandles.Remove(DedupeKey);
        }
    }

    if (!DedupeKey.IsEmpty() &&
        !DedupeHandles.Contains(DedupeKey) &&
        DedupeHandles.Num() >= Settings->MaxDedupeEntries)
    {
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    if (!Request.IsStructurallyValid())
    {
        Result.Code = EGamePlatformVFXResultCode::InvalidRequest;
        return Result;
    }

    InstanceRegistry.Prune();
    if (!FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, InstanceRegistry.Num(), *Settings))
    {
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    const FGamePlatformVFXResolver Resolver(CatalogRegistry);
    const FGamePlatformVFXResolvedDefinition Resolved = Resolver.Resolve(Request);
    if (Resolved.bAmbiguous)
    {
        FGamePlatformVFXDiagnostics::CatalogAmbiguous(Request.SemanticTag, Request.ContextId);
        Result.Code = EGamePlatformVFXResultCode::CatalogAmbiguous;
        return Result;
    }

    if (!Resolved.IsValid())
    {
        FGamePlatformVFXDiagnostics::CatalogMiss(Request.SemanticTag, Request.ContextId);
        Result.Code = EGamePlatformVFXResultCode::CatalogMiss;
        return Result;
    }

    Result.Handle = InstanceRegistry.Reserve(World, &Request);
    if (!DedupeKey.IsEmpty())
    {
        DedupeHandles.Add(DedupeKey, Result.Handle);
    }

    if (UGamePlatformVFXDefinition* Loaded = Resolved.Definition.Get())
    {
        if (IsDefinitionReadyForExecution(*Loaded, Request))
        {
            Result.Code = ExecuteLoadedDefinition(*Loaded, Request, Result.Handle)
                ? EGamePlatformVFXResultCode::Played
                : EGamePlatformVFXResultCode::DefinitionLoadFailed;

            if (Result.Code == EGamePlatformVFXResultCode::DefinitionLoadFailed)
            {
                InstanceRegistry.Stop(Result.Handle);
            }
            return Result;
        }
    }

    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    const FGamePlatformVFXHandle ReservedHandle = Result.Handle;
    const FSoftObjectPath DefinitionPath = Resolved.Definition.ToSoftObjectPath();

    if (PendingInstancePreloads.Num() >= Settings->MaxPendingInstancePreloads)
    {
        InstanceRegistry.Stop(Result.Handle);
        Result.Code = EGamePlatformVFXResultCode::RejectedByScalability;
        return Result;
    }

    const FGamePlatformVFXPreloadHandle PreloadHandle = PreloadCoordinator.RequestDefinition(
        Resolved.Definition,
        [WeakThis, ReservedHandle, Request, DefinitionPath](UGamePlatformVFXDefinition* Definition)
        {
            UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get();
            if (!IsValid(Self) || !Self->InstanceRegistry.IsActive(ReservedHandle))
            {
                return;
            }

            if (!IsValid(Definition) || !Self->ExecuteLoadedDefinition(*Definition, Request, ReservedHandle))
            {
                FGamePlatformVFXDiagnostics::DefinitionLoadFailed(DefinitionPath);
                Self->InstanceRegistry.Stop(ReservedHandle);
            }
        },
        Request.PlatformId,
        Request.QualityTier);

    if (!PreloadHandle.IsValid())
    {
        InstanceRegistry.Stop(Result.Handle);
        Result.Code = EGamePlatformVFXResultCode::DefinitionLoadFailed;
        return Result;
    }

    PendingInstancePreloads.Add(Result.Handle.Id, PreloadHandle);
    InstanceRegistry.SetLoadLease(Result.Handle, PreloadHandle);
    Result.Code = EGamePlatformVFXResultCode::Queued;
    return Result;
}

bool UGamePlatformVFXWorldSubsystem::Stop(const FGamePlatformVFXHandle& Handle)
{
    if (const FGamePlatformVFXPreloadHandle* Pending = PendingInstancePreloads.Find(Handle.Id))
    {
        PreloadCoordinator.Cancel(*Pending);
        PendingInstancePreloads.Remove(Handle.Id);
    }

    for (auto It = DedupeHandles.CreateIterator(); It; ++It)
    {
        if (It.Value() == Handle)
        {
            It.RemoveCurrent();
        }
    }

    return InstanceRegistry.Stop(Handle);
}

bool UGamePlatformVFXWorldSubsystem::IsActive(const FGamePlatformVFXHandle& Handle) const
{
    return InstanceRegistry.IsActive(Handle);
}

FString UGamePlatformVFXWorldSubsystem::MakeDedupeKey(
    const FGamePlatformVFXRequest& Request) const
{
    if (Request.ActivationId.IsValid())
    {
        return FString::Printf(
            TEXT("Activation:%s:%lld"),
            *Request.ActivationId.ToString(EGuidFormats::Digits),
            Request.PredictionKey);
    }
    if (Request.RequestId.IsValid())
    {
        return FString::Printf(
            TEXT("Request:%s"),
            *Request.RequestId.ToString(EGuidFormats::Digits));
    }
    return FString();
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

void UGamePlatformVFXWorldSubsystem::PruneInstanceLeases()
{
    for (auto It = PendingInstancePreloads.CreateIterator(); It; ++It)
    {
        if (!InstanceRegistry.IsActiveId(It.Key()))
        {
            PreloadCoordinator.Cancel(It.Value());
            It.RemoveCurrent();
        }
    }
}

FGamePlatformVFXPreloadHandle UGamePlatformVFXWorldSubsystem::Preload(const FGamePlatformVFXRequest& Request)
{
    const FGamePlatformVFXResolver Resolver(CatalogRegistry);
    const FGamePlatformVFXResolvedDefinition Resolved = Resolver.Resolve(Request);
    if (!Resolved.IsValid())
    {
        return {};
    }

    return PreloadCoordinator.RequestDefinition(
        Resolved.Definition,
        [](UGamePlatformVFXDefinition*) {},
        Request.PlatformId,
        Request.QualityTier);
}

bool UGamePlatformVFXWorldSubsystem::CancelPreload(const FGamePlatformVFXPreloadHandle& Handle)
{
    return PreloadCoordinator.Cancel(Handle);
}

FGamePlatformVFXRegistrationHandle UGamePlatformVFXWorldSubsystem::RegisterCatalog(UGamePlatformVFXCatalog* Catalog)
{
    if (!IsValid(Catalog))
    {
        return {};
    }

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    if (CatalogRegistry.Num() >= Settings->MaxRegisteredCatalogs)
    {
        return {};
    }

    const FGamePlatformVFXRegistrationHandle Handle = CatalogRegistry.Register(Catalog);
    if (Handle.IsValid())
    {
        RegisteredCatalogObjects.Add(Handle.Id, Catalog);
    }
    return Handle;
}

bool UGamePlatformVFXWorldSubsystem::UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle)
{
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
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    FText ValidationReason;
    if (!Definition.ValidateDefinition(ValidationReason) ||
        !Definition.ValidateRequestParameters(Request.Parameters, ValidationReason))
    {
        return false;
    }

    if (!ReservedHandle.BelongsToWorld(World) ||
        !InstanceRegistry.SetDefinition(ReservedHandle, &Definition))
    {
        return false;
    }

    if (UGamePlatformVFXCompositeDefinition* Composite = Cast<UGamePlatformVFXCompositeDefinition>(&Definition))
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
                const TSoftObjectPtr<UGamePlatformVFXDefinition>& ChildDefinition,
                const FGamePlatformVFXRequest& ChildRequest,
                const FGamePlatformVFXHandle& ParentHandle)
            {
                if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
                {
                    Self->PlayDefinitionSoft(ChildDefinition, ChildRequest, ParentHandle);
                }
            });
        return true;
    }

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    const bool bUsePool = FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(Definition, Request, *Settings);
    UNiagaraComponent* Component = FGamePlatformVFXNiagaraExecutor::Spawn(*World, Definition, Request, bUsePool);
    return IsValid(Component) && InstanceRegistry.AttachComponent(ReservedHandle, Component, bUsePool);
}

void UGamePlatformVFXWorldSubsystem::PlayDefinitionSoft(
    const TSoftObjectPtr<UGamePlatformVFXDefinition>& Definition,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ParentHandle)
{
    if (!InstanceRegistry.IsActive(ParentHandle))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    const FGamePlatformVFXHandle ChildHandle = InstanceRegistry.Reserve(World, &Request);
    if (!InstanceRegistry.AddChild(ParentHandle, ChildHandle))
    {
        InstanceRegistry.Stop(ChildHandle);
        return;
    }

    if (UGamePlatformVFXDefinition* Loaded = Definition.Get())
    {
        if (IsDefinitionReadyForExecution(*Loaded, Request))
        {
            if (!ExecuteLoadedDefinition(*Loaded, Request, ChildHandle))
            {
                InstanceRegistry.Stop(ChildHandle);
            }
            return;
        }
    }

    const TWeakObjectPtr<UGamePlatformVFXWorldSubsystem> WeakThis(this);
    const FSoftObjectPath DefinitionPath = Definition.ToSoftObjectPath();
    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    if (PendingInstancePreloads.Num() >= Settings->MaxPendingInstancePreloads)
    {
        InstanceRegistry.Stop(ChildHandle);
        return;
    }

    const FGamePlatformVFXPreloadHandle PreloadHandle = PreloadCoordinator.RequestDefinition(
        Definition,
        [WeakThis, ChildHandle, Request, DefinitionPath](UGamePlatformVFXDefinition* LoadedDefinition)
        {
            if (UGamePlatformVFXWorldSubsystem* Self = WeakThis.Get())
            {
                if (!Self->InstanceRegistry.IsActive(ChildHandle))
                {
                    return;
                }

                if (!IsValid(LoadedDefinition) || !Self->ExecuteLoadedDefinition(*LoadedDefinition, Request, ChildHandle))
                {
                    FGamePlatformVFXDiagnostics::DefinitionLoadFailed(DefinitionPath);
                    Self->InstanceRegistry.Stop(ChildHandle);
                }
            }
        },
        Request.PlatformId,
        Request.QualityTier);

    if (PreloadHandle.IsValid())
    {
        PendingInstancePreloads.Add(ChildHandle.Id, PreloadHandle);
        InstanceRegistry.SetLoadLease(ChildHandle, PreloadHandle);
    }
    else
    {
        InstanceRegistry.Stop(ChildHandle);
    }
}
