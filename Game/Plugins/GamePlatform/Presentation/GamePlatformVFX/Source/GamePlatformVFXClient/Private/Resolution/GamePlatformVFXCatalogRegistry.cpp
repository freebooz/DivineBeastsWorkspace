// 本文件属于GamePlatform平台层 GamePlatformVFX，负责资格过滤/确定性纯值排序；不拥有资源加载。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 客户端兼容目录解析：游戏线程资格过滤后按P13排序；缓存不拥有资源加载。
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Resolution/GamePlatformVFXCatalogScore.h"

namespace
{
constexpr int32 MaxResolveCacheEntries = 1024;
int32 GetSemanticDepth(const FGameplayTag& Tag)
{
    if (!Tag.IsValid()) return 0;
    TArray<FString> Parts;
    Tag.ToString().ParseIntoArray(Parts, TEXT("."), true);
    return Parts.Num();
}
}

FGamePlatformVFXRegistrationHandle FGamePlatformVFXCatalogRegistry::Register(UGamePlatformVFXCatalog* Catalog)
{
    FGamePlatformVFXRegistrationHandle Result;
    if (!IsValid(Catalog) || Catalogs.ContainsByPredicate([Catalog](const FRegisteredCatalog& Item)
        {
            return Item.Catalog.Get() == Catalog;
        }))
    {
        return Result;
    }

    Result.Id = FGuid::NewGuid();
    FRegisteredCatalog& Registered = Catalogs.AddDefaulted_GetRef();
    Registered.Handle = Result;
    Registered.Catalog = Catalog;
    InvalidateCache();
    return Result;
}

bool FGamePlatformVFXCatalogRegistry::Unregister(const FGamePlatformVFXRegistrationHandle& Handle)
{
    if (!Handle.IsValid())
    {
        return false;
    }

    const int32 Removed = Catalogs.RemoveAll([&Handle](const FRegisteredCatalog& Item)
    {
        return Item.Handle.Id == Handle.Id;
    });
    if (Removed > 0)
    {
        InvalidateCache();
        return true;
    }
    return false;
}

