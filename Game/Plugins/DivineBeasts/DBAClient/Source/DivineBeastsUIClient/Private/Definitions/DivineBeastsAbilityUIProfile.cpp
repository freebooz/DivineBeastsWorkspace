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
        if (Entry.DisplayName.IsEmpty() || Entry.Icon.IsNull())
        {
            OutError = TEXT("正式技能条目必须配置本地化名称及真实图标软引用。");
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
