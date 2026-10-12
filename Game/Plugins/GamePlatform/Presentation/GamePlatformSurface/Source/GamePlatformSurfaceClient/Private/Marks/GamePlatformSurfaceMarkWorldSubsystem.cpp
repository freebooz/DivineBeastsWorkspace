// 客户端局部命中贴花：资源从Data租约异步获取，缓存有界，世界退出主动销毁。
#include "Marks/GamePlatformSurfaceMarkWorldSubsystem.h"
#include "Marks/GamePlatformSurfaceMarkCatalog.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSurfaceMark, Log, All);

bool UGamePlatformSurfaceMarkWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World = Cast<UWorld>(Outer);
    if (!IsValid(World) || IsRunningCommandlet()) return false;
    const bool bClientWorld = World->WorldType == EWorldType::Game ||
        World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::GamePreview;
    return bClientWorld && World->GetNetMode() != NM_DedicatedServer;
}

void UGamePlatformSurfaceMarkWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    bLoadAttempted = false;
    bReady = false;
    LoadGeneration = 0;
    ActiveMarks.Reset();
    PendingRequests.Reset();
    RecentEventIds.Reset();
}

void UGamePlatformSurfaceMarkWorldSubsystem::Deinitialize()
{
    bClosing = true;
    ++LoadGeneration;    MarkSpawned.Clear();
    PendingRequests.Reset();
    for (const auto& Mark : ActiveMarks)
    {
        if (UDecalComponent* Decal = Mark.Component.Get()) Decal->DestroyComponent();
    }
    ActiveMarks.Reset();
    RecentEventIds.Reset();
    ReleaseAssetLeases();
    Super::Deinitialize();
}

IGamePlatformDataService* UGamePlatformSurfaceMarkWorldSubsystem::GetDataService() const
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = IsValid(World) ? World->GetGameInstance() : nullptr;
    return IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UGamePlatformSurfaceMarkWorldSubsystem::ReleaseAssetLeases()
{
    ++LoadGeneration; // 先废弃旧请求代次，再撤销自己持有的句柄。
    const auto OldCatalog = CatalogLease;
    const auto OldMaterials = MaterialsLease;
    CatalogLease = {};
    MaterialsLease = {};
    LoadedCatalog = nullptr;
    bReady = false;
    if (auto* Service = GetDataService())
    {
        if (OldMaterials.IsValid()) Service->ReleaseResources(OldMaterials);
        if (OldCatalog.IsValid()) Service->ReleaseResources(OldCatalog);
    }
}

bool UGamePlatformSurfaceMarkWorldSubsystem::RefreshCatalog()
{
    check(IsInGameThread());
    if (bClosing) return false;
    PendingRequests.Reset();
    ReleaseAssetLeases();
    bLoadAttempted = false;
    RecentEventIds.Reset();
    return BeginAssetLoad();
}

bool UGamePlatformSurfaceMarkWorldSubsystem::BeginAssetLoad()
{
    if (bClosing || bLoadAttempted) return false;
    const auto* Settings = GetDefault<UGamePlatformSurfaceSettings>();
    const FSoftObjectPath CatalogPath = Settings ? Settings->MarkCatalog.ToSoftObjectPath() : FSoftObjectPath();
    auto* Service = GetDataService();
    if (!Settings || !Settings->bEnableSurfaceMarks || !CatalogPath.IsValid() || !Service) return false;
    bLoadAttempted = true;
    const int64 Current = ++LoadGeneration;
    const TWeakObjectPtr<UGamePlatformSurfaceMarkWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    CatalogLease = Service->AcquireResources({ CatalogPath }, EGamePlatformDataLifetime::World, this,
        [WeakThis, Current](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->OnCatalogLoaded(Current, Lease, Result);
        }, Accepted);
    if (!Accepted.IsSuccess() || !CatalogLease.IsValid())
    {
        bReady = false;
        PendingRequests.Reset();
        if (CatalogLease.IsValid()) Service->ReleaseResources(CatalogLease);
        CatalogLease = {};
        return false;
    }
    return true;
}

