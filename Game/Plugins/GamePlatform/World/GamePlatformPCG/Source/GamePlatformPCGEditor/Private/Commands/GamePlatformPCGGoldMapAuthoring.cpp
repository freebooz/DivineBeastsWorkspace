#include "Commands/GamePlatformPCGGoldMapAuthoring.h"

#include "Actors/GamePlatformPCGActors.h"
#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGNode.h"
#include "Nodes/GamePlatformPCGNodes.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "Types/GamePlatformPCGDomainIds.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WorldPartition/WorldPartitionHelpers.h"

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


/**
 * 在绑定UPCGGraph前准备固定编辑器生成范围。UE5.8跟踪器通过Actor的PrimitiveComponent
 * 推导执行源Bounds；Spline/Polygon/Connector单独没有Primitive范围，会在地图保存/重开时
 * 报Invalid bounds。生成范围只是无碰撞、无导航的静态数字快照，不主动生成PCG或影响服务器。
 */
bool ApplyGenerationBounds(AGamePlatformPCGActorBase& Actor, const FPlacement& Spec, FString& Error)
{
    UBoxComponent* Bounds = Actor.GenerationBounds.Get();
    if (!IsValid(Bounds))
    {
        Error = TEXT("平台PCG放置器缺少无碰撞GenerationBounds，不能交给官方PCG追踪。");
        return false;
    }

    FVector Center = Actor.GetActorLocation();
    FVector Half = !Spec.HalfExtents.IsNearlyZero()
        ? Spec.HalfExtents : FVector(250.0f, 250.0f, 200.0f);
    if (!Spec.Shape.IsEmpty())
    {
        FBox ShapeBounds(EForceInit::ForceInit);
        for (const FVector& Point : Spec.Shape)
        {
            if (Point.ContainsNaN())
            {
                Error = TEXT("GoldLevel样条存在非法空间点坐标。");
                return false;
            }
            ShapeBounds += Point;
        }
        if (!ShapeBounds.IsValid)
        {
            Error = TEXT("GoldLevel几何不能计算合法的PCG空间范围。");
            return false;
        }
        const double Margin = Spec.HalfWidth >= 0.0f ? Spec.HalfWidth : 100.0;
        Center = ShapeBounds.GetCenter();
        Half = ShapeBounds.GetExtent() + FVector(Margin, Margin, 200.0);
    }
    if (!FMath::IsFinite(Half.X) || !FMath::IsFinite(Half.Y) ||
        !FMath::IsFinite(Half.Z) || Half.GetMin() < 1.0)
    {
        Error = TEXT("GoldLevel生成范围必须包含有限且非零的XYZ体积。");
        return false;
    }
    Bounds->Modify();
    Bounds->SetBoxExtent(Half, false);
    Bounds->SetWorldLocation(Center);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetCanEverAffectNavigation(false);
    return true;
}
bool Preflight(const TArray<FPlacement>& Specs, TArray<UClass*>& Classes,
    TArray<UPCGGraph*>& Graphs, FString& Error, bool bRequiresNewMap = true)
{
    const FString TargetFilename = FPackageName::LongPackageNameToFilename(
        MapPackage, FPackageName::GetMapPackageExtension());
    if (bRequiresNewMap && (FPackageName::DoesPackageExist(MapPackage) ||
        IFileManager::Get().FileExists(*TargetFilename)))
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

        // 严格先配置样条/空间几何，再把批准的Graph交给官方PCGComponent追踪器。
        if (!ApplyGenerationBounds(*Actor, Spec, Error) ||
            !UGamePlatformPCGEditorLibrary::BindPlacedActorGraph(Actor, Graphs[Index], Error))
        {
            Error = FString(Spec.Label) + TEXT("生成范围/PCG图绑定失败：") + Error;
            return false;
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

bool GamePlatformPCGGoldMap::Repair(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    // 明确只修复本工单已有GoldLevel，不创建新地图、不执行Generate、不修改其它世界内容包。
    const FString Filename = FPackageName::LongPackageNameToFilename(
        MapPackage, FPackageName::GetMapPackageExtension());
    if (!FPackageName::DoesPackageExist(MapPackage) || !IFileManager::Get().FileExists(*Filename))
    {
        Error = TEXT("指定GoldLevel原始地图不存在，不允许借修复命令创建新地图。");
        return false;
    }
    TArray<UClass*> Classes;
    TArray<UPCGGraph*> Graphs;
    const TArray<FPlacement> Specs = GetPlacements();
    if (!Preflight(Specs, Classes, Graphs, Error, /*bRequiresNewMap=*/false))
    {
        return false;
    }
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Filename);
    if (!IsValid(World) || !IsValid(World->PersistentLevel) ||
        !IsValid(World->GetOutermost()) || World->GetOutermost()->GetName() != MapPackage)
    {
        Error = TEXT("UE无法按唯一GoldLevel绝对文件路径加载预先保存的真实UWorld。");
        return false;
    }

    AGamePlatformPCGWorldDirector* Director = nullptr;
    TMap<FString, AGamePlatformPCGActorBase*> Found;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor)) { continue; }
        if (AGamePlatformPCGWorldDirector* Candidate = Cast<AGamePlatformPCGWorldDirector>(Actor))
        {
            if (Director)
            {
                Error = TEXT("GoldLevel包含多个WorldDirector，拒绝更改未知地图。");
                return false;
            }
            Director = Candidate;
        }
        if (AGamePlatformPCGActorBase* Placement = Cast<AGamePlatformPCGActorBase>(Actor))
        {
            const FString Label = Placement->GetActorLabel();
            if (!Placement->SourceId.IsValid() || Found.Contains(Label))
            {
                Error = TEXT("GoldLevel旧Actor身份无效或Label重复，拒绝修复。");
                return false;
            }
            Found.Add(Label, Placement);
        }
    }
    if (!Director || Found.Num() != Specs.Num() || !Director->ValidateParticipantSet(Error))
    {
        Error = TEXT("GoldLevel真实Actor和WorldDirector注册集合不符合原批准的11项配置：") + Error;
        return false;
    }
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        const FPlacement& Spec = Specs[Index];
        AGamePlatformPCGActorBase* const* Ptr = Found.Find(Spec.Label);
        if (!Ptr || !IsValid(*Ptr) || !(*Ptr)->IsA(Classes[Index]) ||
            (*Ptr)->DomainId != Spec.Domain || (*Ptr)->WorldStage != Spec.Stage ||
            (*Ptr)->Graph.IsNull())
        {
            Error = FString(TEXT("GoldLevel未知/非法Actor类型、领域或状态，拒绝改动：")) + Spec.Label;
            return false;
        }
        // 可被后端存档的SourceId不因Bounds修复而重新派生。
    }

    // 先对唯一目标.umap建立不会影响Cook的Saved诊断备份；禁止覆盖已有备份。
    const FString BackupRoot = FPaths::ProjectSavedDir() +
        TEXT("Validation/GamePlatformPCG/GoldMapRepairs/");
    IFileManager::Get().MakeDirectory(*BackupRoot, true);
    const FString Backup = BackupRoot + TEXT("PCG_GoldLevel_M1_before_bounds_") +
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".umap");
    if (IFileManager::Get().FileExists(*Backup) ||
        IFileManager::Get().Copy(*Backup, *Filename, true, true) != COPY_OK)
    {
        Error = TEXT("GoldLevel原始.umap在Saved验证目录中备份失败，拒绝原位修复。");
        return false;
    }

    World->Modify();
    for (const FPlacement& Spec : Specs)
    {
        AGamePlatformPCGActorBase* Actor = Found.FindChecked(Spec.Label);
        Actor->Modify();
        if (!ApplyGenerationBounds(*Actor, Spec, Error))
        {
            Error = FString(Spec.Label) + TEXT("空间范围原位修复失败：") + Error;
            return false;
        }
    }
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPackage))
    {
        Error = TEXT("GoldLevel空间范围已经校正但UE原位保存失败，原始备份见：") + Backup;
        return false;
    }
    UE_LOG(LogGamePlatformPCG, Display,
        TEXT("PCG GoldLevel受控Bounds修复已保存，保留11稳定来源ID；原始地图备份：%s"),
        *Backup);
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
                Placement->Graph.IsNull() || !IsValid(Placement->PCGComponent.Get()) ||
                !IsValid(Placement->GenerationBounds.Get()) ||
                !Placement->GetComponentsBoundingBox().IsValid)
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

