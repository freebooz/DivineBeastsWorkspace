#include "Authoring/GamePlatformPCGEditorLibrary.h"
#include "Authoring/PCGDevelopmentGraph.h"
#include "Manifests/PCGSourceFingerprint.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Services/GamePlatformPCGInspection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
const FString AssetRoot = TEXT("/Game/Development/Foundation/PCG/");

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