void UGamePlatformSurfaceMarkWorldSubsystem::OnCatalogLoaded(
    int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bClosing || Generation != LoadGeneration || !Lease.IsValid() ||
        Lease.LeaseId != CatalogLease.LeaseId || Lease.ScopeId != CatalogLease.ScopeId ||
        Lease.Generation != CatalogLease.Generation || Lease.IssuerProof != CatalogLease.IssuerProof ||
        Lease.ResourcePaths != CatalogLease.ResourcePaths) return;
    auto* Service = GetDataService();
    if (!Result.IsSuccess() || !Service ||
        Service->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded ||
        Lease.ResourcePaths.Num() != 1)
    {
        PendingRequests.Reset();
        ReleaseAssetLeases();
        bLoadAttempted = true; // 不在每次命中后重新失败加载；需显式Refresh。
        return;
    }
    auto* Catalog = Cast<UGamePlatformSurfaceMarkCatalog>(Lease.ResourcePaths[0].ResolveObject());
    FString Error;
    if (!IsValid(Catalog) || !Catalog->IsValidCatalog(Error))
    {
        UE_LOG(LogGamePlatformSurfaceMark, Warning, TEXT("痕迹目录不可用：%s"), *Error);
        PendingRequests.Reset();
        ReleaseAssetLeases();
        bLoadAttempted = true;
        return;
    }
    LoadedCatalog = Catalog;
    TArray<FSoftObjectPath> MaterialPaths;
    for (const auto& Style : Catalog->Styles)
        MaterialPaths.AddUnique(Style.Material.ToSoftObjectPath());
    FGamePlatformResult Accepted;
    const TWeakObjectPtr<UGamePlatformSurfaceMarkWorldSubsystem> WeakThis(this);
    MaterialsLease = Service->AcquireResources(MaterialPaths, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const FGamePlatformDataLease& MaterialLease, const FGamePlatformResult& MaterialResult)
        {
            if (auto* Self = WeakThis.Get()) Self->OnMaterialsLoaded(Generation, MaterialLease, MaterialResult);
        }, Accepted);
    if (!Accepted.IsSuccess() || !MaterialsLease.IsValid())
    {
        PendingRequests.Reset();
        ReleaseAssetLeases();
        bLoadAttempted = true;
    }
}

void UGamePlatformSurfaceMarkWorldSubsystem::OnMaterialsLoaded(
    int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bClosing || Generation != LoadGeneration || !Lease.IsValid() ||
        Lease.LeaseId != MaterialsLease.LeaseId || Lease.ScopeId != MaterialsLease.ScopeId ||
        Lease.Generation != MaterialsLease.Generation || Lease.IssuerProof != MaterialsLease.IssuerProof ||
        Lease.ResourcePaths != MaterialsLease.ResourcePaths) return;
    auto* Service = GetDataService();
    if (!Result.IsSuccess() || !Service ||
        Service->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded ||
        !IsValid(LoadedCatalog))
    {
        PendingRequests.Reset();
        ReleaseAssetLeases();
        bLoadAttempted = true;
        return;
    }
    for (const auto& Style : LoadedCatalog->Styles)
    {
        UMaterialInterface* Material = Cast<UMaterialInterface>(Style.Material.ToSoftObjectPath().ResolveObject());
        if (!IsValid(Material) || !Material->GetMaterial() ||
            Material->GetMaterial()->MaterialDomain != MD_DeferredDecal)
        {
            UE_LOG(LogGamePlatformSurfaceMark, Warning,
                TEXT("贴花材质未加载或非Deferred Decal：%s"), *Style.Material.ToString());
            PendingRequests.Reset();
            ReleaseAssetLeases();
            bLoadAttempted = true;
            return;
        }
    }
    bReady = true;
    auto Requests = MoveTemp(PendingRequests);
    PendingRequests.Reset();
    for (const auto& Request : Requests)
    {
        if (bClosing) break;
        if (FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request))
            SpawnLoadedMark(Request);
    }
}

