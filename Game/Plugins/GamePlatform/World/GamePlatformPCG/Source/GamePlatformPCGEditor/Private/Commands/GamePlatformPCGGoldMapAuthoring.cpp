#include "Commands/GamePlatformPCGGoldMapAuthoring.h"

#include "Actors/GamePlatformPCGActors.h"
#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GamePlatformPCGLog.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Types/GamePlatformPCGDomainIds.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
const FString GoldRoot = TEXT("/Game/Development/Foundation/PCG/");
const FString MapPackage = TEXT("/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1");
const FString BlueprintRoot = TEXT("/DBAWorldPack_Village/PCG/Blueprints/");

struct FPlacement
{
    const TCHAR* Blueprint;
    const TCHAR* Label;
    FVector Position;
    const TCHAR* GraphId;
    bool bRealized;
    FName Domain;
    EGamePlatformPCGWorldStage Stage;
    TArray<FVector> Shape;
    bool bClosed = false;
    FVector HalfExtents = FVector::ZeroVector;
    int32 Priority = INDEX_NONE;
    float HalfWidth = -1.0f;
};

/** 与GoldLevel验收矩阵对应，清单在平台Editor集中定义；项目资源留在Village内容包。 */
TArray<FPlacement> GetPlacements()
{
    return {
        {TEXT("BP_PCG_Forest"), TEXT("PCG_Canopy"), FVector::ZeroVector, TEXT("Canopy"), true,
            FGamePlatformPCGDomainIds::ForestCanopy, EGamePlatformPCGWorldStage::Scatter,
            {}, false, FVector(1550, 950, 260)},
        {TEXT("BP_PCG_Rock"), TEXT("PCG_Rock"), FVector(1100, 700, 0), TEXT("Rock"), true,
            FGamePlatformPCGDomainIds::RockScatter, EGamePlatformPCGWorldStage::Scatter,
            {}, false, FVector(420, 380, 200)},
        {TEXT("BP_PCG_Resource"), TEXT("PCG_Resource"), FVector(800, -700, 0), TEXT("TPL_Base"), false,
            FGamePlatformPCGDomainIds::PlayResource, EGamePlatformPCGWorldStage::GameplayAnchors,
            {}, false, FVector(300, 300, 160)},
        {TEXT("BP_PCG_Exclusion"), TEXT("PCG_ManualLock"), FVector(-20, 0, 0), TEXT("TPL_Base"), false,
            FGamePlatformPCGDomainIds::GameplayExclusion, EGamePlatformPCGWorldStage::FieldRead,
            {}, false, FVector(230, 210, 120), 100},
        {TEXT("BP_PCG_Road"), TEXT("PCG_MajorRoad"), FVector::ZeroVector, TEXT("TPL_LinearDresser"), false,
            FGamePlatformPCGDomainIds::RoadNetwork, EGamePlatformPCGWorldStage::Networks,
            {FVector(-1900, 0, 0), FVector(1900, 0, 0)}, false, FVector::ZeroVector, 70, 240.0f},
        {TEXT("BP_PCG_Road"), TEXT("PCG_MinorPath"), FVector(0, -500, 0), TEXT("TPL_LinearDresser"), false,
            FGamePlatformPCGDomainIds::PathTrail, EGamePlatformPCGWorldStage::Networks,
            {FVector(-1800, -500, 0), FVector(1700, -500, 0)}, false, FVector::ZeroVector, 40, 125.0f},
        {TEXT("BP_PCG_Road"), TEXT("PCG_FieldFence"), FVector(650, 300, 0), TEXT("TPL_Enclosure"), false,
            FGamePlatformPCGDomainIds::EnclosureFieldFence, EGamePlatformPCGWorldStage::Enclosures,
            {FVector(400, 50, 0), FVector(1050, 50, 0), FVector(1050, 850, 0), FVector(400, 850, 0)},
            true, FVector::ZeroVector, 48, 35.0f},
        {TEXT("BP_PCG_Field"), TEXT("PCG_Parcel"), FVector(700, 300, 0), TEXT("TPL_ParcelFill"), false,
            FGamePlatformPCGDomainIds::AgriParcel, EGamePlatformPCGWorldStage::Parcels,
            {FVector(400, 50, 0), FVector(1050, 50, 0), FVector(1050, 850, 0), FVector(400, 850, 0)},
            true, FVector::ZeroVector, 50},
        {TEXT("BP_PCG_Crops"), TEXT("PCG_Crops"), FVector(700, 320, 0), TEXT("Crop"), true,
            FGamePlatformPCGDomainIds::AgriCrop, EGamePlatformPCGWorldStage::Scatter,
            {FVector(450, 100, 0), FVector(1000, 100, 0), FVector(1000, 800, 0), FVector(450, 800, 0)}, true},
        {TEXT("BP_PCG_Bridge"), TEXT("PCG_Gate"), FVector(650, -150, 0), TEXT("TPL_GateInsert"), false,
            FGamePlatformPCGDomainIds::GateFarm, EGamePlatformPCGWorldStage::Connectors},
        {TEXT("BP_PCG_Bridge"), TEXT("PCG_HandPlacedBridge"), FVector(-1100, 0, 0), TEXT("TPL_Connector"), false,
            FGamePlatformPCGDomainIds::BridgeSpan, EGamePlatformPCGWorldStage::Connectors}
    };
}