bool GamePlatformPCGGoldMap::ProbeSpatial(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    // 此入口只从磁盘读入本工单批准的GoldLevel；不运行PCG、不保存或替换用户地图。
    const FString Filename = FPackageName::LongPackageNameToFilename(
        MapPackage, FPackageName::GetMapPackageExtension());
    if (!FPackageName::DoesPackageExist(MapPackage) || !IFileManager::Get().FileExists(*Filename))
    {
        Error = TEXT("GoldLevel真实地图缺失，不允许从预设坐标伪造验证结果。");
        return false;
    }
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Filename);
    if (!IsValid(World) || !IsValid(World->PersistentLevel) ||
        World->GetOutermost()->GetName() != MapPackage)
    {
        Error = TEXT("UE5.8未能独立重开已保存的GoldLevel UWorld。");
        return false;
    }

    AGamePlatformPCGWorldDirector* Director = nullptr;
    TMap<FString, AGamePlatformPCGActorBase*> ByLabel;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor)) { continue; }
        if (AGamePlatformPCGWorldDirector* Found = Cast<AGamePlatformPCGWorldDirector>(Actor))
        {
            if (Director)
            {
                Error = TEXT("金标准地图存在多个WorldDirector，拒绝选择任意一个绕过注册审查。");
                return false;
            }
            Director = Found;
        }
        if (AGamePlatformPCGActorBase* Placement = Cast<AGamePlatformPCGActorBase>(Actor))
        {
            if (ByLabel.Contains(Placement->GetActorLabel()))
            {
                Error = TEXT("金标准放置器Label重复，无法建立唯一来源映射。");
                return false;
            }
            ByLabel.Add(Placement->GetActorLabel(), Placement);
        }
    }
    if (!Director || ByLabel.Num() != GetPlacements().Num() ||
        !Director->ValidateParticipantSet(Error))
    {
        Error = TEXT("GoldLevel唯一世界编排器、放置器数量或注册集合非法：") + Error;
        return false;
    }
    for (const FPlacement& Spec : GetPlacements())
    {
        AGamePlatformPCGActorBase* const* Placement = ByLabel.Find(Spec.Label);
        if (!Placement || !IsValid(*Placement) || !(*Placement)->SourceId.IsValid() ||
            (*Placement)->DomainId != Spec.Domain || (*Placement)->WorldStage != Spec.Stage ||
            !Director->GetParticipantsForStage(Spec.Stage).Contains(*Placement))
        {
            Error = TEXT("已保存GoldLevel来源不符合领域、阶段或登记约束：") + FString(Spec.Label);
            return false;
        }
    }

    TArray<FGamePlatformPCGSpatialMask> AllMasks;
    if (!Director->CollectSpatialMasks(AllMasks, Error) || AllMasks.Num() < 7)
    {
        Error = TEXT("已保存的道路、农田、围栏、门桥或排除体积缺失：") + Error;
        return false;
    }
    TArray<FGamePlatformPCGSpatialMask> CropMasks = AllMasks;
    // 农田与作物不能自相排斥；该过滤规则与批准的Crop Graph实例保持一致。
    CropMasks.RemoveAll([](const FGamePlatformPCGSpatialMask& Mask)
    {
        return Mask.DomainId == FGamePlatformPCGDomainIds::AgriParcel ||
            Mask.DomainId == FGamePlatformPCGDomainIds::AgriCrop;
    });

    // 读取真正保存的3份图节点参数，防止仅WorldDirector数值输入正确，但Spawner图仍使用旧空间快照。
    const auto VerifySavedGraph = [&Error](const TCHAR* Name, int32 Priority,
        const TArray<FGamePlatformPCGSpatialMask>& ExpectedMasks) -> bool
    {
        const FString AssetName = FString(TEXT("PCG_Gold_")) + Name;
        const FString ObjectPath = GoldRoot + TEXT("Realized/") +
            AssetName + TEXT(".") + AssetName;
        UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *ObjectPath);
        if (!IsValid(Graph) || Graph->bIsTemplate)
        {
            Error = TEXT("GoldLevel真实图缺失或仍是不能实例化的模板：") + ObjectPath;
            return false;
        }
        const UGamePlatformPCGSpatialCarveSettings* Settings = nullptr;
        for (const UPCGNode* Node : Graph->GetNodes())
        {
            if (const auto* Found = Node
                    ? Cast<UGamePlatformPCGSpatialCarveSettings>(Node->GetSettings()) : nullptr)
            {
                if (Settings)
                {
                    Error = TEXT("已保存的PCG图存在重复SpatialCarve节点：") + ObjectPath;
                    return false;
                }
                Settings = Found;
            }
        }
        if (!Settings || Settings->SubjectPriority != Priority ||
            Settings->Masks.Num() != ExpectedMasks.Num())
        {
            Error = TEXT("PCG图节点掩码数量或优先级与当前GoldLevel真实来源不一致：") + ObjectPath;
            return false;
        }
        for (const FGamePlatformPCGSpatialMask& Expected : ExpectedMasks)
        {
            const FGamePlatformPCGSpatialMask* Saved = Settings->Masks.FindByPredicate(
                [&Expected](const FGamePlatformPCGSpatialMask& Value)
                {
                    return Value.SourceId == Expected.SourceId;
                });
            if (!Saved || Saved->DomainId != Expected.DomainId ||
                Saved->Priority != Expected.Priority || Saved->bClosed != Expected.bClosed ||
                Saved->bFillInterior != Expected.bFillInterior ||
                !FMath::IsNearlyEqual(Saved->Strength, Expected.Strength) ||
                !FMath::IsNearlyEqual(Saved->HalfWidthCm, Expected.HalfWidthCm, 0.01f) ||
                Saved->Vertices.Num() != Expected.Vertices.Num())
            {
                Error = TEXT("PCG图中的静态排除快照已过期，来源/强度/几何结构变化：") +
                    ObjectPath + TEXT(" / ") + Expected.SourceId.ToString(EGuidFormats::Digits);
                return false;
            }
            for (int32 Index = 0; Index < Expected.Vertices.Num(); ++Index)
            {
                if (FVector2D::DistSquared(Saved->Vertices[Index],
                        Expected.Vertices[Index]) > FMath::Square(0.1))
                {
                    Error = TEXT("PCG实际图空间顶点与地图保存结果存在超过0.1厘米差异：") + ObjectPath;
                    return false;
                }
            }
        }
        UE_LOG(LogGamePlatformPCG, Display,
            TEXT("GoldLevel已保存真实Graph空间快照与11名参与者匹配：%s"), Name);
        return true;
    };
    if (!VerifySavedGraph(TEXT("Canopy"), 30, AllMasks) ||
        !VerifySavedGraph(TEXT("Rock"), 15, AllMasks) ||
        !VerifySavedGraph(TEXT("Crop"), 20, CropMasks))
    {
        return false;
    }

    const auto Probe = [&Error, &ByLabel](const TCHAR* CaseName, const FVector2D Position,
        int32 Priority, const TCHAR* ExpectedLabel, const TArray<FGamePlatformPCGSpatialMask>& Masks) -> bool
    {
        float Strength = 0.0f;
        FGuid Source;
        if (!FGamePlatformPCGSpatialRules::Evaluate(Position, Priority, 0.5f, Masks, Strength, Source))
        {
            Error = FString(CaseName) + TEXT("：真实地图的空间Mask数据非法，拒绝输出。");
            return false;
        }
        if (ExpectedLabel)
        {
            AGamePlatformPCGActorBase* const* Actor = ByLabel.Find(ExpectedLabel);
            if (!Actor || Source != (*Actor)->SourceId || !FMath::IsNearlyEqual(Strength, 1.0f))
            {
                Error = FString(CaseName) + TEXT("：选中的来源或排除强度不符，应来自") +
                    ExpectedLabel + TEXT("；实际来源=") + Source.ToString(EGuidFormats::Digits);
                return false;
            }
        }
        else if (Source.IsValid() || Strength > KINDA_SMALL_NUMBER)
        {
            Error = FString(CaseName) + TEXT("：非排除区域被误切，来源=") +
                Source.ToString(EGuidFormats::Digits);
            return false;
        }
        UE_LOG(LogGamePlatformPCG, Display,
            TEXT("GoldLevel只读空间规则探针通过：%s"), CaseName);
        return true;
    };

    // 数值取样只证明持久化几何/优先级合同，不统计官方PCG生成的真实网格实例。
    if (!Probe(TEXT("道路切林"), FVector2D(-700, 0), 30, TEXT("PCG_MajorRoad"), AllMasks) ||
        !Probe(TEXT("小径切林"), FVector2D(-700, -500), 30, TEXT("PCG_MinorPath"), AllMasks) ||
        !Probe(TEXT("道路切田"), FVector2D(700, 100), 20, TEXT("PCG_MajorRoad"), CropMasks) ||
        !Probe(TEXT("农门优先保留"), FVector2D(650, -150), 30, TEXT("PCG_Gate"), AllMasks) ||
        !Probe(TEXT("桥梁优先保留"), FVector2D(-1100, 0), 30, TEXT("PCG_HandPlacedBridge"), AllMasks) ||
        !Probe(TEXT("人工锁最高优先级"), FVector2D(-20, 0), 30, TEXT("PCG_ManualLock"), AllMasks) ||
        !Probe(TEXT("农田内部作物不误排除"), FVector2D(700, 500), 20, nullptr, CropMasks) ||
        !Probe(TEXT("森林非排除区保持可生成"), FVector2D(-700, -350), 30, nullptr, AllMasks) ||
        !Probe(TEXT("高优先级对象不被道路切除"), FVector2D(-700, 0), 80, nullptr, AllMasks))
    {
        return false;
    }

    const AGamePlatformPCGActorBase* const* FenceActor = ByLabel.Find(TEXT("PCG_FieldFence"));
    const FGamePlatformPCGSpatialMask* Fence = FenceActor
        ? AllMasks.FindByPredicate([FenceActor](const FGamePlatformPCGSpatialMask& Mask)
            { return Mask.SourceId == (*FenceActor)->SourceId; }) : nullptr;
    if (!Fence || !Fence->bClosed || Fence->bFillInterior ||
        !FGamePlatformPCGSpatialRules::ContainsPoint(*Fence, FVector2D(400, 50)) ||
        FGamePlatformPCGSpatialRules::ContainsPoint(*Fence, FVector2D(700, 500)))
    {
        Error = TEXT("闭合田篱应只排除边线缓冲区，不能清空内部全部作物。");
        return false;
    }

    TArray<FGamePlatformPCGSpatialMask> Reversed = AllMasks;
    for (int32 L = 0, R = Reversed.Num() - 1; L < R; ++L, --R)
    {
        Reversed.Swap(L, R);
    }
    if (!Probe(TEXT("逆序登记仍有稳定优先级"), FVector2D(-700, 0), 30,
            TEXT("PCG_MajorRoad"), Reversed))
    {
        return false;
    }

    UE_LOG(LogGamePlatformPCG, Display,
        TEXT("GoldLevel只读空间探针已核验：1编排器、11真实放置器、%d排除来源及10项数值取样。"
             "尚未执行PCG Generate、Spawner实例、G01～G16正式验收、导航或Cook。"),
        AllMasks.Num());
    return true;
}

