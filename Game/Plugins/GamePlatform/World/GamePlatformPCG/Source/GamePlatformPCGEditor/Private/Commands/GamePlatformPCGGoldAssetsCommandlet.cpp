#include "Commands/GamePlatformPCGGoldAssetsCommandlet.h"
#include "Commands/GamePlatformPCGGoldMapAuthoring.h"

#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "GamePlatformPCGLog.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "PCGGraph.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr const TCHAR* DevelopmentRoot = TEXT("/Game/Development/Foundation/PCG/");

/** 固定17种编辑器开发数据。目录与类型只能从审批清单得出，不接受外部任意类或保存路径。 */
struct FGoldDefinitionSpec
{
    const TCHAR* Name;
    UClass* Type;
};

TArray<FGoldDefinitionSpec> GoldDefinitionSpecs()
{
    return {
        {TEXT("Exec"), UGamePlatformPCGExecPresetDefinition::StaticClass()},
        {TEXT("Priority"), UGamePlatformPCGPriorityTableDefinition::StaticClass()},
        {TEXT("MeshCanopy"), UGamePlatformPCGMeshSetDefinition::StaticClass()},
        {TEXT("MeshCrop"), UGamePlatformPCGMeshSetDefinition::StaticClass()},
        {TEXT("MeshFence"), UGamePlatformPCGMeshSetDefinition::StaticClass()},
        {TEXT("PolicyForest"), UGamePlatformPCGSpawnPolicyDefinition::StaticClass()},
        {TEXT("PolicyCrop"), UGamePlatformPCGSpawnPolicyDefinition::StaticClass()},
        {TEXT("LayerCanopy"), UGamePlatformPCGLayerDefinition::StaticClass()},
        {TEXT("LayerFloor"), UGamePlatformPCGLayerDefinition::StaticClass()},
        {TEXT("LayerCrop"), UGamePlatformPCGLayerDefinition::StaticClass()},
        {TEXT("Biome"), UGamePlatformPCGBiomePresetDefinition::StaticClass()},
        {TEXT("Exclusion"), UGamePlatformPCGExclusionPresetDefinition::StaticClass()},
        {TEXT("Road"), UGamePlatformPCGRoadProfileDefinition::StaticClass()},
        {TEXT("Enclosure"), UGamePlatformPCGEnclosureProfileDefinition::StaticClass()},
        {TEXT("Parcel"), UGamePlatformPCGParcelPresetDefinition::StaticClass()},
        {TEXT("Crop"), UGamePlatformPCGCropProfileDefinition::StaticClass()},
        {TEXT("Connector"), UGamePlatformPCGConnectorCatalogDefinition::StaticClass()}
    };
}