FString GetGraphPath(const FPlacement& Item)
{
    const FString Name = Item.bRealized
        ? FString(TEXT("PCG_Gold_")) + Item.GraphId : FString(Item.GraphId);
    const FString Folder = Item.bRealized ? TEXT("Realized/") : TEXT("Templates/");
    return GoldRoot + Folder + Name + TEXT(".") + Name;
}

bool Preflight(const TArray<FPlacement>& Specs, TArray<UClass*>& Classes,
    TArray<UPCGGraph*>& Graphs, FString& Error)
{
    const FString TargetFilename = FPackageName::LongPackageNameToFilename(
        MapPackage, FPackageName::GetMapPackageExtension());
    if (FPackageName::DoesPackageExist(MapPackage) ||
        IFileManager::Get().FileExists(*TargetFilename))
    {
        Error = TEXT("PCG GoldLevel地图已存在，绝不覆盖。");
        return false;
    }

    Classes.Reset();
    Graphs.Reset();
    for (const FPlacement& Item : Specs)
    {
        const FString ClassPath = FString::Printf(TEXT("%s%s.%s_C"),
            *BlueprintRoot, Item.Blueprint, Item.Blueprint);
        UClass* Class = LoadClass<AGamePlatformPCGActorBase>(nullptr, *ClassPath);
        UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GetGraphPath(Item));
        if (!IsValid(Class) || !Class->IsChildOf(AGamePlatformPCGActorBase::StaticClass()) ||
            !IsValid(Graph) || !FGamePlatformPCGDomainIds::IsKnown(Item.Domain) ||
            (Item.bRealized && Graph->bIsTemplate))
        {
            Error = TEXT("GoldLevel缺少真实Blueprint、批准领域或Graph：") +
                ClassPath + TEXT(" / ") + GetGraphPath(Item);
            return false;
        }
        Classes.Add(Class);
        Graphs.Add(Graph);
    }
    return true;
}

template<typename TActor>
TActor* SpawnReviewActor(UWorld* World, const TCHAR* Label, const FVector& Position)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TActor* Actor = World->SpawnActor<TActor>(Position, FRotator::ZeroRotator, Params);
    if (IsValid(Actor)) { Actor->SetActorLabel(Label); }
    return Actor;
}

/** 仅向这3份由本工单创建的Gold图保存空间快照，不修改平台共享模板。 */
bool SaveGoldGraph(UPCGGraph* Graph, const TCHAR* Name, FString& Error)
{
    const FString Expected = GoldRoot + TEXT("Realized/PCG_Gold_") + Name;
    UPackage* Package = IsValid(Graph) ? Graph->GetOutermost() : nullptr;
    if (!IsValid(Package) || Package->GetName() != Expected || Graph->bIsTemplate)
    {
        Error = TEXT("金标准图包身份不合法，拒绝修改其它资源。");
        return false;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Expected, FPackageName::GetAssetPackageExtension());
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Graph, *Filename, Args))
    {
        Error = TEXT("金标准实际图空间Mask保存失败：") + Filename;
        return false;
    }
    return true;
}
}

