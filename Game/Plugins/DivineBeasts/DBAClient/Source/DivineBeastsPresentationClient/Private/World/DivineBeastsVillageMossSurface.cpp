// 神兽联盟Village项目客户端苔藓覆盖：复用平台Surface材质、天气MPC及Data世界资源租约。
// 唯一写入对象是显式标记的客户端网格材质槽，不改变碰撞、导航、天气权威或服务器地图。
// 本类与Village雪层桥属于同一个世界作用域：无持续Tick，地图退出撤销委托并恢复原材质。
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"

#include "Settings/DivineBeastsVillageSnowSettings.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAVillageMoss, Log, All);

void UDivineBeastsVillageSnowWorldSubsystem::TryRequestMossMaterial()
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    const auto* Settings = GetDefault<UDivineBeastsVillageSnowSettings>();
    if (bClosing || !bVillageWorld || !IsValid(World) || World->bIsTearingDown ||
        !Settings || !Settings->bEnableTaggedMossSnowRocks ||
        Settings->MossSnowComponentTag.IsNone() || !Settings->MossSnowRockMaterial.IsValid() ||
        MossMaterialLease.IsValid())
    {
        return;
    }

    UGameInstance* Instance = World->GetGameInstance();
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data)
    {
        UE_LOG(LogDBAVillageMoss, Warning,
            TEXT("Village苔藓材质桥缺少GamePlatformData，原始网格材质保持不变。"));
        return;
    }

    // 异步租约归本World与本Subsystem，结束释放；不能用同步LoadObject绕过平台数据管理。
    const int64 Generation = ++MossRequestGeneration;
    const TWeakObjectPtr<UDivineBeastsVillageSnowWorldSubsystem> WeakThis(this);
    const FSoftObjectPath Path = Settings->MossSnowRockMaterial;
    FGamePlatformResult Accepted;
    MossMaterialLease = Data->AcquireResources(
        {Path}, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            if (UDivineBeastsVillageSnowWorldSubsystem* Self = WeakThis.Get())
            {
                Self->HandleMossMaterialLoaded(Generation, Lease, Result);
            }
        }, Accepted);
    if (!Accepted.IsSuccess() || !MossMaterialLease.IsValid())
    {
        UE_LOG(LogDBAVillageMoss, Warning,
            TEXT("Village苔藓MI资源租约未接纳，保持原材质，不进行同步重试。"));
    }
}

void UDivineBeastsVillageSnowWorldSubsystem::HandleMossMaterialLoaded(
    const int64 ExpectedGeneration, const FGamePlatformDataLease& CompletedLease,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (bClosing || !bVillageWorld || !IsValid(World) || World->bIsTearingDown ||
        ExpectedGeneration != MossRequestGeneration || !MossMaterialLease.IsValid() ||
        CompletedLease.LeaseId != MossMaterialLease.LeaseId ||
        CompletedLease.ScopeId != MossMaterialLease.ScopeId ||
        CompletedLease.Generation != MossMaterialLease.Generation ||
        CompletedLease.IssuerProof != MossMaterialLease.IssuerProof ||
        CompletedLease.ResourcePaths != MossMaterialLease.ResourcePaths)
    {
        return;
    }

    UGameInstance* Instance = World->GetGameInstance();
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    UMaterialInterface* Loaded = MossMaterialLease.ResourcePaths.Num() == 1
        ? Cast<UMaterialInterface>(MossMaterialLease.ResourcePaths[0].ResolveObject()) : nullptr;
    if (!Result.IsSuccess() || !Data ||
        Data->GetLeaseState(CompletedLease) != EGamePlatformDataRequestState::Succeeded ||
        !IsValid(Loaded) ||
        !Loaded->GetPathName().StartsWith(TEXT("/DBAPresentationPack_Core/Materials/Environment/")))
    {
        UE_LOG(LogDBAVillageMoss, Warning,
            TEXT("Village苔藓MI加载失败、类型不符或不属于项目纯表现包；不覆盖网格材质。"));
        if (Data) Data->ReleaseResources(CompletedLease);
        MossMaterialLease = {};
        ++MossRequestGeneration;
        return;
    }

    LoadedMossMaterial = Loaded;
    bMossMaterialReady = true;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        ApplyMossMaterialToActor(*It);
    }
    UE_LOG(LogDBAVillageMoss, Display,
        TEXT("VILLAGE_MOSS_MATERIAL_READY bindings=%d material=%s (0表示当前地图尚无主动标记的岩石组件)"),
        MossMaterialSlotBindings.Num(), *Loaded->GetPathName());
}

void UDivineBeastsVillageSnowWorldSubsystem::ApplyMossMaterialToActor(AActor* Actor)
{
    check(IsInGameThread());
    const UWorld* World = GetWorld();
    const auto* Settings = GetDefault<UDivineBeastsVillageSnowSettings>();
    UMaterialInterface* Material = LoadedMossMaterial.Get();
    if (bClosing || !bVillageWorld || !bMossMaterialReady || !IsValid(Actor) ||
        !IsValid(World) || Actor->GetWorld() != World || !IsValid(Material) ||
        !Settings || Settings->MossSnowComponentTag.IsNone())
    {
        return;
    }

    TInlineComponentArray<UMeshComponent*> Components;
    Actor->GetComponents(Components);
    for (UMeshComponent* Component : Components)
    {
        // 必须人工/PCG显式标记。未标记的屋顶、角色、地形、水面不会被误套苔藓。
        if (!IsValid(Component) || !Component->ComponentHasTag(Settings->MossSnowComponentTag))
        {
            continue;
        }
        const int32 NumMaterials = Component->GetNumMaterials();
        for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
        {
            if (MossMaterialSlotBindings.ContainsByPredicate(
                [Component, Slot](const FMossMaterialSlotBinding& Candidate)
                {
                    return Candidate.Component.Get() == Component && Candidate.MaterialSlot == Slot;
                }))
            {
                continue;
            }
            FMossMaterialSlotBinding Binding;
            Binding.Component = Component;
            Binding.MaterialSlot = Slot;
            Binding.RetainedMaterialIndex = RetainedMossOriginalMaterials.Add(Component->GetMaterial(Slot));
            MossMaterialSlotBindings.Add(Binding);
            Component->SetMaterial(Slot, Material);
        }
    }
}
