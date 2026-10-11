// 第三层物种语义与逻辑定义映射；平台机制不包含神兽联盟物种和资源路径。
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"

FGameplayTag FDivineBeastsPresentationProjectCatalog::GetFaunaTag(const FName SpeciesId)
{
    if (SpeciesId==TEXT("ButterflyAzure")) return DivineBeastsPresentationTags::Fauna_ButterflyAzure;
    if (SpeciesId==TEXT("FishSilver")) return DivineBeastsPresentationTags::Fauna_FishSilver;
    return {};
}

FGamePlatformPresentationCatalogFragment FDivineBeastsPresentationProjectCatalog::BuildFaunaFragment()
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId=TEXT("DBA.Presentation.Environment.Fauna");
    Fragment.Revision=1; Fragment.Scope=EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId=FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope=EGamePlatformPresentationContextScope::World;
    for (const TCHAR* Species:{TEXT("ButterflyAzure"),TEXT("FishSilver")})
    {
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId=FName(*(FString(TEXT("DBA.Fauna."))+Species));
        Entry.SemanticTag=GetFaunaTag(FName(Species));
        Entry.ContextQuery.ProjectId=FDivineBeastsProjectCatalog::GetProjectId();
        Entry.ProviderChannel=TEXT("VFX");
        Entry.DefinitionId=FName(*(FString(TEXT("presentation.dba.environment.fauna."))+FString(Species).ToLower()+TEXT("@1")));
        Entry.Scope=EGamePlatformPresentationCatalogScope::Project;
        Entry.Specificity=1; Entry.Priority=0; Entry.ContentRevision=TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    }
    return Fragment;
}
