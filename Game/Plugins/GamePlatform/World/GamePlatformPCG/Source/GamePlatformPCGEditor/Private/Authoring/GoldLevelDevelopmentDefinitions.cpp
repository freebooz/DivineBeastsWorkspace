#include "Authoring/GamePlatformPCGEditorLibrary.h"

#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "PCGGraph.h"
#include "Services/GamePlatformPCGTemplateContract.h"

namespace
{
const FString GoldDefinitionsRoot = TEXT("/Game/Development/Foundation/PCG/Definitions/");
const FString GoldProfilesRoot = TEXT("/Game/Development/Foundation/PCG/Profiles/");

/** Python编辑器无法写入EditDefaultsOnly；限制为单一开发合同、未保存的真实DataAsset。 */
bool ValidateNewGoldAsset(const UGamePlatformDefinitionBase* Asset,
    const FString& PackageName, FString& Error)
{
    if (!IsValid(Asset) || !IsValid(Asset->GetOutermost()) ||
        Asset->GetOutermost()->GetName() != PackageName ||
        FPackageName::DoesPackageExist(PackageName) ||
        Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        Error = TEXT("仅可初始化未保存、未发布的GoldLevel开发定义，拒绝覆盖现有包。");
        return false;
    }
    return true;
}

FPrimaryAssetId GoldDefinitionId(FName ShortName)
{
    const FString Name = FString::Printf(TEXT("foundation.pcg_gold_%s@1"),
        *ShortName.ToString().ToLower());
    return FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*Name));
}

template<typename T>
T* RequireExactType(UGamePlatformDefinitionBase* Asset, FString& Error)
{
    if (!IsValid(Asset) || Asset->GetClass() != T::StaticClass())
    {
        Error = FString::Printf(TEXT("GoldLevel资产的真实C++类型必须是%s。"),
            *T::StaticClass()->GetName());
        return nullptr;
    }
    return CastChecked<T>(Asset);
}

/** 已存在的开发数据依赖只登记逻辑主资产身份，不绕过GamePlatformData统一租约。 */
void AddGoldDependency(UGamePlatformDefinitionBase& Asset, FName Name)
{
    const FPrimaryAssetId Id = GoldDefinitionId(Name);
    if (!Asset.RequiredDefinitions.Contains(Id))
    {
        Asset.RequiredDefinitions.Add(Id);
    }
}
}

