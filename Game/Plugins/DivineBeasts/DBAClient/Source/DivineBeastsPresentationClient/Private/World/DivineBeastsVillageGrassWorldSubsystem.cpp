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
#include "GameFramework/Character.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAVillageGrass, Log, All);

namespace
{
    const FName GrassName(TEXT("VillageGrass"));
    const FName FlowerName(TEXT("VillageFlowers"));
    const FSoftObjectPath GrassPath(TEXT("/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld/LGT_DBA_OpenWorldGrass.LGT_DBA_OpenWorldGrass"));
    const FSoftObjectPath FlowerPath(TEXT("/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld/LGT_DBA_OpenWorldFlowers.LGT_DBA_OpenWorldFlowers"));
    const FSoftObjectPath ContactPath(TEXT("/Game/PN_OpenWorldFoliage/Materials/MaterialCollection.MaterialCollection"));
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
    Lease = Data->AcquireResources({GrassPath, FlowerPath, ContactPath}, EGamePlatformDataLifetime::World, this,
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
    auto* Collection = Cast<UMaterialParameterCollection>(ContactPath.ResolveObject());
    if (!Data || !Result.IsSuccess() || Data->GetLeaseState(Completed) != EGamePlatformDataRequestState::Succeeded ||
        !IsValid(Grass) || !IsValid(Flowers) || !IsValid(Collection))
    {
        UE_LOG(LogDBAVillageGrass, Warning, TEXT("Village草地资源加载失败，保留中性地形。"));
        if (Data) Data->ReleaseResources(Completed);
        Lease = {};
        ++RequestGeneration;
        return;
    }
    GrassType = Grass;
    FlowerType = Flowers;
    ContactCollection = Collection;
    for (int32 Slot = 0; Slot < 4; ++Slot) ClearContact(Slot);
    // 本地玩家优先于NPC占用有限交互槽，余下角色按首次出现顺序登记。
    for (TActorIterator<ACharacter> It(World); It; ++It) if (It->IsLocallyControlled()) AttachCharacter(*It);
    for (TActorIterator<ACharacter> It(World); It; ++It) if (!It->IsLocallyControlled()) AttachCharacter(*It);
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It) Bind(*It);
}

void UDivineBeastsVillageGrassWorldSubsystem::OnActorSpawned(AActor* Actor)
{
    if (auto* Landscape = Cast<ALandscapeProxy>(Actor)) { TryRequestFor(Landscape); Bind(Landscape); }
    if (auto* Character = Cast<ACharacter>(Actor)) AttachCharacter(Character);
}

