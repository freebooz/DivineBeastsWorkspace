// GamePlatformSurfaceEditor跨游戏地形LayerInfo真实生成与校验。
// 输入校验发生在写入前；无伪.uasset、无运行时LayerInfo创建、无项目专属层名硬编码。
#include "Commands/GamePlatformSurfaceTerrainLayersCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeEditTypes.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSurfaceTerrain, Log, All);

UGamePlatformSurfaceTerrainLayersCommandlet::UGamePlatformSurfaceTerrainLayersCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

namespace
{
    bool IsSafeName(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Len() > 64)
        {
            return false;
        }
        for (const TCHAR Char : Value)
        {
            if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
            {
                return false;
            }
        }
        return true;
    }

    bool ValidateLayer(ULandscapeLayerInfoObject* Asset, const FName ExpectedLayer)
    {
        return IsValid(Asset) &&
            Asset->GetLayerName() == ExpectedLayer &&
            Asset->GetBlendMethod() == ELandscapeTargetLayerBlendMethod::FinalWeightBlending;
    }
}

int32 UGamePlatformSurfaceTerrainLayersCommandlet::Main(const FString& Params)
{
    check(IsInGameThread());

    FString Root, NamesString, AssetPrefix, LayerPrefix;
    const bool bValidateOnly = FParse::Param(*Params, TEXT("ValidateOnly"));
    if (!FParse::Value(*Params, TEXT("LayerRoot="), Root) ||
        !FParse::Value(*Params, TEXT("LayerIds="), NamesString))
    {
        UE_LOG(LogGamePlatformSurfaceTerrain, Error,
            TEXT("必须指定-LayerRoot=/项目挂载点/... 与-LayerIds=Grass,Rock等受控图层标识。"));
        return 1;
    }
    if (!FParse::Value(*Params, TEXT("AssetPrefix="), AssetPrefix))
    {
        AssetPrefix = TEXT("LI_");
    }
    if (!FParse::Value(*Params, TEXT("LayerNamePrefix="), LayerPrefix))
    {
        LayerPrefix = TEXT("LS_");
    }

    Root.RemoveFromEnd(TEXT("/"));
    TArray<FString> Names;
    NamesString.ParseIntoArray(Names, TEXT(","), /*CullEmpty=*/true);
    // 最多16个图层，不开放向Engine、Script和平台自身挂载路径写内容。
    if (Names.IsEmpty() || Names.Num() > 16 ||
        !Root.StartsWith(TEXT("/")) || Root.StartsWith(TEXT("/Engine/")) ||
        Root.StartsWith(TEXT("/Script/")) ||
        !IsSafeName(AssetPrefix) || !IsSafeName(LayerPrefix))
    {
        UE_LOG(LogGamePlatformSurfaceTerrain, Error,
            TEXT("地形图层根路径、数量或命名前缀非法：%s"), *Root);
        return 1;
    }

    TSet<FString> UniqueNames;
    TArray<FString> Packages;
    TArray<FName> ExpectedNames;
    for (const FString& Layer : Names)
    {
        const FString Raw = Layer.TrimStartAndEnd();
        const FString LayerName = LayerPrefix + Raw;
        const FString AssetName = AssetPrefix + Raw;
        const FString PackageName = Root + TEXT("/") + AssetName;
        if (!IsSafeName(Raw) || UniqueNames.Contains(Raw) ||
            !IsSafeName(LayerName) || !IsSafeName(AssetName) ||
            !FPackageName::IsValidLongPackageName(PackageName))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error,
                TEXT("无效、重复或越界的地表Layer名称：%s"), *Raw);
            return 1;
        }
        UniqueNames.Add(Raw);
        Packages.Add(PackageName);
        ExpectedNames.Add(FName(*LayerName));
    }

    // 全部路径预检，防止处理到中途才发现已有不兼容资产。
    for (int32 Index=0; Index<Packages.Num(); ++Index)
    {
        const FString& Pkg=Packages[Index];
        if (!FPackageName::DoesPackageExist(Pkg))
        {
            if (bValidateOnly)
            {
                UE_LOG(LogGamePlatformSurfaceTerrain, Error,
                    TEXT("LayerInfo未创建，验证失败：%s"), *Pkg);
                return 1;
            }
            continue;
        }
        const FString ObjectPath = Pkg + TEXT(".") + FPackageName::GetLongPackageAssetName(Pkg);
        ULandscapeLayerInfoObject* Asset = LoadObject<ULandscapeLayerInfoObject>(nullptr, *ObjectPath);
        if (!ValidateLayer(Asset, ExpectedNames[Index]))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error,
                TEXT("LayerInfo路径已存在但图层名或Weight Blend合同不匹配，不允许覆盖：%s"), *Pkg);
            return 1;
        }
    }

    int32 Created=0;
    for (int32 Index=0; Index<Packages.Num(); ++Index)
    {
        const FString& PkgName=Packages[Index];
        if (FPackageName::DoesPackageExist(PkgName))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Display, TEXT("TERRAIN_LAYER_INFO_EXISTS %s"), *PkgName);
            continue;
        }
        UPackage* Pkg=CreatePackage(*PkgName);
        if (!IsValid(Pkg))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error, TEXT("LayerInfo CreatePackage失败：%s"), *PkgName);
            return 1;
        }
        const FName ObjectName(*FPackageName::GetLongPackageAssetName(PkgName));
        ULandscapeLayerInfoObject* Info=NewObject<ULandscapeLayerInfoObject>(
            Pkg, ObjectName, RF_Public | RF_Standalone | RF_Transactional);
        if (!IsValid(Info))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error, TEXT("未能创建真实ULandscapeLayerInfoObject：%s"), *PkgName);
            return 1;
        }
        Info->SetLayerName(ExpectedNames[Index], /*bModify=*/false);
        Info->SetBlendMethod(ELandscapeTargetLayerBlendMethod::FinalWeightBlending, /*bModify=*/false);
        if (!ValidateLayer(Info, ExpectedNames[Index]))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error, TEXT("LayerInfo原生字段设置后检查失败：%s"), *PkgName);
            return 1;
        }
        FAssetRegistryModule::AssetCreated(Info);
        Pkg->MarkPackageDirty();
        const FString File=FPackageName::LongPackageNameToFilename(
            PkgName, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags=RF_Public | RF_Standalone;
        Args.SaveFlags=SAVE_NoError;
        if (!UPackage::SavePackage(Pkg, Info, *File, Args))
        {
            UE_LOG(LogGamePlatformSurfaceTerrain, Error, TEXT("真实LayerInfo保存失败：%s"), *File);
            return 1;
        }
        ++Created;
        UE_LOG(LogGamePlatformSurfaceTerrain, Display,
            TEXT("TERRAIN_LAYER_INFO_CREATED package=%s name=%s"),
            *PkgName, *ExpectedNames[Index].ToString());
    }

    UE_LOG(LogGamePlatformSurfaceTerrain, Display,
        TEXT("TERRAIN_LAYER_INFO_CONTRACT_PASS count=%d created=%d mode=%s"),
        Packages.Num(), Created, bValidateOnly ? TEXT("verify") : TEXT("create"));
    return 0;
}