bool UGamePlatformPCGEditorLibrary::ConfigureGoldDevelopmentDefinition(
    UGamePlatformDefinitionBase* Definition, FName ShortName, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    const FString PackageName = GoldDefinitionsRoot + TEXT("DA_PCGGold_") + ShortName.ToString();
    if (ShortName.IsNone() || !ValidateNewGoldAsset(Definition, PackageName, Error))
    {
        return false;
    }

    // 这里只是强类型编辑器数据构建，不从GamePlatform读取额外网络权限或同步加载客户端资源。
    if (!FGamePlatformId::TryParse(
        FString::Printf(TEXT("foundation.pcg_gold_%s@1"), *ShortName.ToString().ToLower()),
        Definition->LogicalId))
    {
        Error = TEXT("GoldLevel开发Definition的规范逻辑身份无效。");
        return false;
    }
    Definition->DataVersion.SchemaVersion = 1;
    Definition->DataVersion.ContentRevision = 1;
    Definition->RequiredDefinitions.Reset();

    if (ShortName == TEXT("Exec"))
    {
        if (!RequireExactType<UGamePlatformPCGExecPresetDefinition>(Definition, Error)) { return false; }
    }
    else if (ShortName == TEXT("Priority"))
    {
        // PriorityTable默认由平台规则构造正式17层优先级，开发资产不再维护重复版本。
        if (!RequireExactType<UGamePlatformPCGPriorityTableDefinition>(Definition, Error)) { return false; }
    }
    else if (ShortName == TEXT("MeshCanopy") || ShortName == TEXT("MeshCrop") ||
             ShortName == TEXT("MeshFence"))
    {
        UGamePlatformPCGMeshSetDefinition* MeshSet =
            RequireExactType<UGamePlatformPCGMeshSetDefinition>(Definition, Error);
        if (!MeshSet) { return false; }
        const TCHAR* Path =
            ShortName == TEXT("MeshCanopy") ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") :
            ShortName == TEXT("MeshCrop") ? TEXT("/Engine/BasicShapes/Sphere.Sphere") :
            TEXT("/Engine/BasicShapes/Cube.Cube");
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
        if (!IsValid(Mesh))
        {
            Error = TEXT("UE5.8无法加载GoldLevel基础测试网格，不创建空MeshSet。");
            return false;
        }
        MeshSet->Entries.Reset();
        FGamePlatformPCGMeshSetEntry& Entry = MeshSet->Entries.AddDefaulted_GetRef();
        Entry.Mesh = Mesh;
        Entry.Weight = 1.0f;
        Entry.bStaticCollision = ShortName == TEXT("MeshFence");
    }
    else if (ShortName == TEXT("PolicyForest") || ShortName == TEXT("PolicyCrop"))
    {
        UGamePlatformPCGSpawnPolicyDefinition* Policy =
            RequireExactType<UGamePlatformPCGSpawnPolicyDefinition>(Definition, Error);
        if (!Policy) { return false; }
        Policy->Density = ShortName == TEXT("PolicyForest") ? 0.45f : 0.75f;
        Policy->SelfPruneDistanceCm = ShortName == TEXT("PolicyForest") ? 130.0f : 30.0f;
    }
    else if (ShortName == TEXT("LayerCanopy") || ShortName == TEXT("LayerFloor") ||
             ShortName == TEXT("LayerCrop"))
    {
        UGamePlatformPCGLayerDefinition* Layer =
            RequireExactType<UGamePlatformPCGLayerDefinition>(Definition, Error);
        if (!Layer) { return false; }
        Layer->LayerName = ShortName == TEXT("LayerCanopy") ? FName(TEXT("Canopy")) :
            ShortName == TEXT("LayerFloor") ? FName(TEXT("GroundCover")) : FName(TEXT("Crop"));
        const FName MeshName = ShortName == TEXT("LayerCrop") ? FName(TEXT("MeshCrop")) : FName(TEXT("MeshCanopy"));
        const FName PolicyName = ShortName == TEXT("LayerCrop") ? FName(TEXT("PolicyCrop")) : FName(TEXT("PolicyForest"));
        Layer->MeshSetId = GoldDefinitionId(MeshName);
        Layer->SpawnPolicyId = GoldDefinitionId(PolicyName);
        AddGoldDependency(*Layer, MeshName);
        AddGoldDependency(*Layer, PolicyName);
    }
    else if (ShortName == TEXT("Biome"))
    {
        UGamePlatformPCGBiomePresetDefinition* Biome =
            RequireExactType<UGamePlatformPCGBiomePresetDefinition>(Definition, Error);
        if (!Biome) { return false; }
        Biome->BiomeId = FName(TEXT("GoldBiome"));
        Biome->LayerIds = {
            GoldDefinitionId(TEXT("LayerCanopy")),
            GoldDefinitionId(TEXT("LayerFloor")),
            GoldDefinitionId(TEXT("LayerCrop"))
        };
        for (const FName Name : {FName(TEXT("LayerCanopy")), FName(TEXT("LayerFloor")), FName(TEXT("LayerCrop"))})
        {
            AddGoldDependency(*Biome, Name);
        }
    }
    else if (ShortName == TEXT("Exclusion"))
    {
        UGamePlatformPCGExclusionPresetDefinition* Exclusion =
            RequireExactType<UGamePlatformPCGExclusionPresetDefinition>(Definition, Error);
        if (!Exclusion) { return false; }
        Exclusion->SourceId = FName(TEXT("ManualLock"));
        Exclusion->Strength = 1.0f;
    }
    else if (ShortName == TEXT("Road"))
    {
        if (!RequireExactType<UGamePlatformPCGRoadProfileDefinition>(Definition, Error)) { return false; }
    }
    else if (ShortName == TEXT("Enclosure"))
    {
        UGamePlatformPCGEnclosureProfileDefinition* Enclosure =
            RequireExactType<UGamePlatformPCGEnclosureProfileDefinition>(Definition, Error);
        if (!Enclosure) { return false; }
        Enclosure->PostMeshSetId = GoldDefinitionId(TEXT("MeshFence"));
        Enclosure->SpanMeshSetId = GoldDefinitionId(TEXT("MeshFence"));
        AddGoldDependency(*Enclosure, TEXT("MeshFence"));
    }
    else if (ShortName == TEXT("Parcel"))
    {
        UGamePlatformPCGParcelPresetDefinition* Parcel =
            RequireExactType<UGamePlatformPCGParcelPresetDefinition>(Definition, Error);
        if (!Parcel) { return false; }
        Parcel->EdgeEnclosureProfileId = GoldDefinitionId(TEXT("Enclosure"));
        AddGoldDependency(*Parcel, TEXT("Enclosure"));
    }
    else if (ShortName == TEXT("Crop"))
    {
        UGamePlatformPCGCropProfileDefinition* Crop =
            RequireExactType<UGamePlatformPCGCropProfileDefinition>(Definition, Error);
        if (!Crop) { return false; }
        Crop->SeasonalMeshSetIds = {GoldDefinitionId(TEXT("MeshCrop"))};
        AddGoldDependency(*Crop, TEXT("MeshCrop"));
    }
    else if (ShortName == TEXT("Connector"))
    {
        UGamePlatformPCGConnectorCatalogDefinition* Catalog =
            RequireExactType<UGamePlatformPCGConnectorCatalogDefinition>(Definition, Error);
        if (!Catalog) { return false; }
        Catalog->Entries.Reset();
        for (const EGamePlatformPCGConnectorType Type :
            {EGamePlatformPCGConnectorType::Gate, EGamePlatformPCGConnectorType::Bridge})
        {
            FGamePlatformPCGConnectorCatalogEntry& Item = Catalog->Entries.AddDefaulted_GetRef();
            Item.Type = Type;
            Item.ItemId = Type == EGamePlatformPCGConnectorType::Gate ?
                FName(TEXT("GoldGate")) : FName(TEXT("GoldBridge"));
            Item.ContentDefinitionId = GoldDefinitionId(TEXT("MeshFence"));
        }
        AddGoldDependency(*Catalog, TEXT("MeshFence"));
    }
    else
    {
        Error = TEXT("未知GoldLevel Definition类型；拒绝通过字符串初始化任意平台数据。");
        return false;
    }

    const FGamePlatformResult Result = Definition->ValidateDefinition();
    if (!Result.IsSuccess())
    {
        Error = FString::Printf(TEXT("GoldLevel %s Definition合同失败：%s：%s"),
            *ShortName.ToString(), *Result.Code.ToString(), *Result.Message);
        return false;
    }
    return true;
}