FGamePlatformVFXResolvedDefinition FGamePlatformVFXCatalogRegistry::Resolve(const FGamePlatformVFXRequest& Request) const
{
    const FString CacheKey = MakeCacheKey(Request);
    if (const FGamePlatformVFXResolvedDefinition* Cached = ResolveCache.Find(CacheKey))
    {
        return *Cached;
    }

    FGamePlatformVFXResolvedDefinition Best;
    Best.RegistryRevision = Revision;
    FGamePlatformVFXCatalogScore BestRank;
    bool bHasBest = false;

    for (const FRegisteredCatalog& Registered : Catalogs)
    {
        const UGamePlatformVFXCatalog* Catalog = Registered.Catalog.Get();
        if (!IsValid(Catalog))
        {
            continue;
        }

        for (const FGamePlatformVFXCatalogEntry& Entry : Catalog->Entries)
        {
            if (Entry.Definition.IsNull())
            {
                continue;
            }

            const bool bDirectDefinition = !Request.DefinitionId.IsNone();
            if (bDirectDefinition && Entry.DefinitionId != Request.DefinitionId)
            {
                continue;
            }

            FGamePlatformVFXCatalogScore Rank;
            if (bDirectDefinition)
            {
                Rank.SemanticTier = 4;
                Rank.SemanticDepth = 0; // 直接ID路径无语义层级，不让目录标签深度干扰同ID候选。
            }
            else if (Entry.SemanticTag == Request.SemanticTag && Entry.SemanticTag.IsValid())
            {
                Rank.SemanticTier = 3;
                Rank.SemanticDepth = GetSemanticDepth(Entry.SemanticTag);
            }
            else if (Request.bAllowFallback && Entry.SemanticTag.IsValid() &&
                     Request.SemanticTag.IsValid() && Request.SemanticTag.MatchesTag(Entry.SemanticTag))
            {
                Rank.SemanticTier = 2;
                Rank.SemanticDepth = GetSemanticDepth(Entry.SemanticTag);
            }
            else if (Request.bAllowFallback && !Entry.SemanticTag.IsValid() && Entry.bFallback)
            {
                Rank.SemanticTier = 1;
            }
            else
            {
                continue;
            }

            if (!Entry.PlatformId.IsNone() && Entry.PlatformId != Request.PlatformId)
            {
                continue;
            }
            if (!Entry.bAnyQuality && Entry.QualityTier != Request.QualityTier)
            {
                continue;
            }
            if (!Request.ContextTags.HasAll(Entry.RequiredContextTags) ||
                Request.ContextTags.HasAny(Entry.BlockedContextTags))
            {
                continue;
            }

            if (Entry.ContextId == Request.ContextId && !Entry.ContextId.IsNone())
            {
                Rank.ContextTier = 2;
            }
            else if (Entry.ContextId.IsNone() && Request.bAllowFallback)
            {
                Rank.ContextTier = 1;
            }
            else if (Entry.ContextId != Request.ContextId)
            {
                continue;
            }

            // P13只计六个已验证等值项；标签集合与历史ContextId只作资格，不影响层级。
            if ((!Entry.HeroDefinitionId.IsNone() && Entry.HeroDefinitionId != Request.HeroDefinitionId) ||
                (!Entry.AbilityId.IsNone() && Entry.AbilityId != Request.AbilityId) ||
                (!Entry.SkinId.IsNone() && Entry.SkinId != Request.SkinId) ||
                (!Entry.WorldId.IsNone() && Entry.WorldId != Request.WorldId)) continue;
            Rank.Specificity = (!Entry.HeroDefinitionId.IsNone() ? 1 : 0) +
                (!Entry.AbilityId.IsNone() ? 1 : 0) + (!Entry.SkinId.IsNone() ? 1 : 0) +
                (!Entry.WorldId.IsNone() ? 1 : 0) + (!Entry.PlatformId.IsNone() ? 1 : 0) +
                (!Entry.bAnyQuality ? 1 : 0);
            Rank.Scope = static_cast<int32>(Entry.Scope);
            // 两处显式Priority按64位相加；不保留百万权重这一隐式排序维度。
            Rank.Priority = static_cast<int64>(Catalog->Priority) + Entry.Priority;

            if (!bHasBest || Rank.IsBetterThan(BestRank))
            {
                bHasBest = true;
                BestRank = Rank;
                Best.Definition = Entry.Definition;
                Best.DefinitionId = Entry.DefinitionId;
                Best.bAmbiguous = false;
            }
            else if (Rank == BestRank)
            {
                Best.Definition.Reset();
                Best.DefinitionId = NAME_None;
                Best.bAmbiguous = true;
            }
        }
    }

    // 开放世界长会话会产生大量Context组合；缓存达到上限时整体清空，避免长期无界增长。
    if (ResolveCache.Num() >= MaxResolveCacheEntries)
    {
        ResolveCache.Reset();
    }
    ResolveCache.Add(CacheKey, Best);
    return Best;
}

void FGamePlatformVFXCatalogRegistry::Reset()
{
    Catalogs.Reset();
    InvalidateCache();
}

FString FGamePlatformVFXCatalogRegistry::MakeCacheKey(const FGamePlatformVFXRequest& Request) const
{
    return FString::Printf(
        TEXT("%llu|%s|%s|%s|%s|%s|%d|%d|%s|%s|%s|%s"),
        Revision,
        *Request.SemanticTag.ToString(),
        *Request.DefinitionId.ToString(),
        *Request.ContextId.ToString(),
        *Request.ContextTags.ToStringSimple(false),
        *Request.PlatformId.ToString(),
        static_cast<int32>(Request.QualityTier),
        Request.bAllowFallback ? 1 : 0,
        *Request.HeroDefinitionId.ToString(), *Request.AbilityId.ToString(),
        *Request.SkinId.ToString(), *Request.WorldId.ToString());
}

void FGamePlatformVFXCatalogRegistry::InvalidateCache()
{
    ++Revision;
    ResolveCache.Reset();
}
