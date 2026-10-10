#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "Authoring/PCGDevelopmentGraph.h"
#include "Manifests/PCGSourceFingerprint.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Actors/GamePlatformPCGActors.h"
#include "Nodes/GamePlatformPCGNodes.h"
#include "PCGNode.h"
#include "Services/GamePlatformPCGInspection.h"
#include "Services/GamePlatformPCGTemplateContract.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Types/GamePlatformPCGDomainIds.h"

namespace
{
const FString AssetRoot = TEXT("/Game/Development/Foundation/PCG/");

TArray<FName> GetM0M1FoundationTemplateIds()
{
    return
    {
        FGamePlatformPCGTemplateIds::Base,
        FGamePlatformPCGTemplateIds::ScatterSurface,
        FGamePlatformPCGTemplateIds::BiomeGenerator,
        FGamePlatformPCGTemplateIds::LinearDresser,
        FGamePlatformPCGTemplateIds::Enclosure,
        FGamePlatformPCGTemplateIds::EnclosureClosed,
        FGamePlatformPCGTemplateIds::Connector,
        FGamePlatformPCGTemplateIds::GateInsert,
        FGamePlatformPCGTemplateIds::ParcelFill,
        FGamePlatformPCGTemplateIds::CropField,
        FGamePlatformPCGTemplateIds::AssemblySpawn,
        FGamePlatformPCGTemplateIds::InterfaceBand
    };
}

bool IsOccupied(const FString& PackageName);

TArray<FName> GetM0M1FoundationSubgraphIds()
{
    return TArray<FName>(FGamePlatformPCGSubgraphIds::All());
}

TArray<FString> BuildPackages(const TArray<FName>& Ids, const FString& Folder)
{
    TArray<FString> Packages;
    Packages.Reserve(Ids.Num());
    for (const FName Id : Ids)
    {
        Packages.Add(AssetRoot + Folder + Id.ToString());
    }
    return Packages;
}

bool EnsurePackagesAvailable(const TArray<FString>& Packages, const TCHAR* Kind, FString& Error)
{
    for (const FString& PackageName : Packages)
    {
        if (IsOccupied(PackageName))
        {
            Error = FString::Printf(TEXT("%s资产已存在或内存中已占用；拒绝覆盖：%s"), Kind, *PackageName);
            return false;
        }
    }
    return true;
}

bool IsOccupied(const FString& PackageName)
{
    if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
    {
        return true;
    }
    const FString Stem = FPackageName::LongPackageNameToFilename(PackageName);
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Stem + TEXT(".*")), true, false);
    return !Files.IsEmpty();
}

bool SaveNewAsset(UObject& Asset, FString& Error)
{
    UPackage* Package = Asset.GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    if (FPackageName::DoesPackageExist(Package->GetName()))
    {
        Error = TEXT("保存前目标被占用，拒绝覆盖：") + Package->GetName();
        return false;
    }
    FAssetRegistryModule::AssetCreated(&Asset);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, &Asset, *Filename, Args))
    {
        Error = TEXT("真实资产保存失败；可能已有部分创建，禁止自动删除或覆盖：") + Filename;
        return false;
    }
    return true;
}
}

