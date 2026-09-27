#include "Resolution/GamePlatformVFXCatalogRegistry.h"

namespace
{
constexpr int32 MaxResolveCacheEntries = 1024;
struct FResolverRank
{
    int32 SemanticTier = 0;
    int32 SemanticDepth = 0;
    int32 ContextTier = 0;
    int32 Specificity = 0;
    int32 Scope = 0;
    int64 Priority = 0;

    bool operator==(const FResolverRank& Other) const
    {
        return SemanticTier == Other.SemanticTier &&
            SemanticDepth == Other.SemanticDepth &&
            ContextTier == Other.ContextTier &&
            Specificity == Other.Specificity &&
            Scope == Other.Scope &&
            Priority == Other.Priority;
    }
};

bool IsBetterRank(const FResolverRank& A, const FResolverRank& B)
{
    if (A.SemanticTier != B.SemanticTier) return A.SemanticTier > B.SemanticTier;
    if (A.SemanticDepth != B.SemanticDepth) return A.SemanticDepth > B.SemanticDepth;
    if (A.ContextTier != B.ContextTier) return A.ContextTier > B.ContextTier;
    if (A.Specificity != B.Specificity) return A.Specificity > B.Specificity;
    if (A.Scope != B.Scope) return A.Scope > B.Scope;
    return A.Priority > B.Priority;
}

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
    FResolverRank BestRank;
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

            FResolverRank Rank;
            if (bDirectDefinition)
            {
                Rank.SemanticTier = 4;
                Rank.SemanticDepth = GetSemanticDepth(Entry.SemanticTag);
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

            Rank.Specificity = Entry.Specificity + Entry.RequiredContextTags.Num();
            Rank.Scope = static_cast<int32>(Entry.Scope);
            Rank.Priority = static_cast<int64>(Catalog->Priority) * 1000000LL + Entry.Priority;

            if (!bHasBest || IsBetterRank(Rank, BestRank))
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
        TEXT("%llu|%s|%s|%s|%s|%s|%d|%d"),
        Revision,
        *Request.SemanticTag.ToString(),
        *Request.DefinitionId.ToString(),
        *Request.ContextId.ToString(),
        *Request.ContextTags.ToStringSimple(false),
        *Request.PlatformId.ToString(),
        static_cast<int32>(Request.QualityTier),
        Request.bAllowFallback ? 1 : 0);
}

void FGamePlatformVFXCatalogRegistry::InvalidateCache()
{
    ++Revision;
    ResolveCache.Reset();
}
