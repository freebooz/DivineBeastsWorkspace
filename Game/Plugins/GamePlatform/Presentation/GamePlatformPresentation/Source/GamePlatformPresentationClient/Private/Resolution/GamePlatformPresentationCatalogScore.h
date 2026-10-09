// 目录候选排序的私有纯值策略；只比较资格过滤后的候选，不持有注册或世界。
#pragma once
struct FGamePlatformPresentationCatalogScore
{
    // 精确为2，允许的父回退为1；父距离越小越近，只有同语义层级才比较作用域。
    int SemanticRank = 0;
    int SemanticDistance = 0;
    int Specificity = 0;
    int Scope = 0;
    int Priority = 0;

    bool IsBetterThan(const FGamePlatformPresentationCatalogScore& Other) const
    {
        if (SemanticRank != Other.SemanticRank)
        {
            return SemanticRank > Other.SemanticRank;
        }
        if (SemanticDistance != Other.SemanticDistance)
        {
            return SemanticDistance < Other.SemanticDistance;
        }
        if (Scope != Other.Scope)
        {
            return Scope > Other.Scope;
        }
        if (Specificity != Other.Specificity)
        {
            return Specificity > Other.Specificity;
        }
        return Priority > Other.Priority;
    }

    bool IsEquivalentTo(const FGamePlatformPresentationCatalogScore& Other) const
    {
        return SemanticRank == Other.SemanticRank &&
               SemanticDistance == Other.SemanticDistance &&
               Specificity == Other.Specificity &&
               Scope == Other.Scope &&
               Priority == Other.Priority;
    }
};