void UGamePlatformSurfaceMarkWorldSubsystem::PruneActive()
{
    const UWorld* World = GetWorld();
    const double Now = IsValid(World) ? World->GetTimeSeconds() : 0.0;
    ActiveMarks.RemoveAll([Now](const FActiveMark& Mark)
    {
        return !Mark.Component.IsValid() || (Mark.ExpireTimeSeconds > 0.0 && Now >= Mark.ExpireTimeSeconds);
    });
}

bool UGamePlatformSurfaceMarkWorldSubsystem::AlreadySeenEvent(const FGuid& EventId) const
{
    if (!EventId.IsValid()) return false;
    if (RecentEventIds.Contains(EventId)) return true;
    for (const auto& Queued : PendingRequests)
        if (Queued.EventId == EventId) return true;
    return false;
}

void UGamePlatformSurfaceMarkWorldSubsystem::RememberEvent(const FGuid& EventId)
{
    if (!EventId.IsValid() || !LoadedCatalog) return;
    RecentEventIds.Add(EventId);
    const int32 Extra = RecentEventIds.Num() - LoadedCatalog->MaxRecentEventIds;
    if (Extra > 0) RecentEventIds.RemoveAt(0, Extra, EAllowShrinking::No);
}

EGamePlatformSurfaceMarkStatus UGamePlatformSurfaceMarkWorldSubsystem::SpawnLoadedMark(
    const FGamePlatformSurfaceMarkRequest& Request)
{
    UWorld* World = GetWorld();
    if (!bReady || !IsValid(World) || World->bIsTearingDown || !LoadedCatalog)
        return EGamePlatformSurfaceMarkStatus::InvalidWorld;
    if (Request.HitComponent && (!IsValid(Request.HitComponent) || Request.HitComponent->GetWorld() != World))
        return EGamePlatformSurfaceMarkStatus::InvalidRequest;
    const auto* Style = LoadedCatalog->FindStyle(Request.Kind, Request.SurfaceType.GetValue());
    if (!Style) return EGamePlatformSurfaceMarkStatus::MissingAssets;
    auto* Material = Cast<UMaterialInterface>(Style->Material.ToSoftObjectPath().ResolveObject());
    if (!IsValid(Material)) return EGamePlatformSurfaceMarkStatus::MissingAssets;

    PruneActive();
    while (ActiveMarks.Num() >= LoadedCatalog->MaxActiveMarks)
    {
        if (UDecalComponent* Old = ActiveMarks[0].Component.Get()) Old->DestroyComponent();
        ActiveMarks.RemoveAt(0, 1, EAllowShrinking::No);
    }
    const FVector Normal = Request.Normal.GetSafeNormal();
    FVector Tangent = FVector::VectorPlaneProject(Request.Direction, Normal).GetSafeNormal();
    if (Tangent.IsNearlyZero())
    {
        FVector Secondary;
        Normal.FindBestAxisVectors(Tangent, Secondary);
    }
    const FRotator Rotation = FRotationMatrix::MakeFromXZ(Normal, Tangent).Rotator();
    const FVector Position = Request.Location + Normal * Style->NormalOffsetCm;
    const FVector Extent = Style->SizeCm * Request.SizeScale;
    UDecalComponent* Decal = nullptr;
    if (Style->bAttachToHitComponent && IsValid(Request.HitComponent))
    {
        Decal = UGameplayStatics::SpawnDecalAttached(Material, Extent, Request.HitComponent,
            NAME_None, Position, Rotation, EAttachLocation::KeepWorldPosition, Style->LifetimeSeconds);
    }
    else
    {
        Decal = UGameplayStatics::SpawnDecalAtLocation(World, Material, Extent,
            Position, Rotation, Style->LifetimeSeconds);
    }
    if (!IsValid(Decal)) return EGamePlatformSurfaceMarkStatus::MissingAssets;
    if (Style->LifetimeSeconds > 0.0f && Style->FadeSeconds > 0.0f)
        Decal->SetFadeOut(FMath::Max(0.0f, Style->LifetimeSeconds - Style->FadeSeconds),
            FMath::Min(Style->LifetimeSeconds, Style->FadeSeconds), false);

    FActiveMark Active;
    Active.Component = Decal;
    Active.ExpireTimeSeconds = Style->LifetimeSeconds > 0.0f
        ? World->GetTimeSeconds() + Style->LifetimeSeconds : 0.0;
    ActiveMarks.Add(Active);
    RememberEvent(Request.EventId);    // 仅真实生成后通知现有Niagara/音效桥，外部失败不能改变命中权威。
    MarkSpawned.Broadcast(Request);
    return EGamePlatformSurfaceMarkStatus::Spawned;
}

