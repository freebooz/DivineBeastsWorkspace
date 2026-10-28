// 客户端按名称替换已烘焙的地形草图类型；服务器只持有不含网格的中性草图身份。
#include "World/DivineBeastsVillageGrassWorldSubsystem.h"
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeGrassType.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAVillageGrass, Log, All);

namespace
{
    const FName GrassName(TEXT("VillageGrass"));
    const FName FlowerName(TEXT("VillageFlowers"));
    const FSoftObjectPath GrassPath(TEXT("/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld/LGT_DBA_OpenWorldGrass.LGT_DBA_OpenWorldGrass"));
    const FSoftObjectPath FlowerPath(TEXT("/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld/LGT_DBA_OpenWorldFlowers.LGT_DBA_OpenWorldFlowers"));
}

bool UDivineBeastsVillageGrassWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
        && World->GetNetMode() != NM_DedicatedServer && !IsRunningCommandlet();
}

void UDivineBeastsVillageGrassWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    bActive = UDivineBeastsVillageSnowWorldSubsystem::IsVillageWorldPackage(InWorld.GetOutermost()->GetName());
    if (!bActive || InWorld.GetNetMode() == NM_DedicatedServer) return;
    ActorHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::OnActorSpawned));
    LevelHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::OnLevelAdded);
    for (TActorIterator<ALandscapeProxy> It(&InWorld); It; ++It) TryRequestFor(*It);
}

void UDivineBeastsVillageGrassWorldSubsystem::TryRequestFor(ALandscapeProxy* Landscape)
{
    UWorld* World = GetWorld();
    if (!bActive || bRequestAttempted || !IsValid(World) || !IsValid(Landscape) || Landscape->GetWorld() != World) return;
    bool bHasOutputs = false;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
        if (IsValid(Component) && Component->GetNamedGrassTypes().Contains(GrassName) &&
            Component->GetNamedGrassTypes().Contains(FlowerName)) { bHasOutputs = true; break; }
    if (!bHasOutputs) return;
    bRequestAttempted = true;
    UGameInstance* Instance = World->GetGameInstance();
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data)
    {
        UE_LOG(LogDBAVillageGrass, Warning, TEXT("Village草地未找到Data服务，保留中性地形。"));
        return;
    }
    const int64 Generation = ++RequestGeneration;
    TWeakObjectPtr<UDivineBeastsVillageGrassWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    Lease = Data->AcquireResources({GrassPath, FlowerPath}, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->OnReady(Generation, Completed, Result);
        }, Accepted);
    if (!Accepted.IsSuccess() || !Lease.IsValid())
        UE_LOG(LogDBAVillageGrass, Warning, TEXT("Village草地资源租约未接纳。"));
}

void UDivineBeastsVillageGrassWorldSubsystem::OnReady(int64 Generation,
    const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (!bActive || !IsValid(World) || World->bIsTearingDown || Generation != RequestGeneration ||
        !Lease.IsValid() || Completed.ScopeId != Lease.ScopeId || Completed.LeaseId != Lease.LeaseId ||
        Completed.Generation != Lease.Generation || Completed.IssuerProof != Lease.IssuerProof ||
        Completed.ResourcePaths != Lease.ResourcePaths) return;
    UGameInstance* Instance = World->GetGameInstance();
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    auto* Grass = Cast<ULandscapeGrassType>(GrassPath.ResolveObject());
    auto* Flowers = Cast<ULandscapeGrassType>(FlowerPath.ResolveObject());
    if (!Data || !Result.IsSuccess() || Data->GetLeaseState(Completed) != EGamePlatformDataRequestState::Succeeded ||
        !IsValid(Grass) || !IsValid(Flowers))
    {
        UE_LOG(LogDBAVillageGrass, Warning, TEXT("Village草地资源加载失败，保留中性地形。"));
        if (Data) Data->ReleaseResources(Completed);
        Lease = {};
        ++RequestGeneration;
        return;
    }
    GrassType = Grass;
    FlowerType = Flowers;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It) Bind(*It);
}

void UDivineBeastsVillageGrassWorldSubsystem::OnActorSpawned(AActor* Actor)
{
    if (auto* Landscape = Cast<ALandscapeProxy>(Actor)) { TryRequestFor(Landscape); Bind(Landscape); }
}

