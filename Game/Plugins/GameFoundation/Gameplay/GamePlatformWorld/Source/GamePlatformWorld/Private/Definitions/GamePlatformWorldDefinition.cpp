#include "Definitions/GamePlatformWorldDefinition.h"
#include "Misc/PackageName.h"

FGamePlatformResult UGamePlatformWorldDefinition::ValidateDefinition() const
{
    check(IsInGameThread());
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess()) { return BaseResult; }
    const FSoftObjectPath Path = MapIdentity.ToSoftObjectPath();
    if (Path.IsNull() || !Path.IsAsset() || !FPackageName::IsValidObjectPath(Path.ToString()))
    { return FGamePlatformResult::Failure(TEXT("InvalidMapIdentity"), TEXT("世界定义必须引用合法的顶层地图对象，不能使用空值或子对象。")); }
    if (!FMath::IsFinite(ReadinessTimeoutSeconds) || ReadinessTimeoutSeconds <= 0.f)
    { return FGamePlatformResult::Failure(TEXT("InvalidReadinessTimeout"), TEXT("世界就绪截止秒数必须是有限正数。")); }
    const bool bEmptyExperience = DefaultExperienceId.Namespace.IsEmpty() && DefaultExperienceId.Name.IsEmpty() && DefaultExperienceId.LogicalVersion == 1;
    if (!bEmptyExperience && !DefaultExperienceId.IsValid())
    { return FGamePlatformResult::Failure(TEXT("InvalidDefaultExperienceId"), TEXT("默认体验必须留为完整默认空值，或填写合法的中立逻辑身份。")); }
    TSet<FPrimaryAssetId> Seen;
    for (const FPrimaryAssetId& Region : Regions)
    {
        FGamePlatformId Parsed;
        if (Region.PrimaryAssetType != DefinitionAssetType() || !FGamePlatformId::TryParse(Region.PrimaryAssetName.ToString(), Parsed) || Region == GetPrimaryAssetId())
        { return FGamePlatformResult::Failure(TEXT("InvalidRegionId"), TEXT("区域必须是合法的GamePlatformDefinition身份，且不能引用当前世界定义自身。")); }
        if (Seen.Contains(Region))
        { return FGamePlatformResult::Failure(TEXT("DuplicateRegion"), TEXT("同一个世界不能重复声明同一逻辑区域。")); }
        Seen.Add(Region);
        if (!RequiredDefinitions.Contains(Region))
        { return FGamePlatformResult::Failure(TEXT("RegionDependencyMissing"), TEXT("每个必需区域都必须显式加入RequiredDefinitions，以进入真实Data租约依赖闭包。")); }
    }
    return FGamePlatformResult::Success();
}
