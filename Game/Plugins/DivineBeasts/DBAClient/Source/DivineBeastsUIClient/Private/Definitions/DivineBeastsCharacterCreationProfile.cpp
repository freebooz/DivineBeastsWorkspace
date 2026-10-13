// 项目客户端创建介绍的数据身份和字段门禁；没有播放器、授权入口或独立资产加载器。
#include "Definitions/DivineBeastsCharacterCreationProfile.h"
#include "Types/GamePlatformId.h"

FPrimaryAssetId UDivineBeastsCharacterCreationProfile::GetPrimaryAssetId() const
{
    return HeroDefinitionId.IsNone() ? FPrimaryAssetId()
        : FPrimaryAssetId(TEXT("DivineBeastsCharacterCreationProfile"), HeroDefinitionId);
}

bool UDivineBeastsCharacterCreationProfile::ValidateCreationProfile(FString& OutError) const
{
    if (!ValidateProfile(OutError)) return false;
    if (Entries.Num() != 5)
    {
        OutError = TEXT("首期创建介绍必须包含作者按普攻、被动、主动一、主动二、终极顺序配置的五项内容。");
        return false;
    }
    const FString HeroSuffix = HeroDefinitionId.ToString().RightChop(FString(TEXT("Hero.Zodiac.")).Len());
    const FString AbilityPrefix = HeroSuffix.ToLower() + TEXT("_");
    for (const FDivineBeastsAbilityUIEntry& Entry : Entries)
    {
        FGamePlatformId Parsed;
        if (!FGamePlatformId::TryParse(Entry.AbilityId.ToString(), Parsed) ||
            Parsed.Namespace != TEXT("dba.ability") || !Parsed.Name.StartsWith(AbilityPrefix))
        {
            OutError = TEXT("创建技能介绍必须使用同一英雄的既有项目技能内容身份，不能跨英雄拼接。");
            return false;
        }
        const bool bDevelopmentIdentity = Parsed.Name.Contains(TEXT("_dev_"));
        if ((bDevelopmentIdentity && !bDescriptionsAreDraft) ||
            (bDescriptionsAreDraft && !Entry.Description.ToString().Contains(TEXT("开发候选，未作为正式技能批准"))))
        {
            OutError = TEXT("开发候选说明必须逐项明确“开发候选，未作为正式技能批准”，不能当作正式技能介绍。");
            return false;
        }
        const FString ExpectedIconRoot = FString(TEXT("/DBAHeroPack_")) + HeroSuffix + TEXT("/UI/Abilities/");
        if (!Entry.Icon.ToSoftObjectPath().GetLongPackageName().StartsWith(ExpectedIconRoot))
        {
            OutError = TEXT("创建介绍图标只能软引用本英雄内容包的UI/Abilities，不得引入开发Gameplay资源。");
            return false;
        }
    }
    OutError.Reset();
    return true;
}