FGamePlatformSurfaceMarkResult UGamePlatformSurfaceMarkWorldSubsystem::MakeResult(
    EGamePlatformSurfaceMarkStatus Status) const
{
    FGamePlatformSurfaceMarkResult Result;
    Result.Status = Status;
    Result.ActiveCount = ActiveMarks.Num();
    Result.PendingCount = PendingRequests.Num();
    return Result;
}

FGamePlatformSurfaceMarkResult UGamePlatformSurfaceMarkWorldSubsystem::Submit(
    const FGamePlatformSurfaceMarkRequest& Request)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown ||
        World->GetNetMode() == NM_DedicatedServer)
        return MakeResult(EGamePlatformSurfaceMarkStatus::InvalidWorld);
    if (!GetDefault<UGamePlatformSurfaceSettings>()->bEnableSurfaceMarks)
        return MakeResult(EGamePlatformSurfaceMarkStatus::Disabled);
    if (!FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request) ||
        (Request.HitComponent &&
            (!IsValid(Request.HitComponent) || Request.HitComponent->GetWorld() != World)))
        return MakeResult(EGamePlatformSurfaceMarkStatus::InvalidRequest);
    if (AlreadySeenEvent(Request.EventId)) return MakeResult(EGamePlatformSurfaceMarkStatus::Duplicate);
    if (bReady)
        return MakeResult(SpawnLoadedMark(Request));
    if (bLoadAttempted && !CatalogLease.IsValid())
        return MakeResult(EGamePlatformSurfaceMarkStatus::MissingAssets);
    if (PendingRequests.Num() >= 32)
        return MakeResult(EGamePlatformSurfaceMarkStatus::BudgetExceeded);
    PendingRequests.Add(Request);
    if (!bLoadAttempted && !BeginAssetLoad())
    {
        PendingRequests.Pop();
        return MakeResult(EGamePlatformSurfaceMarkStatus::MissingAssets);
    }
    // 加载后还需按Catalog的实际队列预算裁剪，默认固定硬上限不允许客户端无限堆积。
    return MakeResult(EGamePlatformSurfaceMarkStatus::Pending);
}

FDelegateHandle UGamePlatformSurfaceMarkWorldSubsystem::AddSpawnedHandler(
    const FGamePlatformSurfaceMarkSpawned::FDelegate& Handler)
{
    check(IsInGameThread());
    return !bClosing ? MarkSpawned.Add(Handler) : FDelegateHandle();
}

void UGamePlatformSurfaceMarkWorldSubsystem::RemoveSpawnedHandler(FDelegateHandle Handle)
{
    check(IsInGameThread());
    MarkSpawned.Remove(Handle);
}

void UGamePlatformSurfaceMarkWorldSubsystem::GetCounts(int32& OutActive, int32& OutPending) const
{
    OutActive = ActiveMarks.Num();
    OutPending = PendingRequests.Num();
}
