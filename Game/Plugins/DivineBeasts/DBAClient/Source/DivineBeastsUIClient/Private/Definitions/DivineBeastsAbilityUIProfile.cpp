// 项目客户端技能显示主资产校验：只验证作者字段，不加载图标、不授予技能或解释权威数值。
// 创建介绍派生资产复用同一条目合同，但使用自己的主资产类型；错误输出供VM/作者工具诊断。
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
        // 保留名称、说明均拒绝纯空白的校验；合并不能削弱已有作者数据门禁。
        if (Entry.DisplayName.ToString().TrimStartAndEnd().IsEmpty() ||
            Entry.Description.ToString().TrimStartAndEnd().IsEmpty() || Entry.Icon.IsNull())
        {
            OutError = TEXT("技能显示条目必须配置非空白本地化名称、说明及图标软引用；内容存在不表示正式技能获批。");
            return false;
        }
    }
    if (!ArtPreviewAbilityIds.IsEmpty())
    {
        TSet<FName> PreviewIds;
        for (const FName Id : ArtPreviewAbilityIds)
        {
            if (!Unique.Contains(Id) || PreviewIds.Contains(Id))
            {
                OutError = TEXT("美术预览必须引用本Profile内唯一的技能条目。");
                return false;
            }
            PreviewIds.Add(Id);
        }
        if (PreviewIds.Num() != 8)
        {
            OutError = TEXT("美术预览必须明确配置八个槽位，不能从条目顺序猜测。");
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