bool GamePlatformPCGGoldMap::PreviewGenerated(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    // 实际调用官方PCG生成：只在新的UE Editor命令行加载本工单地图。
    // 本函数不保存地图/组件、不执行Cook，不允许任何正式Village资产被本次测试修改。
    const FString Filename = FPackageName::LongPackageNameToFilename(
        MapPackage, FPackageName::GetMapPackageExtension());
    if (!GEditor || !FPackageName::DoesPackageExist(MapPackage) ||
        !IFileManager::Get().FileExists(*Filename))
    {
        Error = TEXT("真实GoldLevel地图或UE编辑器上下文缺失，拒绝伪造静态实例生成结果。");
        return false;
    }
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Filename);
    if (!IsValid(World) || World->WorldType != EWorldType::Editor ||
        !IsValid(World->PersistentLevel) || World->GetOutermost()->GetName() != MapPackage)
    {
        Error = TEXT("仅允许在独立UE Editor加载已保存的GoldLevel地图。");
        return false;
    }

    AGamePlatformPCGWorldDirector* Director = nullptr;
    TMap<FString, AGamePlatformPCGActorBase*> Actors;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor)) { continue; }
        if (auto* Found = Cast<AGamePlatformPCGWorldDirector>(Actor))
        {
            if (Director)
            {
                Error = TEXT("已有多个PCG世界编排器，禁止继续真实生成。");
                return false;
            }
            Director = Found;
        }
        if (auto* Placement = Cast<AGamePlatformPCGActorBase>(Actor))
        {
            if (Actors.Contains(Placement->GetActorLabel()))
            {
                Error = TEXT("PCG参与者存在重复Label，无法限定本次拥有的生成结果。");
                return false;
            }
            Actors.Add(Placement->GetActorLabel(), Placement);
        }
    }
    if (!Director || Actors.Num() != GetPlacements().Num() ||
        !Director->ValidateParticipantSet(Error))
    {
        Error = TEXT("GoldLevel来源数量、稳定ID或编排器注册无效：") + Error;
        return false;
    }
    TArray<AGamePlatformPCGActorBase*> Plan;
    if (!Director->BuildStaticExecutionPlan(Plan, Error) || Plan.Num() != Actors.Num())
    {
        Error = TEXT("PCG静态阶段或已保存的Graph合同不合法：") + Error;
        return false;
    }
    TArray<FGamePlatformPCGSpatialMask> AllMasks;
    if (!Director->CollectSpatialMasks(AllMasks, Error))
    {
        Error = TEXT("PCG已保存的道路/地块/人工锁几何不合法：") + Error;
        return false;
    }
    TArray<FGamePlatformPCGSpatialMask> CropMasks = AllMasks;
    CropMasks.RemoveAll([](const FGamePlatformPCGSpatialMask& Mask)
    {
        return Mask.DomainId == FGamePlatformPCGDomainIds::AgriParcel ||
            Mask.DomainId == FGamePlatformPCGDomainIds::AgriCrop;
    });

    struct FPreview
    {
        const TCHAR* Label;
        const TCHAR* GraphName;
        int32 Priority;
        const TArray<FGamePlatformPCGSpatialMask>* Masks;
    };
    const TArray<FPreview> Previews = {
        {TEXT("PCG_Canopy"), TEXT("Canopy"), 30, &AllMasks},
        {TEXT("PCG_Rock"), TEXT("Rock"), 15, &AllMasks},
        {TEXT("PCG_Crops"), TEXT("Crop"), 20, &CropMasks}
    };
    TArray<TWeakObjectPtr<UPCGComponent>> ThisRunComponents;
    ThisRunComponents.Reserve(Previews.Num());

    // 始终清理本次新启动的组件。拒绝对地图原有已生成/管理资源运行，以免误删除用户结果。
    ON_SCOPE_EXIT
    {
        for (const TWeakObjectPtr<UPCGComponent>& Weak : ThisRunComponents)
        {
            if (UPCGComponent* Component = Weak.Get())
            {
                if (Component->IsGenerating())
                {
                    Component->CancelGeneration();
                }
                Component->CleanupLocalImmediate(/*bRemoveComponents=*/true,
                    /*bCleanupLocalComponents=*/true);
            }
        }
    };

    // UE官方PCGWorldPartitionBuilder也采用FakeEngineTick驱动编辑器任务；
    // 此处限定每组件最长60秒，超时即取消并清理，不无限循环。
    World->BlockTillLevelStreamingCompleted();
    FWorldPartitionHelpers::FakeEngineTick(World);

    for (const FPreview& Spec : Previews)
    {
        AGamePlatformPCGActorBase* const* ActorPtr = Actors.Find(Spec.Label);
        AGamePlatformPCGActorBase* Actor = ActorPtr ? *ActorPtr : nullptr;
        UPCGComponent* Component = IsValid(Actor) ? Actor->PCGComponent.Get() : nullptr;
        const FString GraphName = FString(TEXT("PCG_Gold_")) + Spec.GraphName;
        const FString Path = GoldRoot + TEXT("Realized/") +
            GraphName + TEXT(".") + GraphName;
        UPCGGraph* SavedGraph = LoadObject<UPCGGraph>(nullptr, *Path);
        if (!IsValid(Component) || !IsValid(SavedGraph) || SavedGraph->bIsTemplate ||
            Component->GetGraph() != SavedGraph || Component->IsGenerating() ||
            Component->IsCleaningUp() || Component->bGenerated ||
            Component->IsGeneratedOffline() || !Component->AreManagedResourcesAccessible() ||
            !Component->GetGeneratedGraphOutput().TaggedData.IsEmpty())
        {
            Error = FString(TEXT("Gold实际预览生成前置不满足、图不是审批实例或已有结果：")) +
                Spec.Label;
            return false;
        }

        bool bExistingManagedResource = false;
        Component->ForEachConstManagedResource(
            [&bExistingManagedResource](const UPCGManagedResource*)
            {
                bExistingManagedResource = true;
            });
        if (bExistingManagedResource)
        {
            Error = FString(TEXT("已有PCG Managed Resource，拒绝覆盖/清理：")) + Spec.Label;
            return false;
        }

        const FPCGTaskId Task = Component->GenerateLocalGetTaskId(/*bForce=*/true);
        if (Task == InvalidPCGTaskId)
        {
            Error = FString(TEXT("官方PCG引擎拒绝调度网格生成任务：")) + Spec.Label;
            return false;
        }
        ThisRunComponents.Add(Component);

        const double Start = FPlatformTime::Seconds();
        while (Component->IsGenerating() && FPlatformTime::Seconds() - Start < 60.0)
        {
            FWorldPartitionHelpers::FakeEngineTick(World);
            FPlatformProcess::Sleep(0.005f);
        }
        if (Component->IsGenerating() || Component->IsCleaningUp() ||
            Component->GetGeneratedGraphOutput().bCancelExecution ||
            !Component->AreManagedResourcesAccessible())
        {
            Error = FString(TEXT("官方PCG生成超时或取消、资源仍不可读取：")) + Spec.Label;
            return false;
        }

        int64 TotalInstances = 0;
        bool bValid = true;
        Component->ForEachConstManagedResource(
            [&bValid, &TotalInstances, &Spec, World, Component](const UPCGManagedResource* Resource)
            {
                const UPCGManagedISMComponent* Managed = Cast<UPCGManagedISMComponent>(Resource);
                if (!Managed)
                {
                    bValid = false;
                    return;
                }
                const UInstancedStaticMeshComponent* ISM = Managed->GetComponent();
                if (!IsValid(ISM) || !ISM->IsRegistered() ||
                    ISM->GetWorld() != World || ISM->GetOwner() != Component->GetOwner() ||
                    !IsValid(ISM->GetStaticMesh()) ||
                    ISM->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
                    ISM->GetGenerateOverlapEvents() || ISM->CanEverAffectNavigation() ||
                    ISM->GetIsReplicated())
                {
                    bValid = false;
                    return;
                }
                const int32 N = ISM->GetInstanceCount();
                if (N < 0 || N > 20000 || TotalInstances + N > 20000)
                {
                    bValid = false;
                    return;
                }
                for (int32 I = 0; I < N; ++I)
                {
                    FTransform Transform;
                    if (!ISM->GetInstanceTransform(I, Transform, /*bWorldSpace=*/true))
                    {
                        bValid = false;
                        return;
                    }
                    const FVector Point = Transform.GetLocation();
                    float Strength = 0.0f;
                    FGuid Source;
                    if (!FGamePlatformPCGSpatialRules::Evaluate(
                            FVector2D(Point.X, Point.Y), Spec.Priority, 0.5f,
                            *Spec.Masks, Strength, Source) || Source.IsValid())
                    {
                        bValid = false;
                        return;
                    }
                }
                TotalInstances += N;
            });
        if (!bValid || TotalInstances <= 0)
        {
            Error = FString(TEXT("GoldLevel官方PCG没有产生合法非碰撞实例、越过排除区或超出预算：")) +
                Spec.Label + TEXT("，实例数=") + FString::Printf(TEXT("%lld"), TotalInstances);
            return false;
        }
        UE_LOG(LogGamePlatformPCG, Display,
            TEXT("GoldLevel官方PCG实例生成预览：%s，实际ISM实例=%lld，耗时=%.3fs；"
                 "范围内排除Mask与非碰撞/导航/复制隔离已复核。"),
            Spec.Label, TotalInstances, FPlatformTime::Seconds() - Start);
    }

    UE_LOG(LogGamePlatformPCG, Display,
        TEXT("GoldLevel三份已批准Graph真实生成预览完成：全部有非碰撞网格实例且无禁区实例。"
             "已注册自动清理，不保存地图；尚非G01-G16完整验收或客户端/服务器Cook。"));
    return true;
}