bool PackageAvailable(const FString& PackageName, FString& Error)
{
    if (!PackageName.StartsWith(DevelopmentRoot) ||
        FindPackage(nullptr, *PackageName) ||
        FPackageName::DoesPackageExist(PackageName))
    {
        Error = TEXT("PCG GoldLevel目标不在专用开发目录或已被占用：") + PackageName;
        return false;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    if (IFileManager::Get().FileExists(*Filename))
    {
        Error = TEXT("真实文件已存在，拒绝覆盖：") + Filename;
        return false;
    }
    return true;
}

/** 仅保存已校验过、位于独占开发包的真实UObject；任何失败都保留现场。 */
bool SaveNewGoldAsset(UObject* Asset, FString& Error)
{
    if (!IsValid(Asset) || !Asset->HasAllFlags(RF_Public | RF_Standalone))
    {
        Error = TEXT("PCG金标准待保存对象无效或缺少资产身份标记。");
        return false;
    }
    UPackage* Package = Asset->GetOutermost();
    const FString PackageName = Package ? Package->GetName() : FString();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    if (!PackageName.StartsWith(DevelopmentRoot) || IFileManager::Get().FileExists(*Filename))
    {
        Error = TEXT("PCG金标准保存范围非法或已有文件：") + PackageName;
        return false;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FAssetRegistryModule::AssetCreated(Asset);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
    {
        Error = TEXT("PCG金标准真实UE包保存失败：") + Filename;
        return false;
    }
    return true;
}

bool CreateDefinitions(FString& Error)
{
    check(IsInGameThread());
    const TArray<FGoldDefinitionSpec> Specs = GoldDefinitionSpecs();
    TArray<FString> Packages;
    Packages.Reserve(Specs.Num());
    for (const FGoldDefinitionSpec& Spec : Specs)
    {
        const FString PackageName = FString(DevelopmentRoot) + TEXT("Definitions/DA_PCGGold_") + Spec.Name;
        if (!PackageAvailable(PackageName, Error)) { return false; }
        Packages.Add(PackageName);
    }

    // 首先创建并校验完整17个对象，避免第一个配置不合法时已保存一半正式资源。
    TArray<UGamePlatformDefinitionBase*> Assets;
    Assets.Reserve(Specs.Num());
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        UPackage* Package = CreatePackage(*Packages[Index]);
        const FString AssetName = FString(TEXT("DA_PCGGold_")) + Specs[Index].Name;
        UGamePlatformDefinitionBase* Asset = NewObject<UGamePlatformDefinitionBase>(
            Package, Specs[Index].Type, FName(*AssetName), RF_Public | RF_Standalone);
        if (!Asset || !UGamePlatformPCGEditorLibrary::ConfigureGoldDevelopmentDefinition(
                Asset, FName(Specs[Index].Name), Error))
        {
            Error = FString::Printf(TEXT("Gold Definition %s创建或强类型校验失败：%s"),
                Specs[Index].Name, *Error);
            return false;
        }
        Assets.Add(Asset);
    }

    for (int32 Index = 0; Index < Assets.Num(); ++Index)
    {
        if (!SaveNewGoldAsset(Assets[Index], Error)) { return false; }
        UE_LOG(LogGamePlatformPCG, Display, TEXT("PCG Gold Definition 已保存：%s"), *Packages[Index]);
    }
    UE_LOG(LogGamePlatformPCG, Display, TEXT("PCG Gold 17项Definition已通过UE5.8 C++创作并落盘。"));
    return true;
}

bool CreateRealized(FString& Error)
{
    check(IsInGameThread());
    struct FRealizedSpec
    {
        const TCHAR* Name;
        const TCHAR* Template;
        const TCHAR* MeshSet;
    };
    const TArray<FRealizedSpec> Specs = {
        {TEXT("Canopy"), TEXT("TPL_ScatterSurface"), TEXT("MeshCanopy")},
        {TEXT("Rock"), TEXT("TPL_ScatterSurface"), TEXT("MeshCanopy")},
        {TEXT("Crop"), TEXT("TPL_CropField"), TEXT("MeshCrop")}
    };

    // 对6项目标先完整检查占用与输入依赖，再动任何资产。
    struct FLoaded
    {
        FString ProfilePackage;
        FString GraphPackage;
        UPCGGraph* Template = nullptr;
        UGamePlatformPCGMeshSetDefinition* MeshSet = nullptr;
    };
    TArray<FLoaded> Loaded;
    Loaded.Reserve(Specs.Num());
    for (const FRealizedSpec& Spec : Specs)
    {
        FLoaded Entry;
        Entry.ProfilePackage = FString(DevelopmentRoot) + TEXT("Profiles/DA_PCGGold_") + Spec.Name;
        Entry.GraphPackage = FString(DevelopmentRoot) + TEXT("Realized/PCG_Gold_") + Spec.Name;
        if (!PackageAvailable(Entry.ProfilePackage, Error) || !PackageAvailable(Entry.GraphPackage, Error))
        {
            return false;
        }
        const FString TemplateObjectPath = FString(DevelopmentRoot) + TEXT("Templates/") +
            Spec.Template + TEXT(".") + Spec.Template;
        const FString MeshObjectPath = FString(DevelopmentRoot) + TEXT("Definitions/DA_PCGGold_") +
            Spec.MeshSet + TEXT(".DA_PCGGold_") + Spec.MeshSet;
        Entry.Template = LoadObject<UPCGGraph>(nullptr, *TemplateObjectPath);
        Entry.MeshSet = LoadObject<UGamePlatformPCGMeshSetDefinition>(nullptr, *MeshObjectPath);
        if (!IsValid(Entry.Template) || !IsValid(Entry.MeshSet) ||
            !Entry.MeshSet->ValidateDefinition().IsSuccess())
        {
            Error = TEXT("Gold真实图缺少有效且已保存的Foundation模板或网格集合：") +
                TemplateObjectPath + TEXT(" / ") + MeshObjectPath;
            return false;
        }
        Loaded.Add(Entry);
    }

    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        const FLoaded& Entry = Loaded[Index];
        UPackage* Package = CreatePackage(*Entry.ProfilePackage);
        const FString AssetName = FString(TEXT("DA_PCGGold_")) + Specs[Index].Name;
        UGamePlatformPCGProfileDefinition* Profile = NewObject<UGamePlatformPCGProfileDefinition>(
            Package, FName(*AssetName), RF_Public | RF_Standalone);
        if (!Profile || !UGamePlatformPCGEditorLibrary::ConfigureGoldDevelopmentProfile(
                Profile, FName(Specs[Index].Name), Entry.Template, Entry.MeshSet, Error))
        {
            Error = TEXT("Gold Profile强类型配置失败：") + Error;
            return false;
        }
        UPCGGraph* Graph = UGamePlatformPCGEditorLibrary::CreateDevelopmentRealizedGraphAsset(
            Entry.GraphPackage, Profile, Entry.MeshSet, Error);
        if (!IsValid(Graph) ||
            !UGamePlatformPCGEditorLibrary::FinalizeGoldDevelopmentProfile(Profile, Graph, Error) ||
            !SaveNewGoldAsset(Profile, Error))
        {
            Error = TEXT("Gold真实Spawner图/Profile保存失败：") + Error;
            return false;
        }
        UE_LOG(LogGamePlatformPCG, Display, TEXT("PCG Gold带Spawner的图与Profile已保存：%s / %s"),
            *Entry.GraphPackage, *Entry.ProfilePackage);
    }
    return true;
}
}

