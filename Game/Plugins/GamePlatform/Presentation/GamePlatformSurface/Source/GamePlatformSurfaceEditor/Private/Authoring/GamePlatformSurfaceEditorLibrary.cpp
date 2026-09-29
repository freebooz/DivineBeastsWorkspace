// Surface核心MPC的真实UE资产生成实现；只允许编辑器显式调用。
#include "Authoring/GamePlatformSurfaceEditorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Validation/GamePlatformSurfaceAssetContract.h"

namespace
{
bool SaveNewAsset(UObject& Asset, FString& OutError)
{
    UPackage* Package = Asset.GetOutermost();
    const FString Filename =
        FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    if (FPackageName::DoesPackageExist(Package->GetName()))
    {
        OutError = TEXT("保存前目标已存在，拒绝覆盖：") + Package->GetName();
        return false;
    }

    FAssetRegistryModule::AssetCreated(&Asset);
    Package->MarkPackageDirty();

    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, &Asset, *Filename, Args))
    {
        OutError = TEXT("Surface核心MPC保存失败；保留现场供人工检查，不自动删除或覆盖：") + Filename;
        return false;
    }
    return true;
}
}

bool GamePlatformSurfaceEditor::EnsureCoreParameterCollection(
    const bool bValidateOnly,
    FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();

    const FString& PackageName = FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionPackageName();
    const FSoftObjectPath ObjectPath = FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionObjectPath();

    if (FPackageName::DoesPackageExist(PackageName) || ObjectPath.ResolveObject())
    {
        UMaterialParameterCollection* Existing =
            Cast<UMaterialParameterCollection>(ObjectPath.TryLoad());
        if (!IsValid(Existing))
        {
            OutError = TEXT("Surface核心MPC路径已被占用但无法按MaterialParameterCollection加载：") + ObjectPath.ToString();
            return false;
        }
        return FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(*Existing, OutError);
    }

    if (bValidateOnly)
    {
        OutError = TEXT("Surface核心MPC尚未创建：") + ObjectPath.ToString();
        return false;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!IsValid(Package))
    {
        OutError = TEXT("无法创建Surface核心MPC包：") + PackageName;
        return false;
    }

    const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
    UMaterialParameterCollection* Collection = NewObject<UMaterialParameterCollection>(
        Package,
        AssetName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!IsValid(Collection))
    {
        OutError = TEXT("无法创建Surface核心MaterialParameterCollection对象。");
        return false;
    }

    Collection->ScalarParameters.Reset();
    Collection->VectorParameters.Reset();
    for (const TPair<FName, float>& Pair : FGamePlatformSurfaceAssetContract::GetRequiredScalarParameters())
    {
        FCollectionScalarParameter Parameter;
        Parameter.ParameterName = Pair.Key;
        Parameter.DefaultValue = Pair.Value;
        Collection->ScalarParameters.Add(Parameter);
    }

    Collection->StateId = FGuid::NewGuid();

    if (!FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(*Collection, OutError))
    {
        return false;
    }

    return SaveNewAsset(*Collection, OutError);
}