bool UGamePlatformPCGEditorLibrary::FinalizeGoldDevelopmentProfile(
    UGamePlatformPCGProfileDefinition* Profile, UPCGGraph* RealizedGraph, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    if (!IsValid(Profile) || !IsValid(RealizedGraph) ||
        Profile->TemplateId.IsNone() || !Profile->MeshSetDefinitionId.IsValid())
    {
        Error = TEXT("GoldLevel Profile只能关联有效的项目静态Spawner生成图。");
        return false;
    }

    const FString ProfileName = Profile->GetName();
    const FString ProfilePackage = GoldProfilesRoot + ProfileName;
    const FString TargetName = ProfileName.Replace(TEXT("DA_PCGGold_"), TEXT("PCG_Gold_"));
    const FString TargetPackage = TEXT("/Game/Development/Foundation/PCG/Realized/") + TargetName;
    if (!ProfileName.StartsWith(TEXT("DA_PCGGold_")) ||
        !ValidateNewGoldAsset(Profile, ProfilePackage, Error) ||
        RealizedGraph->GetOutermost()->GetName() != TargetPackage ||
        RealizedGraph->bIsTemplate)
    {
        if (Error.IsEmpty()) { Error = TEXT("GoldLevel实际图/配置路径不匹配或仍为共享模板。"); }
        return false;
    }

    Profile->Modify();
    Profile->GraphReference = RealizedGraph;
    const FGamePlatformResult Result = Profile->ValidateDefinition();
    if (!Result.IsSuccess())
    {
        Error = TEXT("GoldLevel图绑定后Profile定义校验失败：") + Result.Code.ToString();
        return false;
    }
    Profile->MarkPackageDirty();
    return true;
}

