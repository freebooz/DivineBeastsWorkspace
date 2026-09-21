#include "Definitions/GamePlatformDefinitionBase.h"
#include "AssetRegistry/AssetRegistryTagsContext.h"

FPrimaryAssetType UGamePlatformPrimaryDataAsset::DefinitionAssetType() { return FPrimaryAssetType(TEXT("GamePlatformDefinition")); }
FName UGamePlatformPrimaryDataAsset::LogicalIdTag() { return TEXT("GamePlatformLogicalId"); }
FPrimaryAssetId UGamePlatformPrimaryDataAsset::GetPrimaryAssetId() const
{
    return LogicalId.IsValid() ? FPrimaryAssetId(DefinitionAssetType(), FName(*LogicalId.ToString())) : FPrimaryAssetId();
}
void UGamePlatformPrimaryDataAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
    Super::GetAssetRegistryTags(Context);
    Context.AddTag(FAssetRegistryTag(LogicalIdTag(), LogicalId.ToString(), FAssetRegistryTag::TT_Alphabetical));
}
int32 UGamePlatformDefinitionBase::GetMinimumReadableSchemaVersion() const { return 1; }
int32 UGamePlatformDefinitionBase::GetMaximumReadableSchemaVersion() const { return 1; }
FGamePlatformResult UGamePlatformDefinitionBase::ValidateDefinition() const
{
    if (!LogicalId.IsValid())
        return FGamePlatformResult::Failure(TEXT("InvalidLogicalId"), TEXT("定义缺少合法的稳定逻辑身份。"));
    const auto* ClassDefaults = GetClass()->GetDefaultObject<UGamePlatformDefinitionBase>();
    const int32 Minimum = ClassDefaults->GetMinimumReadableSchemaVersion();
    const int32 Maximum = ClassDefaults->GetMaximumReadableSchemaVersion();
    if (Minimum < 1 || Maximum < Minimum || DataVersion.SchemaVersion < Minimum || DataVersion.SchemaVersion > Maximum || DataVersion.ContentRevision < 1)
        return FGamePlatformResult::Failure(TEXT("UnsupportedDataVersion"), TEXT("定义结构版本不在该类CDO声明的可读范围内，或内容修订非法。"));
    TSet<FPrimaryAssetId> UniqueDependencies;
    for (const FPrimaryAssetId& Dependency : RequiredDefinitions)
    {
        FGamePlatformId Parsed;
        if (Dependency.PrimaryAssetType != DefinitionAssetType() || !FGamePlatformId::TryParse(Dependency.PrimaryAssetName.ToString(), Parsed))
            return FGamePlatformResult::Failure(TEXT("InvalidDependencyId"), TEXT("必需定义必须使用GamePlatformDefinition类型及合法逻辑身份。"));
        if (UniqueDependencies.Contains(Dependency))
            return FGamePlatformResult::Failure(TEXT("DuplicateDependency"), TEXT("同一定义不得重复声明同一依赖。"));
        UniqueDependencies.Add(Dependency);
    }
    return FGamePlatformResult::Success();
}
