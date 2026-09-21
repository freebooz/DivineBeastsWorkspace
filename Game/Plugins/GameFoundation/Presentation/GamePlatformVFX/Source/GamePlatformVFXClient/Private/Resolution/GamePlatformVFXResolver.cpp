#include "Resolution/GamePlatformVFXResolver.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Catalogs/GamePlatformVFXCatalog.h"

namespace
{
    int32 ScopeWeight(EGamePlatformVFXCatalogScope Scope)
    {
        switch (Scope)
        {
        case EGamePlatformVFXCatalogScope::ContentPack: return 400000;
        case EGamePlatformVFXCatalogScope::Project:     return 300000;
        case EGamePlatformVFXCatalogScope::Moba:        return 200000;
        case EGamePlatformVFXCatalogScope::Platform:    return 100000;
        default: return 0;
        }
    }

    bool Matches(const FGamePlatformVFXRequest& Request, const FGamePlatformVFXCatalogEntry& Entry, bool& bOutExactSemantic, int32& OutDepth)
    {
        bOutExactSemantic = false;
        OutDepth = 0;

        if (!Entry.SemanticTag.IsValid() || !Entry.DefinitionId.IsValid())
        {
            return false;
        }

        if (Request.SemanticTag.MatchesTagExact(Entry.SemanticTag))
        {
            bOutExactSemantic = true;
            OutDepth = 1000;
        }
        else if (Entry.bAllowParentSemanticFallback && Request.SemanticTag.MatchesTag(Entry.SemanticTag))
        {
            OutDepth = Request.SemanticTag.MatchesTagDepth(Entry.SemanticTag);
        }
        else
        {
            return false;
        }

        if (!Request.ContextTags.HasAll(Entry.RequiredContextTags))
        {
            return false;
        }

        if (Request.ContextTags.HasAny(Entry.BlockedContextTags))
        {
            return false;
        }

        return true;
    }
}

FGamePlatformVFXResolveResult FGamePlatformVFXResolver::Resolve(
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXCatalogRegistry& Registry)
{
    FGamePlatformVFXResolveResult Result;

    if (Request.ExplicitDefinitionId.IsValid())
    {
        Result.bSuccess = true;
        Result.DefinitionId = Request.ExplicitDefinitionId;
        return Result;
    }

    if (!Request.SemanticTag.IsValid())
    {
        Result.Error = TEXT("SemanticTag 无效，且未提供 ExplicitDefinitionId。");
        return Result;
    }

    TArray<UGamePlatformVFXCatalog*> Catalogs;
    Registry.GetCatalogs(Catalogs);

    int64 BestScore = MIN_int64;
    FPrimaryAssetId BestId;
    bool bFound = false;
    bool bAmbiguous = false;

    for (const UGamePlatformVFXCatalog* Catalog : Catalogs)
    {
        if (!Catalog) { continue; }

        for (const FGamePlatformVFXCatalogEntry& Entry : Catalog->Entries)
        {
            bool bExactSemantic = false;
            int32 SemanticDepth = 0;
            if (!Matches(Request, Entry, bExactSemantic, SemanticDepth))
            {
                continue;
            }

            const int32 Specificity = Entry.RequiredContextTags.Num();
            const int64 Score =
                static_cast<int64>(ScopeWeight(Entry.Scope)) +
                static_cast<int64>(bExactSemantic ? 50000 : SemanticDepth * 100) +
                static_cast<int64>(Specificity * 1000) +
                static_cast<int64>(Entry.Priority);

            if (!bFound || Score > BestScore)
            {
                bFound = true;
                bAmbiguous = false;
                BestScore = Score;
                BestId = Entry.DefinitionId;
            }
            else if (Score == BestScore && Entry.DefinitionId != BestId)
            {
                bAmbiguous = true;
            }
        }
    }

    if (!bFound)
    {
        Result.Error = FString::Printf(TEXT("未找到 VFX 映射：%s"), *Request.SemanticTag.ToString());
        return Result;
    }

    if (bAmbiguous)
    {
        Result.bAmbiguous = true;
        Result.Error = FString::Printf(TEXT("VFX Catalog 存在同优先级歧义：%s"), *Request.SemanticTag.ToString());
        return Result;
    }

    Result.bSuccess = true;
    Result.DefinitionId = BestId;
    return Result;
}