void UDivineBeastsVillageGrassWorldSubsystem::OnLevelAdded(ULevel* Level, UWorld* World)
{
    if (!bActive || World != GetWorld() || !IsValid(Level)) return;
    for (AActor* Actor : Level->Actors) if (auto* Landscape = Cast<ALandscapeProxy>(Actor))
    { TryRequestFor(Landscape); Bind(Landscape); }
    for (AActor* Actor : Level->Actors) if (auto* Character = Cast<ACharacter>(Actor)) AttachCharacter(Character);
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
    if (auto* Value = Landscape->GrassTypesOverrides.Find(GrassKey)) { Previous.bHadGrass = true; Previous.GrassIndex = RetainedOriginalTypes.Add(Value->Get()); }
    if (auto* Value = Landscape->GrassTypesOverrides.Find(FlowerKey)) { Previous.bHadFlowers = true; Previous.FlowersIndex = RetainedOriginalTypes.Add(Value->Get()); }
    Landscape->GrassTypesOverrides.Add(GrassKey, GrassType.Get());
    Landscape->GrassTypesOverrides.Add(FlowerKey, FlowerType.Get());
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (!IsValid(Component)) continue;
        auto Types = Component->GetNamedGrassTypes();
        auto* OldGrass = Types.Find(GrassName);
        auto* OldFlowers = Types.Find(FlowerName);
        if (!OldGrass || !OldFlowers) continue;
        FPreviousOverride::FComponentTypes Old;
        Old.Component = Component;
        Old.GrassIndex = RetainedOriginalTypes.Add(OldGrass->Get());
        Old.FlowersIndex = RetainedOriginalTypes.Add(OldFlowers->Get());
        Types[GrassName] = GrassType.Get();
        Types[FlowerName] = FlowerType.Get();
        Component->SetNamedGrassTypes(Types); // Cook后材质不再提供可重建的草种表。
        Previous.Components.Add(Old);
    }
    // 空中性草种不会构建权重；客户端替换后必须丢弃空草图，交给UE运行时生成。
    Landscape->FlushGrassComponents(nullptr, true);
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
        Restore(GrassKey, GrassType.Get(), Previous.bHadGrass,
            RetainedOriginalTypes.IsValidIndex(Previous.GrassIndex) ? RetainedOriginalTypes[Previous.GrassIndex].Get() : nullptr);
        Restore(FlowerKey, FlowerType.Get(), Previous.bHadFlowers,
            RetainedOriginalTypes.IsValidIndex(Previous.FlowersIndex) ? RetainedOriginalTypes[Previous.FlowersIndex].Get() : nullptr);
        for (const FPreviousOverride::FComponentTypes& Old : Previous.Components)
        {
            ULandscapeComponent* Component = Old.Component.Get();
            if (!IsValid(Component)) continue;
            auto Types = Component->GetNamedGrassTypes();
            if (auto* Current = Types.Find(GrassName); Current && Current->Get() == GrassType.Get() && RetainedOriginalTypes.IsValidIndex(Old.GrassIndex))
                *Current = RetainedOriginalTypes[Old.GrassIndex];
            if (auto* Current = Types.Find(FlowerName); Current && Current->Get() == FlowerType.Get() && RetainedOriginalTypes.IsValidIndex(Old.FlowersIndex))
                *Current = RetainedOriginalTypes[Old.FlowersIndex];
            Component->SetNamedGrassTypes(Types);
        }
        Landscape->FlushGrassComponents(nullptr, false);
    }
    Bound.Reset();
    RetainedOriginalTypes.Reset();
}

void UDivineBeastsVillageGrassContactComponent::Bind(UDivineBeastsVillageGrassWorldSubsystem* OwnerSubsystem, int32 Slot)
{
    Subsystem = OwnerSubsystem;
    SlotIndex = Slot;
    if (auto* Character = Cast<ACharacter>(GetOwner()))
        Character->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::OnMoved);
}

void UDivineBeastsVillageGrassContactComponent::OnMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    if (auto* OwnerSubsystem = Subsystem.Get())
    {
        auto* Character = Cast<ACharacter>(GetOwner());
        if (SlotIndex == INDEX_NONE) OwnerSubsystem->AttachCharacter(Character);
        OwnerSubsystem->UpdateContact(SlotIndex, Character);
    }
}

void UDivineBeastsVillageGrassContactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* Character = Cast<ACharacter>(GetOwner()))
        Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::OnMoved);
    if (auto* OwnerSubsystem = Subsystem.Get()) OwnerSubsystem->RemoveContact(SlotIndex, this);
    Subsystem.Reset();
    SlotIndex = INDEX_NONE;
    Super::EndPlay(EndPlayReason);
}

void UDivineBeastsVillageGrassWorldSubsystem::AttachCharacter(ACharacter* Character)
{
    if (!bActive || !ContactCollection.IsValid() || !IsValid(Character) || Character->GetWorld() != GetWorld()) return;
    UDivineBeastsVillageGrassContactComponent* Observer = nullptr;
    for (const auto& Existing : Observers)
        if (Existing.IsValid() && Existing->GetOwner() == Character) { Observer = Existing.Get(); break; }
    if (!IsValid(Observer))
    {
        Observer = NewObject<UDivineBeastsVillageGrassContactComponent>(Character);
        if (!IsValid(Observer)) return;
        Character->AddInstanceComponent(Observer);
        Observer->RegisterComponent();
        Observer->Bind(this, INDEX_NONE);
        Observers.Add(Observer);
    }
    if (Observer->GetSlot() != INDEX_NONE) return;
    int32 Free = INDEX_NONE;
    for (int32 I = 0; I < 4; ++I) if (!Contacts[I].IsValid()) { Free = I; break; }
    if (Free == INDEX_NONE && Character->IsLocallyControlled())
    {
        for (int32 I = 0; I < 4; ++I)
        {
            ACharacter* Existing = Contacts[I].IsValid() ? Cast<ACharacter>(Contacts[I]->GetOwner()) : nullptr;
            if (Existing && !Existing->IsLocallyControlled())
            {
                Contacts[I]->SetSlot(INDEX_NONE);
                Contacts[I].Reset();
                ClearContact(I);
                Free = I;
                break;
            }
        }
    }
    if (Free == INDEX_NONE) return; // 保留移动订阅；角色获得本地控制后可抢占非本地槽。
    Contacts[Free] = Observer;
    Observer->SetSlot(Free);
    UpdateContact(Free, Character);
}

