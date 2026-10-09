// 本文件属于GamePlatform平台层 GamePlatformPresentation，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "GamePlatformPresentationCatalog.h"

namespace
{
    bool MatchesName(FName Query, FName Value)
    {
        return Query.IsNone() || Query == Value;
    }
}

bool FGamePlatformPresentationContextQuery::Matches(
    const FGamePlatformPresentationContext& Context) const
{
    return MatchesName(ProjectId, Context.ProjectId) &&
           MatchesName(HeroDefinitionId, Context.HeroDefinitionId) &&
           MatchesName(AbilityId, Context.AbilityId) &&
           MatchesName(SkinId, Context.SkinId) &&
           MatchesName(WorldId, Context.WorldId) &&
           MatchesName(ExperienceId, Context.ExperienceId) &&
           MatchesName(RegionId, Context.RegionId) &&
           MatchesName(ArenaModeId, Context.ArenaModeId) &&
           MatchesName(ContentPackId, Context.ContentPackId) &&
           MatchesName(PlatformId, Context.PlatformId) &&
           (QualityTier == EGamePlatformPresentationQualityTier::Unknown ||
            QualityTier == Context.QualityTier);
}

int32 FGamePlatformPresentationContextQuery::GetSpecificity() const
{
    // P13一期只计英雄/技能/皮肤/世界/平台/画质六个等值项；其他字段仍作资格。
    int32 Result = 0;
    Result += HeroDefinitionId.IsNone() ? 0 : 1;
    Result += AbilityId.IsNone() ? 0 : 1;
    Result += SkinId.IsNone() ? 0 : 1;
    Result += WorldId.IsNone() ? 0 : 1;
    Result += PlatformId.IsNone() ? 0 : 1;
    Result += QualityTier == EGamePlatformPresentationQualityTier::Unknown ? 0 : 1;
    return Result;
}

bool FGamePlatformPresentationCatalogFragment::IsValid() const
{
    constexpr int32 MaxEntriesPerFragment = 512;
    if (FragmentId.IsNone() ||
        Revision <= 0 ||
        Entries.IsEmpty() ||
        Entries.Num() > MaxEntriesPerFragment)
    {
        return false;
    }

    TSet<FName> Ids;
    for (const FGamePlatformPresentationCatalogEntry& Entry : Entries)
    {
        if (!Entry.IsValid() || Ids.Contains(Entry.EntryId))
        {
            return false;
        }
        Ids.Add(Entry.EntryId);
        if (Entry.Scope != Scope)
        {
            return false;
        }
    }
    return true;
}
