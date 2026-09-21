#include "Subsystems/GamePlatformVFXWorldSubsystem.h"

#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Resolution/GamePlatformVFXResolver.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"

bool UGamePlatformVFXWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->GetNetMode() != NM_DedicatedServer;
}

bool UGamePlatformVFXWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::EditorPreview;
}

void UGamePlatformVFXWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ++WorldGeneration;
    if (WorldGeneration <= 0)
    {
        WorldGeneration = 1;
    }
}

void UGamePlatformVFXWorldSubsystem::Deinitialize()
{
    TArray<FGuid> InstanceIds;
    InstanceRegistry.GetAllIds(InstanceIds);
    for (const FGuid& Id : InstanceIds)
    {
        if (FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Id))
        {
            Stop(Record->Handle, true);
        }
    }

    PreloadCoordinator.Reset();
    CatalogRegistry.Reset();
    CompositeTimers.Reset();
    Super::Deinitialize();
}

FGamePlatformVFXPlayResult UGamePlatformVFXWorldSubsystem::Play(const FGamePlatformVFXRequest& Request)
{
    FGamePlatformVFXPlayResult Result;

    if (!GetWorld())
    {
        Result.Code = EGamePlatformVFXPlayResultCode::WorldUnavailable;
        Result.Message = TEXT("VFX World 不可用。");
        return Result;
    }

    if (!Request.IsStructurallyValid())
    {
        Result.Code = EGamePlatformVFXPlayResultCode::InvalidRequest;
        Result.Message = TEXT("请求必须提供 SemanticTag 或 ExplicitDefinitionId。");
        return Result;
    }

    const FGamePlatformVFXResolveResult ResolveResult = FGamePlatformVFXResolver::Resolve(Request, CatalogRegistry);
    if (!ResolveResult.bSuccess)
    {
        Result.Code = EGamePlatformVFXPlayResultCode::ResolveFailed;
        Result.Message = ResolveResult.Error;
        return Result;
    }

    const int32 AmbientCount = CountAmbientInstances();
    if (!FGamePlatformVFXScalabilityPolicy::CanSpawn(Request.Importance, InstanceRegistry.Num(), AmbientCount))
    {
        Result.Code = EGamePlatformVFXPlayResultCode::BudgetRejected;
        Result.Message = TEXT("VFX 请求被可伸缩策略拒绝。");
        return Result;
    }

    FGamePlatformVFXInstanceRecord Record;
    Record.Handle.InstanceId = FGuid::NewGuid();
    Record.Handle.Generation = WorldGeneration;
    Record.DefinitionId = ResolveResult.DefinitionId;
    Record.State = EGamePlatformVFXLifecycleState::Requested;
    Record.OriginalRequest = Request;

    FGamePlatformVFXInstanceRecord& StoredRecord = InstanceRegistry.Add(Record);
    BeginAsyncLoad(StoredRecord);

    Result.Code = EGamePlatformVFXPlayResultCode::Accepted;
    Result.Handle = StoredRecord.Handle;
    Result.Message = TEXT("VFX 请求已接受，进入异步加载流程。");
    return Result;
}

void UGamePlatformVFXWorldSubsystem::BeginAsyncLoad(FGamePlatformVFXInstanceRecord& Record)
{
    UAssetManager& AssetManager = UAssetManager::Get();
    const FSoftObjectPath DefinitionPath = AssetManager.GetPrimaryAssetPath(Record.DefinitionId);
    if (!DefinitionPath.IsValid())
    {
        FailInstance(Record.Handle.InstanceId, FString::Printf(TEXT("Definition 未被 AssetManager 扫描：%s"), *Record.DefinitionId.ToString()));
        return;
    }

    Record.State = EGamePlatformVFXLifecycleState::Loading;
    const FGuid InstanceId = Record.Handle.InstanceId;
    Record.DefinitionLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        DefinitionPath,
        FStreamableDelegate::CreateUObject(this, &UGamePlatformVFXWorldSubsystem::OnDefinitionLoaded, InstanceId));
}