bool UGamePlatformPCGEditorLibrary::CreateDevelopmentAssets(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    TArray<FString> GraphPackages;
    TArray<FString> ProfilePackages;
    for (const FString& Suffix : {FString(TEXT("Cosmetic")), FString(TEXT("StaticCollision"))})
    {
        GraphPackages.Add(AssetRoot + TEXT("Graphs/PCG_") + Suffix);
        ProfilePackages.Add(AssetRoot + TEXT("Profiles/DA_PCG_") + Suffix);
    }
    for (int32 Index = 0; Index < GraphPackages.Num(); ++Index)
    {
        if (IsOccupied(GraphPackages[Index]) || IsOccupied(ProfilePackages[Index]))
        {
            Error = TEXT("开发资产已存在或内存中已占用；本阶段仅支持首次创建，请审查已有资产，不覆盖");
            return false;
        }
    }
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Mesh)
    {
        Error = TEXT("引擎基础Cube不可加载，不能用文本占位或手摆网格替代");
        return false;
    }
    for (int32 Index = 0; Index < GraphPackages.Num(); ++Index)
    {
        UPackage* GraphPackage = CreatePackage(*GraphPackages[Index]);
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateDevelopmentGraph(GraphPackage,
            FName(*FPackageName::GetLongPackageAssetName(GraphPackages[Index])), Mesh, Index == 1, Error);
        if (!Graph) { return false; }
        UPackage* ProfilePackage = CreatePackage(*ProfilePackages[Index]);
        auto* Profile = NewObject<UGamePlatformPCGProfileDefinition>(ProfilePackage,
            FName(*FPackageName::GetLongPackageAssetName(ProfilePackages[Index])), RF_Public | RF_Standalone | RF_Transactional);
        FGamePlatformId::TryParse(Index == 0 ? TEXT("foundation.pcg_cosmetic@1") : TEXT("foundation.pcg_static_collision@1"), Profile->LogicalId);
        FGamePlatformId::TryParse(TEXT("foundation.region_a@1"), Profile->RegionId);
        Profile->GraphReference = Graph;
        Profile->OutputMesh = Mesh;
        Profile->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
        Profile->OutputUsage = Index == 0 ? EGamePlatformPCGOutputUsage::Cosmetic : EGamePlatformPCGOutputUsage::StaticCollision;
        Profile->MinimumOutputs = 1;
        const FGamePlatformResult Validation = GamePlatformPCGInspection::ValidateApprovedGraph(*Profile);
        if (!Validation.IsSuccess())
        {
            Error = Validation.Code.ToString() + TEXT(": ") + Validation.Message;
            return false;
        }
        if (!SaveNewAsset(*Graph, Error) || !SaveNewAsset(*Profile, Error)) { return false; }
    }
    return true;
}

bool UGamePlatformPCGEditorLibrary::CreateFoundationTemplateAssets(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    const TArray<FName> TemplateIds = GetM0M1FoundationTemplateIds();
    TArray<FString> Packages;
    Packages.Reserve(TemplateIds.Num());
    for (const FName TemplateId : TemplateIds)
    {
        Packages.Add(AssetRoot + TEXT("Templates/") + TemplateId.ToString());
    }

    for (const FString& PackageName : Packages)
    {
        if (IsOccupied(PackageName))
        {
            Error = TEXT("Foundation模板资产已存在或内存中已占用；拒绝覆盖：") + PackageName;
            return false;
        }
    }

    TArray<TObjectPtr<UPCGGraph>> Graphs;
    Graphs.Reserve(TemplateIds.Num());
    for (int32 Index = 0; Index < TemplateIds.Num(); ++Index)
    {
        UPackage* Package = CreatePackage(*Packages[Index]);
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateFoundationTemplateGraph(
            Package,
            TemplateIds[Index],
            TemplateIds[Index],
            Error);
        if (!Graph)
        {
            return false;
        }

        UGamePlatformPCGProfileDefinition* Probe = NewObject<UGamePlatformPCGProfileDefinition>(GetTransientPackage());
        if (!FGamePlatformId::TryParse(TEXT("foundation.pcg_template_probe@1"), Probe->LogicalId) ||
            !FGamePlatformId::TryParse(TEXT("foundation.region_a@1"), Probe->RegionId))
        {
            Error = TEXT("Foundation模板探针逻辑身份初始化失败。");
            return false;
        }
        Probe->GraphReference = Graph;
        Probe->TemplateId = TemplateIds[Index];
        Probe->TemplateVersion = 1;
        Probe->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
        Probe->OutputUsage = EGamePlatformPCGOutputUsage::Cosmetic;
        Probe->MinimumOutputs = 0;

        const FGamePlatformResult Validation = GamePlatformPCGInspection::ValidateApprovedGraph(*Probe);
        if (!Validation.IsSuccess())
        {
            Error = Validation.Code.ToString() + TEXT(": ") + Validation.Message;
            return false;
        }
        Graphs.Add(Graph);
    }

    // 全部合同验证成功后再落盘，尽量避免因逻辑错误产生半套资产；保存失败仍保留现场供人工审查，不自动删包。
    for (UPCGGraph* Graph : Graphs)
    {
        if (!Graph || !SaveNewAsset(*Graph, Error))
        {
            return false;
        }
    }

    return true;
}