bool GamePlatformPCGGoldMap::Create(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    const TArray<FPlacement> Specs = GetPlacements();
    TArray<UClass*> Classes;
    TArray<UPCGGraph*> Graphs;
    if (!Preflight(Specs, Classes, Graphs, Error)) { return false; }
    if (!GEditor)
    {
        Error = TEXT("缺少真正Editor上下文，禁止伪造测试地图。");
        return false;
    }

    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!IsValid(World) || World->WorldType != EWorldType::Editor || !IsValid(World->PersistentLevel))
    {
        Error = TEXT("UE5.8 Editor无法创建新UWorld。");
        return false;
    }
    AGamePlatformPCGWorldDirector* Director = SpawnReviewActor<AGamePlatformPCGWorldDirector>(
        World, TEXT("PCG_WorldDirector"), FVector::ZeroVector);
    if (!IsValid(Director))
    {
        Error = TEXT("PCG金标准地图未能实例化WorldDirector。");
        return false;
    }

    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        const FPlacement& Spec = Specs[Index];
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AGamePlatformPCGActorBase* Actor = World->SpawnActor<AGamePlatformPCGActorBase>(
            Classes[Index], Spec.Position, FRotator::ZeroRotator, Params);
        if (!IsValid(Actor))
        {
            Error = TEXT("真实PCG放置器Blueprint实例化失败：") + FString(Spec.Blueprint);
            return false;
        }
        Actor->SetActorLabel(Spec.Label);
        Actor->DomainId = Spec.Domain;
        Actor->WorldStage = Spec.Stage;
        if (!Actor->SourceId.IsValid()) { Actor->SourceId = FGuid::NewGuid(); }

        if (!UGamePlatformPCGEditorLibrary::BindPlacedActorGraph(Actor, Graphs[Index], Error))
        {
            Error = FString(Spec.Label) + TEXT("PCG图绑定失败：") + Error;
            return false;
        }

        if (!Spec.Shape.IsEmpty())
        {
            USplineComponent* Spline = nullptr;
            if (AGamePlatformPCGSplineActor* Road = Cast<AGamePlatformPCGSplineActor>(Actor))
            {
                Spline = Road->Spline;
            }
            else if (AGamePlatformPCGPolygonActor* Parcel = Cast<AGamePlatformPCGPolygonActor>(Actor))
            {
                Spline = Parcel->Boundary;
            }
            if (!IsValid(Spline))
            {
                Error = FString(Spec.Label) + TEXT("缺少样条或闭合地块边界组件。");
                return false;
            }
            Spline->ClearSplinePoints(false);
            for (const FVector& Point : Spec.Shape)
            {
                Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
            }
            Spline->SetClosedLoop(Spec.bClosed, false);
            Spline->UpdateSpline();
        }

        if (!Spec.HalfExtents.IsNearlyZero())
        {
            UBoxComponent* Box = nullptr;
            if (AGamePlatformPCGVolumeActor* Volume = Cast<AGamePlatformPCGVolumeActor>(Actor))
            {
                Box = Volume->Bounds;
            }
            else if (AGamePlatformPCGExclusionActor* Exclusion = Cast<AGamePlatformPCGExclusionActor>(Actor))
            {
                Box = Exclusion->Bounds;
            }
            if (!IsValid(Box))
            {
                Error = FString(Spec.Label) + TEXT("缺少预期非碰撞体积。");
                return false;
            }
            Box->SetBoxExtent(Spec.HalfExtents, false);
        }

        if (Spec.Priority != INDEX_NONE)
        {
            if (AGamePlatformPCGSplineActor* Road = Cast<AGamePlatformPCGSplineActor>(Actor))
            {
                Road->CarvePriority = Spec.Priority;
                if (Spec.HalfWidth >= 0.0f) { Road->CarveHalfWidthCm = Spec.HalfWidth; }
            }
            else if (AGamePlatformPCGPolygonActor* Parcel = Cast<AGamePlatformPCGPolygonActor>(Actor))
            {
                Parcel->CarvePriority = Spec.Priority;
            }
            else if (AGamePlatformPCGExclusionActor* Exclusion = Cast<AGamePlatformPCGExclusionActor>(Actor))
            {
                Exclusion->CarvePriority = Spec.Priority;
                Exclusion->Strength = 1.0f;
            }
        }

        if (!Director->RegisterParticipant(Actor))
        {
            Error = FString(Spec.Label) + TEXT("来源未能在唯一WorldDirector登记。");
            return false;
        }
    }

    if (!Director->ValidateParticipantSet(Error))
    {
        Error = TEXT("GoldLevel参与者源ID/Schema校验失败：") + Error;
        return false;
    }
    TArray<FGamePlatformPCGSpatialMask> Masks;
    if (!Director->CollectSpatialMasks(Masks, Error))
    {
        Error = TEXT("道路/地块/围栏/桥梁空间Mask输入不合法：") + Error;
        return false;
    }

    struct FRealizedBinding { const TCHAR* Name; int32 Priority; TArray<FName> Ignored; };
    const TArray<FRealizedBinding> Bindings = {
        {TEXT("Canopy"), 30, {}},
        {TEXT("Rock"), 15, {}},
        {TEXT("Crop"), 20, {FGamePlatformPCGDomainIds::AgriParcel, FGamePlatformPCGDomainIds::AgriCrop}}
    };
    for (const FRealizedBinding& Bind : Bindings)
    {
        const FString Path = GoldRoot + TEXT("Realized/PCG_Gold_") +
            Bind.Name + TEXT(".PCG_Gold_") + Bind.Name;
        UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *Path);
        if (!IsValid(Graph) ||
            !UGamePlatformPCGEditorLibrary::ConfigureRealizedGraphSpatialMasks(
                Graph, Director, Bind.Priority, Bind.Ignored, Error))
        {
            Error = TEXT("空间规则绑定失败：") + Path + TEXT(" / ") + Error;
            return false;
        }
    }
    for (const FRealizedBinding& Bind : Bindings)
    {
        const FString Path = GoldRoot + TEXT("Realized/PCG_Gold_") +
            Bind.Name + TEXT(".PCG_Gold_") + Bind.Name;
        if (!SaveGoldGraph(LoadObject<UPCGGraph>(nullptr, *Path), Bind.Name, Error))
        {
            return false;
        }
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    AStaticMeshActor* Ground = SpawnReviewActor<AStaticMeshActor>(
        World, TEXT("PCG_GoldLevel_TestGround"), FVector(0, 0, -50));
    if (!IsValid(Ground) || !IsValid(Mesh) || !IsValid(Ground->GetStaticMeshComponent()))
    {
        Error = TEXT("GoldLevel基础测试平面不可用。");
        return false;
    }
    Ground->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Ground->SetActorScale3D(FVector(40, 40, 1));
    if (!SpawnReviewActor<ADirectionalLight>(World, TEXT("PCG_ReviewSun"), FVector(0, 0, 1100)) ||
        !SpawnReviewActor<ASkyLight>(World, TEXT("PCG_ReviewSky"), FVector(0, 0, 1050)) ||
        !SpawnReviewActor<APlayerStart>(World, TEXT("PCG_ReviewSpawn"), FVector(-1600, -1600, 120)))
    {
        Error = TEXT("GoldLevel审核光源或出生标识放置失败。");
        return false;
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPackage) ||
        !FPackageName::DoesPackageExist(MapPackage))
    {
        Error = TEXT("UE5.8未成功真实保存PCG_GoldLevel_M1.umap。");
        return false;
    }
    UE_LOG(LogGamePlatformPCG, Display,
        TEXT("UE5.8 PCG GoldLevel地图已保存：%s，1编排器、11参与者、3份空间规则图。"), *MapPackage);
    return true;
}