void UDivineBeastsVillageGrassWorldSubsystem::OnLevelAdded(ULevel* Level, UWorld* World)
{
    if (!bActive || World != GetWorld() || !IsValid(Level)) return;
    for (AActor* Actor : Level->Actors) if (auto* Landscape = Cast<ALandscapeProxy>(Actor))
    { TryRequestFor(Landscape); Bind(Landscape); }
}

void UDivineBeastsVillageGrassWorldSubsystem::Bind(ALandscapeProxy* Landscape)
{
    UWorld* World = GetWorld();
    if (!bActive || !IsValid(World) || !IsValid(Landscape) || Landscape->GetWorld() != World ||
        !GrassType.IsValid() || !FlowerType.IsValid() || Bound.ContainsByPredicate(
            [Landscape](const FPreviousOverride& Item) { return Item.Landscape.Get() == Landscape; })) return;
    bool bNamesPresent = false;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (IsValid(Component) && Component->GetNamedGrassTypes().Contains(GrassName) &&
            Component->GetNamedGrassTypes().Contains(FlowerName)) { bNamesPresent = true; break; }
    }
    if (!bNamesPresent) return;
    const FLandscapeGrassName GrassKey(GrassName), FlowerKey(FlowerName);
    FPreviousOverride Previous;
    Previous.Landscape = Landscape;
    if (auto* Value = Landscape->GrassTypesOverrides.Find(GrassKey)) { Previous.bHadGrass = true; Previous.Grass = Value->Get(); }
    if (auto* Value = Landscape->GrassTypesOverrides.Find(FlowerKey)) { Previous.bHadFlowers = true; Previous.Flowers = Value->Get(); }
    Landscape->GrassTypesOverrides.Add(GrassKey, GrassType.Get());
    Landscape->GrassTypesOverrides.Add(FlowerKey, FlowerType.Get());
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents) if (IsValid(Component)) Component->UpdateGrassTypes(true);
    Landscape->FlushGrassComponents(nullptr, false); // 保留已烘焙的权重图。
    Bound.Add(Previous);
    UE_LOG(LogDBAVillageGrass, Display, TEXT("VILLAGE_GRASS_BOUND %s"), *Landscape->GetPathName());
}

void UDivineBeastsVillageGrassWorldSubsystem::UnbindAll()
{
    for (const FPreviousOverride& Previous : Bound)
    {
        ALandscapeProxy* Landscape = Previous.Landscape.Get();
        if (!IsValid(Landscape)) continue;
        const FLandscapeGrassName GrassKey(GrassName), FlowerKey(FlowerName);
        auto Restore = [Landscape](const FLandscapeGrassName& Key, ULandscapeGrassType* Ours,
            bool bHad, ULandscapeGrassType* Original)
        {
            const auto* Current = Landscape->GrassTypesOverrides.Find(Key);
            if (!Current || Current->Get() != Ours) return;
            if (bHad) Landscape->GrassTypesOverrides.Add(Key, Original);
            else Landscape->GrassTypesOverrides.Remove(Key);
        };
        Restore(GrassKey, GrassType.Get(), Previous.bHadGrass, Previous.Grass.Get());
        Restore(FlowerKey, FlowerType.Get(), Previous.bHadFlowers, Previous.Flowers.Get());
        for (ULandscapeComponent* Component : Landscape->LandscapeComponents) if (IsValid(Component)) Component->UpdateGrassTypes(true);
        Landscape->FlushGrassComponents(nullptr, false);
    }
    Bound.Reset();
}

void UDivineBeastsVillageGrassWorldSubsystem::Deinitialize()
{
    bActive = false;
    ++RequestGeneration;
    if (UWorld* World = GetWorld(); World && ActorHandle.IsValid()) World->RemoveOnActorSpawnedHandler(ActorHandle);
    if (LevelHandle.IsValid()) FWorldDelegates::LevelAddedToWorld.Remove(LevelHandle);
    UnbindAll();
    const FGamePlatformDataLease OldLease = Lease;
    Lease = {};
    GrassType.Reset(); FlowerType.Reset();
    if (UWorld* World = GetWorld())
        if (UGameInstance* Instance = World->GetGameInstance())
            if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*Instance); Data && OldLease.IsValid())
                Data->ReleaseResources(OldLease);
    Super::Deinitialize();
}