bool UGamePlatformPCGEditorLibrary::CreateFoundationSubgraphAssets(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    const TArray<FName> SubgraphIds = GetM0M1FoundationSubgraphIds();
    const TArray<FString> Packages = BuildPackages(SubgraphIds, TEXT("Subgraphs/"));
    if (!EnsurePackagesAvailable(Packages, TEXT("Foundation子图"), Error))
    {
        return false;
    }

    TArray<TObjectPtr<UPCGGraph>> Graphs;
    Graphs.Reserve(SubgraphIds.Num());
    for (int32 Index = 0; Index < SubgraphIds.Num(); ++Index)
    {
        UPackage* Package = CreatePackage(*Packages[Index]);
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateFoundationSubgraphGraph(
            Package, SubgraphIds[Index], SubgraphIds[Index], Error);
        if (!Graph)
        {
            return false;
        }
        Graphs.Add(Graph);
    }

    for (UPCGGraph* Graph : Graphs)
    {
        if (!Graph || !SaveNewAsset(*Graph, Error))
        {
            return false;
        }
    }
    return true;
}

bool UGamePlatformPCGEditorLibrary::CreateFoundationAssets(FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    const TArray<FString> TemplatePackages = BuildPackages(GetM0M1FoundationTemplateIds(), TEXT("Templates/"));
    const TArray<FString> SubgraphPackages = BuildPackages(GetM0M1FoundationSubgraphIds(), TEXT("Subgraphs/"));
    if (!EnsurePackagesAvailable(TemplatePackages, TEXT("Foundation模板"), Error) ||
        !EnsurePackagesAvailable(SubgraphPackages, TEXT("Foundation子图"), Error))
    {
        return false;
    }

    // 占用情况已统一预检；下面的两个入口仍会各自复核，以应对预检与保存之间的外部竞争。
    return CreateFoundationTemplateAssets(Error) && CreateFoundationSubgraphAssets(Error);
}

UPCGGraph* UGamePlatformPCGEditorLibrary::CreateDevelopmentRealizedGraphAsset(
    const FString& PackageName, UGamePlatformPCGProfileDefinition* Profile,
    UGamePlatformPCGMeshSetDefinition* MeshSet, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    // 防止模板/客户端误写正式发布关卡：提供固定开发验证目录，正式内容通过项目编辑器工具封装调用底层创作合同。
    const FString AllowedRoot = AssetRoot + TEXT("Realized/");
    if (!IsValid(Profile) || !IsValid(MeshSet) ||
        !PackageName.StartsWith(AllowedRoot) ||
        !FPackageName::IsValidLongPackageName(PackageName) ||
        PackageName.EndsWith(TEXT("/")) || IsOccupied(PackageName))
    {
        Error = TEXT("真实图实例需要有效已加载Definition，且目标必须是未占用的PCG开发Realized资产包。");
        return nullptr;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        Error = TEXT("无法在引擎中创建真实图实例资产包。");
        return nullptr;
    }

    UPCGGraph* Realized = GamePlatformPCGEditor::CreateFoundationRealizedGraph(
        Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)), *Profile, *MeshSet, Error);
    if (!Realized)
    {
        return nullptr;
    }

    // 用独立的临时副本检查即将保存的图；绝不把预览Graph偷偷写回调用者的原始Profile。
    UGamePlatformPCGProfileDefinition* Probe = DuplicateObject<UGamePlatformPCGProfileDefinition>(
        Profile, GetTransientPackage());
    if (!Probe)
    {
        Error = TEXT("无法创建Profile只读审查用副本，拒绝保存未校验的图。");
        return nullptr;
    }
    Probe->GraphReference = Realized;
    const FGamePlatformResult Valid = GamePlatformPCGInspection::ValidateApprovedGraph(*Probe);
    if (!Valid.IsSuccess())
    {
        Error = Valid.Code.ToString() + TEXT(": ") + Valid.Message;
        return nullptr;
    }

    if (!SaveNewAsset(*Realized, Error))
    {
        return nullptr;
    }
    return Realized;
}