bool GamePlatformPCGGoldMap::Verify(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    if (!FPackageName::DoesPackageExist(MapPackage))
    {
        Error = TEXT("GoldLevel地图.uasset/.umap尚未存在。");
        return false;
    }
    const FString ObjectPath = MapPackage + TEXT(".PCG_GoldLevel_M1");
    UWorld* World = LoadObject<UWorld>(nullptr, *ObjectPath);
    if (!IsValid(World) || !IsValid(World->PersistentLevel))
    {
        Error = TEXT("独立UE进程无法重新反序列化PCG GoldLevel UWorld。");
        return false;
    }
    int32 Directors = 0;
    int32 Actors = 0;
    TSet<FGuid> Sources;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor)) { continue; }
        if (Cast<AGamePlatformPCGWorldDirector>(Actor)) { ++Directors; }
        if (const AGamePlatformPCGActorBase* Placement = Cast<AGamePlatformPCGActorBase>(Actor))
        {
            if (!Placement->SourceId.IsValid() || Sources.Contains(Placement->SourceId) ||
                Placement->Graph.IsNull() || !IsValid(Placement->PCGComponent.Get()))
            {
                Error = TEXT("重开地图后PCG来源ID重复或图/原生组件缺失。");
                return false;
            }
            Sources.Add(Placement->SourceId);
            ++Actors;
        }
    }
    if (Directors != 1 || Actors != 11)
    {
        Error = FString::Printf(TEXT("GoldLevel的WorldDirector/Actor数量不符：%d / %d"), Directors, Actors);
        return false;
    }
    for (const TCHAR* Name : {TEXT("Canopy"), TEXT("Rock"), TEXT("Crop")})
    {
        const FString GraphPath = GoldRoot + TEXT("Realized/PCG_Gold_") + Name +
            TEXT(".PCG_Gold_") + Name;
        UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
        if (!IsValid(Graph) || Graph->bIsTemplate)
        {
            Error = TEXT("新UE进程无法回读真实Spawner图：") + GraphPath;
            return false;
        }
    }
    UE_LOG(LogGamePlatformPCG, Display,
        TEXT("GoldLevel独立重开：1 WorldDirector、11参与者、3份实际图均可读取（G01-G16仍待正式测试）。"));
    return true;
}
