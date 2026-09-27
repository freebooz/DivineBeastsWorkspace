#include "Audit/GamePlatformAssetAudit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"

bool FGamePlatformAssetAudit::AuditFolder(
    FName PackagePath,
    TArray<FGamePlatformAssetAuditRow>& OutRows,
    FString& OutError)
{
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = AssetRegistryModule.Get();

    TArray<FAssetData> Assets;
    if (!Registry.GetAssetsByPath(PackagePath, Assets, true, true))
    {
        OutError = FString::Printf(TEXT("Asset Registry无法读取目录：%s"), *PackagePath.ToString());
        return false;
    }

    OutRows.Reset();
    OutRows.Reserve(Assets.Num());

    for (const FAssetData& Asset : Assets)
    {
        FGamePlatformAssetAuditRow Row;
        Row.PackageName = Asset.PackageName.ToString();
        Row.AssetName = Asset.AssetName.ToString();
        Row.ClassName = Asset.AssetClassPath.GetAssetName().ToString();
        Asset.GetTagValue(TEXT("PrimaryAssetType"), Row.PrimaryAssetType);

        TArray<FString> ChunkStrings;
        for (const int32 ChunkId : Asset.GetChunkIDs())
        {
            ChunkStrings.Add(FString::FromInt(ChunkId));
        }
        Row.ChunkIds = FString::Join(ChunkStrings, TEXT("|"));

        FString DiskSizeText;
        if (Asset.GetTagValue(TEXT("DiskSize"), DiskSizeText))
        {
            Row.DiskSize = FCString::Atoi64(*DiskSizeText);
        }

        OutRows.Add(MoveTemp(Row));
    }

    return true;
}

bool FGamePlatformAssetAudit::AuditDependencies(
    FName PackageName,
    TArray<FName>& OutDependencies,
    FString& OutError)
{
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& Registry = AssetRegistryModule.Get();

    OutDependencies.Reset();
    if (!Registry.GetDependencies(
        PackageName,
        OutDependencies,
        UE::AssetRegistry::EDependencyCategory::Package,
        UE::AssetRegistry::FDependencyQuery()))
    {
        OutError = FString::Printf(TEXT("无法读取依赖：%s"), *PackageName.ToString());
        return false;
    }

    return true;
}

bool FGamePlatformAssetAudit::WriteCsv(
    const TArray<FGamePlatformAssetAuditRow>& Rows,
    const FString& OutputPath,
    FString& OutError)
{
    FString Csv = TEXT("PackageName,AssetName,ClassName,PrimaryAssetType,ChunkIds,DiskSize\n");

    auto Escape = [](const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *Escaped);
    };

    for (const FGamePlatformAssetAuditRow& Row : Rows)
    {
        Csv += FString::Printf(
            TEXT("%s,%s,%s,%s,%s,%lld\n"),
            *Escape(Row.PackageName),
            *Escape(Row.AssetName),
            *Escape(Row.ClassName),
            *Escape(Row.PrimaryAssetType),
            *Escape(Row.ChunkIds),
            Row.DiskSize);
    }

    if (!FFileHelper::SaveStringToFile(Csv, *OutputPath))
    {
        OutError = FString::Printf(TEXT("写入CSV失败：%s"), *OutputPath);
        return false;
    }

    return true;
}