void UGamePlatformVFXWorldSubsystem::OnDefinitionLoaded(FGuid InstanceId)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(InstanceId);
    if (!Record || Record->State == EGamePlatformVFXLifecycleState::Cancelled)
    {
        return;
    }

    UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(Record->DefinitionId);
    if (!LoadedObject)
    {
        LoadedObject = UAssetManager::Get().GetPrimaryAssetPath(Record->DefinitionId).ResolveObject();
    }

    UGamePlatformVFXDefinition* Definition = Cast<UGamePlatformVFXDefinition>(LoadedObject);
    if (!Definition)
    {
        FailInstance(InstanceId, TEXT("已加载主资产不是 UGamePlatformVFXDefinition。"));
        return;
    }

    if (Definition->Behavior == EGamePlatformVFXBehavior::Composite)
    {
        Record->State = EGamePlatformVFXLifecycleState::Active;
        FGamePlatformVFXCompositeRunner::Start(*this, Record->Handle, *CastChecked<UGamePlatformVFXCompositeDefinition>(Definition), Record->OriginalRequest);
        return;
    }

    if (Definition->NiagaraSystem.IsNull())
    {
        FailInstance(InstanceId, TEXT("Definition 未配置 NiagaraSystem。"));
        return;
    }

    if (Definition->NiagaraSystem.IsValid())
    {
        SpawnResolved(InstanceId, *Definition);
        return;
    }

    Record->NiagaraLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        Definition->NiagaraSystem.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UGamePlatformVFXWorldSubsystem::OnNiagaraLoaded, InstanceId));
}

void UGamePlatformVFXWorldSubsystem::OnNiagaraLoaded(FGuid InstanceId)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(InstanceId);
    if (!Record || Record->State == EGamePlatformVFXLifecycleState::Cancelled)
    {
        return;
    }

    UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetPath(Record->DefinitionId).ResolveObject();
    UGamePlatformVFXDefinition* Definition = Cast<UGamePlatformVFXDefinition>(LoadedObject);
    if (!Definition || !Definition->NiagaraSystem.IsValid())
    {
        FailInstance(InstanceId, TEXT("Niagara System 异步加载失败。"));
        return;
    }

    SpawnResolved(InstanceId, *Definition);
}

void UGamePlatformVFXWorldSubsystem::SpawnResolved(FGuid InstanceId, UGamePlatformVFXDefinition& Definition)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(InstanceId);
    if (!Record)
    {
        return;
    }

    Record->State = EGamePlatformVFXLifecycleState::Spawning;
    UNiagaraComponent* Component = FGamePlatformVFXNiagaraExecutor::Spawn(GetWorld(), Definition, Record->OriginalRequest);
    if (!Component)
    {
        FailInstance(InstanceId, TEXT("Niagara Component 创建失败。"));
        return;
    }

    Record->NiagaraComponent = Component;
    Record->State = EGamePlatformVFXLifecycleState::Active;
    Component->OnSystemFinished.AddDynamic(this, &UGamePlatformVFXWorldSubsystem::HandleNiagaraSystemFinished);
}

bool UGamePlatformVFXWorldSubsystem::Stop(const FGamePlatformVFXHandle& Handle, bool bImmediate)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Handle.InstanceId);
    if (!Record || Record->Handle.Generation != Handle.Generation)
    {
        return false;
    }

    Record->State = EGamePlatformVFXLifecycleState::Stopping;

    if (TArray<FTimerHandle>* Timers = CompositeTimers.Find(Handle.InstanceId))
    {
        if (UWorld* World = GetWorld())
        {
            for (const FTimerHandle& Timer : *Timers)
            {
                World->GetTimerManager().ClearTimer(Timer);
            }
        }
        CompositeTimers.Remove(Handle.InstanceId);
    }

    for (const FGamePlatformVFXHandle& Child : Record->ChildHandles)
    {
        Stop(Child, bImmediate);
    }

    if (Record->DefinitionLoadHandle) { Record->DefinitionLoadHandle->CancelHandle(); }
    if (Record->NiagaraLoadHandle) { Record->NiagaraLoadHandle->CancelHandle(); }

    if (UNiagaraComponent* Component = Record->NiagaraComponent.Get())
    {
        if (bImmediate)
        {
            Component->DeactivateImmediate();
        }
        else
        {
            Component->Deactivate();
        }
    }

    ReleaseRecordResources(*Record);
    InstanceRegistry.Remove(Handle.InstanceId);
    return true;
}

bool UGamePlatformVFXWorldSubsystem::UpdateParameters(const FGamePlatformVFXHandle& Handle, const FGamePlatformVFXParameters& Parameters)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Handle.InstanceId);
    if (!Record || Record->Handle.Generation != Handle.Generation)
    {
        return false;
    }

    if (UNiagaraComponent* Component = Record->NiagaraComponent.Get())
    {
        FGamePlatformVFXNiagaraExecutor::ApplyParameters(*Component, Parameters);
        return true;
    }
    return false;
}

