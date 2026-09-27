#include "ContentPacks/DivineBeastsPresentationContentPack.h"

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
        if (DefinitionId.IsNone() || UniqueDefinitions.Contains(DefinitionId))
        {
            OutError = TEXT("LogicalPreloadDefinitionIds存在空值或重复值。");
            return false;
        }
        UniqueDefinitions.Add(DefinitionId);
    }
    return true;
}
