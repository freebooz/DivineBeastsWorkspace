<<<<<<< HEAD
// 项目客户端技能显示主资产校验：只验证作者字段，不加载图标、不授予技能或解释权威数值。
// 创建介绍派生资产复用同一条目合同，但使用自己的主资产类型；错误输出供VM/作者工具诊断。
=======
// 项目客户端Profile只提供英雄/技能显示资料，保留既有PrimaryAsset身份，不承载玩法权威。
// 校验只读作者字段并返回中文错误，不加载图标、不修补或代填作者内容；由Data完成后的UI消费方调用。
>>>>>>> 0963476cd210192af4c0e11459fa707dd95bff99
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Types/GamePlatformId.h"

FPrimaryAssetId UDivineBeastsAbilityUIProfile::GetPrimaryAssetId() const
{
    return HeroDefinitionId.IsNone() ? FPrimaryAssetId()
        : FPrimaryAssetId(FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), HeroDefinitionId);
}

bool UDivineBeastsAbilityUIProfile::ValidateProfile(FString& OutError) const
{
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
    {
        OutError = TEXT("技能表现配置缺少十二生肖合法英雄编号。");
        return false;
    }
    if (Entries.IsEmpty() || Entries.Num() > 64)
    {
        OutError = TEXT("技能表现配置必须包含1～64个实际技能条目。");
        return false;
    }
    TSet<FName> Unique;
    for (const FDivineBeastsAbilityUIEntry& Entry : Entries)
    {
        FGamePlatformId Parsed;
        if (Entry.AbilityId.IsNone() ||
            !FGamePlatformId::TryParse(Entry.AbilityId.ToString(), Parsed) ||
            Unique.Contains(Entry.AbilityId))
        {
            OutError = TEXT("技能表现配置的稳定技能编号不合法或出现重复。");
            return false;
        }
        Unique.Add(Entry.AbilityId);
<<<<<<< HEAD
        if (Entry.DisplayName.ToString().TrimStartAndEnd().IsEmpty() ||
            Entry.Description.ToString().TrimStartAndEnd().IsEmpty() || Entry.Icon.IsNull())
=======
        // 名称/图标存在不能替代可读技能说明；空白文本同样无法形成玩家可见介绍。
        if (Entry.Description.ToString().TrimStartAndEnd().IsEmpty())
        {
            OutError = TEXT("正式技能条目必须配置非空白的本地化技能说明。");
            return false;
        }
        if (Entry.DisplayName.IsEmpty() || Entry.Icon.IsNull())
>>>>>>> 0963476cd210192af4c0e11459fa707dd95bff99
        {
            OutError = TEXT("技能显示条目必须配置非空白本地化名称、说明及图标软引用；内容存在不表示正式技能获批。");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

const FDivineBeastsAbilityUIEntry* UDivineBeastsAbilityUIProfile::FindEntry(
    FName AbilityId) const
{
    return Entries.FindByPredicate([AbilityId](const FDivineBeastsAbilityUIEntry& Entry)
    {
        return Entry.AbilityId == AbilityId;
    });
}
