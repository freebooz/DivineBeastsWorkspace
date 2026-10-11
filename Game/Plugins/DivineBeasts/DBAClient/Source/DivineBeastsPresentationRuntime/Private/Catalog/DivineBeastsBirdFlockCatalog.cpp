// 神兽联盟项目层：环境鸟类资源身份与表现语义的唯一映射，不包含Niagara或渲染执行。
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"

FGameplayTag FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(FName SpeciesId)
{
    if(SpeciesId==TEXT("Seagull")) return DivineBeastsPresentationTags::BirdFlock_Seagull;
    if(SpeciesId==TEXT("Swallow")) return DivineBeastsPresentationTags::BirdFlock_Swallow;
    if(SpeciesId==TEXT("Eagle")) return DivineBeastsPresentationTags::BirdFlock_Eagle;
    return {}; // 未知物种不静默变成海鸥，避免误导策划和掩盖资源配置错误。
}

FGamePlatformPresentationCatalogFragment FDivineBeastsPresentationProjectCatalog::BuildBirdFlockFragment()
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId=TEXT("DBA.Presentation.Environment.BirdFlock");
    Fragment.Revision=1;
    Fragment.Scope=EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId=FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope=EGamePlatformPresentationContextScope::World;
    for(const TCHAR* Species:{TEXT("Seagull"),TEXT("Swallow"),TEXT("Eagle")})
    {
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId=FName(*(FString(TEXT("DBA.BirdFlock."))+Species));
        Entry.SemanticTag=GetBirdFlockTag(FName(Species));
        Entry.ContextQuery.ProjectId=FDivineBeastsProjectCatalog::GetProjectId();
        Entry.ProviderChannel=TEXT("VFX");
        Entry.DefinitionId=FName(*(FString(TEXT("presentation.dba.environment.birdflock."))+FString(Species).ToLower()+TEXT("@1")));
        Entry.Scope=EGamePlatformPresentationCatalogScope::Project;
        Entry.Specificity=1; Entry.Priority=0; Entry.ContentRevision=TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    }
    return Fragment;
}
