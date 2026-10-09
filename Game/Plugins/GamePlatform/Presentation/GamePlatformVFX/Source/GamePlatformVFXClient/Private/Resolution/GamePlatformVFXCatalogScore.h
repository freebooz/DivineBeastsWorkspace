// 本文件属于GamePlatform平台层 GamePlatformVFX，负责资格过滤/确定性纯值排序；不拥有资源加载。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// VFX旧目录兼容入口的纯值排序；资格由Registry验证，本策略不访问资产或世界。
#pragma once
#include <cstdint>
struct FGamePlatformVFXCatalogScore
{
    // 直接ID=4、精确=3、父语义=2、无语义回退=1；同层父语义深度越大越近。
    int SemanticTier = 0;
    int SemanticDepth = 0;
    // 旧ContextId只作资格字段，保留值以兼容测试输入，不参与P13排序。
    int ContextTier = 0;
    int Specificity = 0;
    int Scope = 0;
    std::int64_t Priority = 0;
    bool operator==(const FGamePlatformVFXCatalogScore& Other) const
    {
        return SemanticTier == Other.SemanticTier && SemanticDepth == Other.SemanticDepth &&
            Specificity == Other.Specificity &&
            Scope == Other.Scope && Priority == Other.Priority;
    }
    bool IsBetterThan(const FGamePlatformVFXCatalogScore& Other) const
    {
        if (SemanticTier != Other.SemanticTier) return SemanticTier > Other.SemanticTier;
        if (SemanticDepth != Other.SemanticDepth) return SemanticDepth > Other.SemanticDepth;
        if (Scope != Other.Scope) return Scope > Other.Scope;
        if (Specificity != Other.Specificity) return Specificity > Other.Specificity;
        return Priority > Other.Priority;
    }
};