bool UGamePlatformPCGEditorLibrary::ConfigureRealizedGraphSpatialMasks(
    UPCGGraph* RealizedGraph, AGamePlatformPCGWorldDirector* Director,
    int32 SubjectPriority, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    if (!IsValid(RealizedGraph) || !IsValid(Director) || RealizedGraph->bIsTemplate ||
        SubjectPriority < 0 || SubjectPriority > 100)
    {
        Error = TEXT("空间数据只能写入有效的非模板Graph Instance，层优先级必须在0..100。");
        return false;
    }

    TArray<FGamePlatformPCGSpatialMask> Masks;
    if (!Director->CollectSpatialMasks(Masks, Error))
    {
        return false;
    }

    UGamePlatformPCGSpatialCarveSettings* Target = nullptr;
    for (UPCGNode* Node : RealizedGraph->GetNodes())
    {
        if (auto* Settings = Node ? Cast<UGamePlatformPCGSpatialCarveSettings>(Node->GetSettings()) : nullptr)
        {
            if (Target)
            {
                Error = TEXT("Graph Instance包含重复SpatialCarve节点，拒绝半配置。");
                return false;
            }
            Target = Settings;
        }
    }
    if (!Target)
    {
        Error = TEXT("当前模板不支持空间Mask绑定；禁止静默忽略道路/地块排除。");
        return false;
    }

    RealizedGraph->Modify();
    Target->Modify();
    Target->SubjectPriority = SubjectPriority;
    Target->Masks = MoveTemp(Masks);
    RealizedGraph->MarkPackageDirty();
    return true;
}

