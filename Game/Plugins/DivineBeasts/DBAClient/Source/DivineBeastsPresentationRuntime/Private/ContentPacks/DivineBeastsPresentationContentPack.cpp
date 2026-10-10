// 本文件属于DivineBeasts项目层 DivineBeastsPresentationRuntime，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "Types/GamePlatformId.h"

bool FDivineBeastsPresentationContentPackFragment::IsValid(
    FString& OutError) const
{
    if (ContentPackId.IsNone() || Revision <= 0)
    {
        OutError = TEXT("ContentPackId不能为空且Revision必须大于0。");
        return false;
    }
    if (!CatalogFragment.IsValid())
    {
        OutError = TEXT("ContentPack CatalogFragment无效。");
        return false;
    }
    if (CatalogFragment.Scope !=
        EGamePlatformPresentationCatalogScope::ContentPack)
    {
        OutError = TEXT("ContentPack目录作用域必须为ContentPack。");
        return false;
    }
    if (CatalogFragment.OwnerScopeId != ContentPackId)
    {
        OutError = TEXT("Catalog OwnerScopeId必须等于ContentPackId。");
        return false;
    }
    if (CatalogFragment.Revision != Revision)
    {
        OutError = TEXT("Catalog Revision必须与ContentPack Revision一致。");
        return false;
    }
    if (CatalogFragment.LifecycleScope != LifecycleScope)
    {
        OutError = TEXT("Catalog LifecycleScope必须与ContentPack一致。");
        return false;
    }

    TSet<FName> UniqueDefinitions;
    for (const FName DefinitionId : LogicalPreloadDefinitionIds)
    {
        FGamePlatformId Id;
        if (!FGamePlatformId::TryParse(DefinitionId.ToString(), Id) || UniqueDefinitions.Contains(FName(*Id.ToString())))
        {
            OutError = TEXT("LogicalPreloadDefinitionIds存在空值或重复值。");
            return false;
        }
        UniqueDefinitions.Add(FName(*Id.ToString()));
    }
    for (const auto& Entry : CatalogFragment.Entries)
    {
        FGamePlatformId Id;
        if (!FGamePlatformId::TryParse(Entry.DefinitionId.ToString(), Id))
        { OutError = TEXT("目录DefinitionId必须是namespace.name@version合法逻辑身份。"); return false; }
    }
    return true;
}