bool UGamePlatformPCGEditorLibrary::ConfigureGoldDevelopmentProfile(
    UGamePlatformPCGProfileDefinition* Profile, FName ShortName,
    UPCGGraph* FoundationTemplate, UGamePlatformPCGMeshSetDefinition* MeshSet,
    FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    const FString Package = GoldProfilesRoot + TEXT("DA_PCGGold_") + ShortName.ToString();
    if ((ShortName != TEXT("Canopy") && ShortName != TEXT("Rock") && ShortName != TEXT("Crop")) ||
        !ValidateNewGoldAsset(Profile, Package, Error) || !IsValid(FoundationTemplate) ||
        !IsValid(MeshSet) || !MeshSet->ValidateDefinition().IsSuccess())
    {
        if (Error.IsEmpty()) { Error = TEXT("GoldLevel Profile必须引用已加载且有效的模板和MeshSet定义。"); }
        return false;
    }

    const FName ExpectedTemplate = ShortName == TEXT("Crop")
        ? FGamePlatformPCGTemplateIds::CropField : FGamePlatformPCGTemplateIds::ScatterSurface;
    const FName ExpectedMesh = ShortName == TEXT("Crop") ?
        FName(TEXT("MeshCrop")) : FName(TEXT("MeshCanopy"));
    const FString ExpectedTemplatePath = FString::Printf(
        TEXT("/Game/Development/Foundation/PCG/Templates/%s"),
        *ExpectedTemplate.ToString());
    if (!FoundationTemplate->bIsTemplate || FoundationTemplate->GetOutermost()->GetName() != ExpectedTemplatePath ||
        MeshSet->GetPrimaryAssetId() != GoldDefinitionId(ExpectedMesh))
    {
        Error = TEXT("GoldLevel Profile的模板或MeshSet身份与固定开发合同不一致。");
        return false;
    }

    if (!FGamePlatformId::TryParse(
        FString::Printf(TEXT("foundation.pcg_gold_profile_%s@1"), *ShortName.ToString().ToLower()),
        Profile->LogicalId) ||
        !FGamePlatformId::TryParse(TEXT("foundation.region_a@1"), Profile->RegionId))
    {
        Error = TEXT("GoldLevel Profile/Region的规范逻辑身份无效。");
        return false;
    }

    Profile->DataVersion.SchemaVersion = 1;
    Profile->DataVersion.ContentRevision = 1;
    Profile->TemplateId = ExpectedTemplate;
    Profile->TemplateVersion = 1;
    Profile->GraphReference = FoundationTemplate;
    Profile->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
    Profile->OutputUsage = EGamePlatformPCGOutputUsage::Cosmetic;
    Profile->MinimumOutputs = 0;
    Profile->MeshSetDefinitionId = MeshSet->GetPrimaryAssetId();
    Profile->RequiredDefinitions = {Profile->MeshSetDefinitionId};

    const FGamePlatformResult Result = Profile->ValidateDefinition();
    if (!Result.IsSuccess())
    {
        Error = FString::Printf(TEXT("GoldLevel Profile合同失败：%s：%s"),
            *Result.Code.ToString(), *Result.Message);
        return false;
    }
    return true;
}