bool UGamePlatformPCGEditorLibrary::CreatePCGPlacementBlueprints(
    const FString& RootPackagePath, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    // 通用平台编辑器不能假设“神兽联盟”包名，由项目调用者指定当前已注册的ContentPack挂载目录。
    // 固定在PCG/Blueprints，防止工具覆盖世界入口地图、其它插件资产或引擎内置资源。
    const bool bAllowed = RootPackagePath.EndsWith(TEXT("/PCG/Blueprints/")) &&
        !RootPackagePath.StartsWith(TEXT("/Engine/")) &&
        !RootPackagePath.StartsWith(TEXT("/Script/")) &&
        FPackageName::IsValidLongPackageName(RootPackagePath + TEXT("BP_PCG_Path"));
    if (!bAllowed)
    {
        Error = TEXT("PCG蓝图生成目录非法，必须使用已注册的项目ContentPack/PCG/Blueprints/挂载目录。");
        return false;
    }

    struct FBlueprintSpec
    {
        const TCHAR* Name;
        UClass* Parent;
        FName Domain;
        EGamePlatformPCGPrimitive Primitive;
        EGamePlatformPCGWorldStage Stage;
    };
    const TArray<FBlueprintSpec> Specifications =
    {
        {TEXT("BP_PCG_Forest"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::ForestCanopy, EGamePlatformPCGPrimitive::P1_Scatter, EGamePlatformPCGWorldStage::Scatter},
        {TEXT("BP_PCG_Rock"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::RockScatter, EGamePlatformPCGPrimitive::P1_Scatter, EGamePlatformPCGWorldStage::Scatter},
        {TEXT("BP_PCG_Road"), AGamePlatformPCGSplineActor::StaticClass(), FGamePlatformPCGDomainIds::RoadNetwork, EGamePlatformPCGPrimitive::P2_Linear, EGamePlatformPCGWorldStage::Networks},
        {TEXT("BP_PCG_Field"), AGamePlatformPCGPolygonActor::StaticClass(), FGamePlatformPCGDomainIds::AgriParcel, EGamePlatformPCGPrimitive::P4_Parcel, EGamePlatformPCGWorldStage::Parcels},
        {TEXT("BP_PCG_Crops"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::AgriCrop, EGamePlatformPCGPrimitive::P1_Scatter, EGamePlatformPCGWorldStage::Scatter},
        {TEXT("BP_PCG_Bridge"), AGamePlatformPCGConnectorActor::StaticClass(), FGamePlatformPCGDomainIds::BridgeSpan, EGamePlatformPCGPrimitive::P3_Connector, EGamePlatformPCGWorldStage::Connectors},
        {TEXT("BP_PCG_Exclusion"), AGamePlatformPCGExclusionActor::StaticClass(), FGamePlatformPCGDomainIds::GameplayExclusion, EGamePlatformPCGPrimitive::P0_Field, EGamePlatformPCGWorldStage::FieldRead},
        {TEXT("BP_PCG_WaterBank"), AGamePlatformPCGSplineActor::StaticClass(), FGamePlatformPCGDomainIds::WaterBank, EGamePlatformPCGPrimitive::P6_InterfaceBand, EGamePlatformPCGWorldStage::InterfaceBands},
        {TEXT("BP_PCG_Building"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::SettlementBuilding, EGamePlatformPCGPrimitive::P5_Assembly, EGamePlatformPCGWorldStage::Buildings},
        {TEXT("BP_PCG_Resource"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::PlayResource, EGamePlatformPCGPrimitive::P1_Scatter, EGamePlatformPCGWorldStage::GameplayAnchors},
        {TEXT("BP_PCG_Spawn"), AGamePlatformPCGVolumeActor::StaticClass(), FGamePlatformPCGDomainIds::PlaySpawn, EGamePlatformPCGPrimitive::P1_Scatter, EGamePlatformPCGWorldStage::GameplayAnchors}
    };

    TArray<FString> Packages;
    Packages.Reserve(Specifications.Num());
    for (const FBlueprintSpec& Spec : Specifications)
    {
        if (!FGamePlatformPCGDomainIds::IsKnown(Spec.Domain) || !Spec.Parent)
        {
            Error = TEXT("蓝图清单包含未经注册的领域或父类。");
            return false;
        }
        Packages.Add(RootPackagePath + Spec.Name);
    }
    if (!EnsurePackagesAvailable(Packages, TEXT("PCG Blueprint"), Error))
    {
        return false;
    }

    for (int32 I = 0; I < Specifications.Num(); ++I)
    {
        const FBlueprintSpec& Spec = Specifications[I];
        UPackage* Package = CreatePackage(*Packages[I]);
        UBlueprint* Blueprint = Package ? FKismetEditorUtilities::CreateBlueprint(
            Spec.Parent, Package, FName(Spec.Name), BPTYPE_Normal,
            UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
            TEXT("GamePlatformPCGEditor")) : nullptr;
        if (!Blueprint)
        {
            Error = FString::Printf(TEXT("无法创建真实PCG蓝图：%s"), Spec.Name);
            return false;
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        AGamePlatformPCGActorBase* Defaults = Blueprint->GeneratedClass
            ? Cast<AGamePlatformPCGActorBase>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        if (!Defaults)
        {
            Error = TEXT("PCG Blueprint缺少预期平台Actor生成类；未保存该资产。");
            return false;
        }

        // 仅设置父类已公开的通用领域/阶段与原语；项目专属网格、图/体验/天气实例由项目内容资产绑定。
        Defaults->Modify();
        Defaults->DomainId = Spec.Domain;
        Defaults->Primitive = Spec.Primitive;
        Defaults->WorldStage = Spec.Stage;
        Defaults->bLockedGraph = true;
        if (!SaveNewAsset(*Blueprint, Error))
        {
            return false;
        }
    }
    return true;
}

bool UGamePlatformPCGEditorLibrary::InspectProfileSource(UGamePlatformPCGProfileDefinition* Profile,
    FString& Fingerprint, TArray<FString>& Dependencies, FString& Error)
{
    check(IsInGameThread());
    Fingerprint.Reset(); Dependencies.Reset(); Error.Reset();
    if (!IsValid(Profile))
    {
        Error = TEXT("需要有效且已加载的配置");
        return false;
    }
    const FGamePlatformResult Validation = GamePlatformPCGInspection::ValidateApprovedGraph(*Profile);
    if (!Validation.IsSuccess())
    {
        Error = Validation.Code.ToString() + TEXT(": ") + Validation.Message;
        return false;
    }
    return GamePlatformPCGEditor::CalculateSourceFingerprint(*Profile, Fingerprint, Dependencies, Error);
}

bool UGamePlatformPCGEditorLibrary::ValidateProfileContract(
    UGamePlatformPCGProfileDefinition* Profile,
    FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    if (!IsValid(Profile))
    {
        Error = TEXT("需要有效且已加载的PCG Profile（配置）。");
        return false;
    }

    const FGamePlatformResult Result = GamePlatformPCGInspection::ValidateApprovedGraph(*Profile);
    if (!Result.IsSuccess())
    {
        Error = Result.Code.ToString() + TEXT(": ") + Result.Message;
        return false;
    }

    return true;
}

TArray<FName> UGamePlatformPCGEditorLibrary::GetKnownTemplateIds()
{
    return TArray<FName>(FGamePlatformPCGTemplateIds::All());
}

TArray<FName> UGamePlatformPCGEditorLibrary::GetKnownSubgraphIds()
{
    return TArray<FName>(FGamePlatformPCGSubgraphIds::All());
}
