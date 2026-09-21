#include "Validation/GamePlatformDefinitionValidation.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"

FGamePlatformResult ResolveUniqueGamePlatformDefinitionSource(const FPrimaryAssetId& DefinitionId,
    FSoftObjectPath& OutSource, const UGamePlatformPrimaryDataAsset* EditorCandidate)
{
    check(IsInGameThread());
    OutSource.Reset();
    FGamePlatformId Parsed;
    if (DefinitionId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
        !FGamePlatformId::TryParse(DefinitionId.PrimaryAssetName.ToString(), Parsed))
        return FGamePlatformResult::Failure(TEXT("InvalidDefinitionId"), TEXT("主资产类型或规范逻辑身份非法。"));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets())
        return FGamePlatformResult::Failure(TEXT("AssetRegistryNotReady"), TEXT("源资产发现尚未完成，当前不能证明身份唯一。"));
    TArray<FAssetData> Sources;
    Registry.GetAssetsByClass(UGamePlatformPrimaryDataAsset::StaticClass()->GetClassPathName(), Sources, true);
    TArray<FSoftObjectPath> Matches;
    for (const FAssetData& Source : Sources)
    {
        const FSoftObjectPath Path = Source.GetSoftObjectPath();
        if (EditorCandidate && Path == FSoftObjectPath(EditorCandidate)) continue;
        FString LogicalText;
        FGamePlatformId SourceId;
        if (Source.GetTagValue(UGamePlatformPrimaryDataAsset::LogicalIdTag(), LogicalText) &&
            FGamePlatformId::TryParse(LogicalText, SourceId) && SourceId == Parsed) Matches.AddUnique(Path);
    }
    if (EditorCandidate && EditorCandidate->GetPrimaryAssetId() == DefinitionId) Matches.AddUnique(FSoftObjectPath(EditorCandidate));
    if (Matches.Num() == 0)
        return FGamePlatformResult::Failure(TEXT("MissingDefinition"), FString::Printf(TEXT("源注册表找不到定义：%s。"), *DefinitionId.ToString()));
    if (Matches.Num() != 1)
        return FGamePlatformResult::Failure(TEXT("DuplicateLogicalId"), FString::Printf(TEXT("逻辑身份%s被%d个源资产占用。"), *DefinitionId.ToString(), Matches.Num()));
    OutSource = Matches[0];
    return FGamePlatformResult::Success();
}