void UDivineBeastsVillageGrassWorldSubsystem::UpdateContact(int32 Slot, ACharacter* Character)
{
    UWorld* World = GetWorld();
    UMaterialParameterCollection* Collection = ContactCollection.Get();
    if (!bActive || Slot < 0 || Slot >= 4 || !IsValid(World) || !IsValid(Character) ||
        Character->GetWorld() != World || !IsValid(Collection)) return;
    UMaterialParameterCollectionInstance* MPC = World->GetParameterCollectionInstance(Collection);
    if (!IsValid(MPC)) return;
    const FString Suffix = Slot == 0 ? TEXT("") : FString::Printf(TEXT("-%d"), Slot);
    const FVector Position = Character->GetActorLocation();
    const FVector Direction = Character->GetActorForwardVector();
    const FLinearColor P(Position.X, Position.Y, Position.Z, 0.f);
    for (int32 I = 0; I < 3; ++I)
        MPC->SetVectorParameterValue(FName(*FString::Printf(TEXT("PlayerPos%d%s"), I, *Suffix)), P);
    MPC->SetVectorParameterValue(FName(*FString::Printf(TEXT("PlayerDir0%s"), *Suffix)),
        FLinearColor(Direction.X, Direction.Y, Direction.Z, 0.f));
}

void UDivineBeastsVillageGrassWorldSubsystem::ClearContact(int32 Slot)
{
    UWorld* World = GetWorld();
    UMaterialParameterCollection* Collection = ContactCollection.Get();
    if (Slot < 0 || Slot >= 4 || !IsValid(World) || !IsValid(Collection)) return;
    UMaterialParameterCollectionInstance* MPC = World->GetParameterCollectionInstance(Collection);
    if (!IsValid(MPC)) return;
    const FString Suffix = Slot == 0 ? TEXT("") : FString::Printf(TEXT("-%d"), Slot);
    for (int32 I = 0; I < 3; ++I)
        MPC->SetVectorParameterValue(FName(*FString::Printf(TEXT("PlayerPos%d%s"), I, *Suffix)),
            FLinearColor(1.e8f, 1.e8f, 1.e8f, 0.f));
}

void UDivineBeastsVillageGrassWorldSubsystem::RemoveContact(int32 Slot,
    const UDivineBeastsVillageGrassContactComponent* Component)
{
    Observers.RemoveAll([Component](const TWeakObjectPtr<UDivineBeastsVillageGrassContactComponent>& Item)
        { return !Item.IsValid() || Item.Get() == Component; });
    if (Slot < 0 || Slot >= 4 || Contacts[Slot].Get() != Component) return;
    Contacts[Slot].Reset();
    ClearContact(Slot);
}

void UDivineBeastsVillageGrassWorldSubsystem::Deinitialize()
{
    bActive = false;
    ++RequestGeneration;
    if (UWorld* World = GetWorld(); World && ActorHandle.IsValid()) World->RemoveOnActorSpawnedHandler(ActorHandle);
    if (LevelHandle.IsValid()) FWorldDelegates::LevelAddedToWorld.Remove(LevelHandle);
    const auto OldObservers = MoveTemp(Observers);
    for (const auto& Observer : OldObservers) if (auto* Component = Observer.Get()) Component->DestroyComponent();
    for (int32 Slot = 0; Slot < 4; ++Slot) { Contacts[Slot].Reset(); ClearContact(Slot); }
    UnbindAll();
    const FGamePlatformDataLease OldLease = Lease;
    Lease = {};
    GrassType.Reset(); FlowerType.Reset();
    ContactCollection.Reset();
    if (UWorld* World = GetWorld())
        if (UGameInstance* Instance = World->GetGameInstance())
            if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*Instance); Data && OldLease.IsValid())
                Data->ReleaseResources(OldLease);
    Super::Deinitialize();
}