UGamePlatformPCGGoldAssetsCommandlet::UGamePlatformPCGGoldAssetsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UGamePlatformPCGGoldAssetsCommandlet::Main(const FString& Params)
{
    FString Error;
    FString Stage;
    if (!FParse::Value(*Params, TEXT("Stage="), Stage))
    {
        UE_LOG(LogGamePlatformPCG, Error,
            TEXT("必须显式使用-Stage=Definitions或-Stage=Realized；禁止不明确的开发资产写入。"));
        return 1;
    }
    bool bSuccess = false;
    if (Stage.Equals(TEXT("Definitions"), ESearchCase::IgnoreCase))
    {
        bSuccess = CreateDefinitions(Error);
    }
    else if (Stage.Equals(TEXT("Realized"), ESearchCase::IgnoreCase))
    {
        bSuccess = CreateRealized(Error);
    }
    else if (Stage.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
    {
        bSuccess = GamePlatformPCGGoldMap::Create(Error);
    }
    else if (Stage.Equals(TEXT("RepairMap"), ESearchCase::IgnoreCase))
    {
        bSuccess = GamePlatformPCGGoldMap::Repair(Error);
    }
    else if (Stage.Equals(TEXT("Verify"), ESearchCase::IgnoreCase))
    {
        bSuccess = GamePlatformPCGGoldMap::Verify(Error);
    }
    else
    {
        Error = TEXT("仅支持Definitions/Realized/Map/RepairMap/Verify五个固定GoldLevel阶段。");
    }
    if (!bSuccess)
    {
        UE_LOG(LogGamePlatformPCG, Error, TEXT("PCG金标准原生UE资产创建失败：%s"), *Error);
        return 1;
    }
    return 0;
}