EGamePlatformVFXLifecycleState UGamePlatformVFXWorldSubsystem::GetState(const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Handle.InstanceId);
    if (!Record || Record->Handle.Generation != Handle.Generation)
    {
        return EGamePlatformVFXLifecycleState::Invalid;
    }
    return Record->State;
}

FGamePlatformVFXRegistrationHandle UGamePlatformVFXWorldSubsystem::RegisterCatalog(UGamePlatformVFXCatalog* Catalog)
{
    return CatalogRegistry.Register(Catalog);
}

bool UGamePlatformVFXWorldSubsystem::UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle)
{
    return CatalogRegistry.Unregister(Handle);
}

FGamePlatformVFXPreloadHandle UGamePlatformVFXWorldSubsystem::Preload(const FPrimaryAssetId& DefinitionId)
{
    return PreloadCoordinator.Preload(DefinitionId);
}

bool UGamePlatformVFXWorldSubsystem::ReleasePreload(const FGamePlatformVFXPreloadHandle& Handle)
{
    return PreloadCoordinator.Release(Handle);
}

void UGamePlatformVFXWorldSubsystem::AttachChildHandle(const FGamePlatformVFXHandle& Parent, const FGamePlatformVFXHandle& Child)
{
    if (FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Parent.InstanceId))
    {
        if (Record->Handle.Generation == Parent.Generation)
        {
            Record->ChildHandles.Add(Child);
        }
    }
}

void UGamePlatformVFXWorldSubsystem::AttachCompositeTimer(const FGamePlatformVFXHandle& Parent, const FTimerHandle& TimerHandle)
{
    CompositeTimers.FindOrAdd(Parent.InstanceId).Add(TimerHandle);
}

void UGamePlatformVFXWorldSubsystem::CompleteComposite(const FGamePlatformVFXHandle& Parent)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Parent.InstanceId);
    if (!Record || Record->Handle.Generation != Parent.Generation)
    {
        return;
    }

    Record->State = EGamePlatformVFXLifecycleState::Completed;
    ReleaseRecordResources(*Record);
    CompositeTimers.Remove(Parent.InstanceId);
    InstanceRegistry.Remove(Parent.InstanceId);
}

void UGamePlatformVFXWorldSubsystem::HandleNiagaraSystemFinished(UNiagaraComponent* FinishedComponent)
{
    FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.FindByComponent(FinishedComponent);
    if (!Record)
    {
        return;
    }

    const FGuid InstanceId = Record->Handle.InstanceId;
    Record->State = EGamePlatformVFXLifecycleState::Completed;
    ReleaseRecordResources(*Record);
    InstanceRegistry.Remove(InstanceId);
}

void UGamePlatformVFXWorldSubsystem::FailInstance(FGuid InstanceId, const FString& Reason)
{
    if (FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(InstanceId))
    {
        Record->State = EGamePlatformVFXLifecycleState::Failed;
        FGamePlatformVFXDiagnostics::Warning(Reason);
        ReleaseRecordResources(*Record);
        InstanceRegistry.Remove(InstanceId);
    }
}

void UGamePlatformVFXWorldSubsystem::ReleaseRecordResources(FGamePlatformVFXInstanceRecord& Record)
{
    if (Record.NiagaraLoadHandle) { Record.NiagaraLoadHandle->ReleaseHandle(); Record.NiagaraLoadHandle.Reset(); }
    if (Record.DefinitionLoadHandle) { Record.DefinitionLoadHandle->ReleaseHandle(); Record.DefinitionLoadHandle.Reset(); }
    Record.NiagaraComponent.Reset();
}

int32 UGamePlatformVFXWorldSubsystem::CountAmbientInstances() const
{
    // 当前版本没有把最终 Definition 的 DefaultImportance 缓存在实例记录中，
    // 因此只统计请求级 Ambient。后续可由实例记录直接缓存最终重要等级。
    int32 Count = 0;
    TArray<FGuid> Ids;
    InstanceRegistry.GetAllIds(Ids);
    for (const FGuid& Id : Ids)
    {
        if (const FGamePlatformVFXInstanceRecord* Record = InstanceRegistry.Find(Id))
        {
            if (Record->OriginalRequest.Importance == EGamePlatformVFXImportance::Ambient)
            {
                ++Count;
            }
        }
    }
    return Count;
}
