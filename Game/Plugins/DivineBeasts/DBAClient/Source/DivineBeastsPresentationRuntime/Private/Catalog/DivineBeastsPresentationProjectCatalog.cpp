#include "Catalog/DivineBeastsPresentationProjectCatalog.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"

FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment()
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = TEXT("DBA.Presentation.Project.Default");
    Fragment.Revision = CatalogRevision;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::Session;

    FGamePlatformPresentationCatalogEntry Interaction;
    Interaction.EntryId = TEXT("DBA.Presentation.World.Interaction.Committed.Default");
    Interaction.SemanticTag =
        DivineBeastsPresentationTags::World_Interaction_Committed;
    Interaction.ContextQuery.ProjectId =
        FDivineBeastsProjectCatalog::GetProjectId();
    Interaction.ProviderChannel = TEXT("VFX");
    Interaction.DefinitionId =
        TEXT("Presentation.DBA.World.Interaction.Committed.Default");
    Interaction.Scope = EGamePlatformPresentationCatalogScope::Project;
    Interaction.Specificity = 1;
    Interaction.Priority = 0;
    Interaction.ContentRevision = TEXT("1");
    Fragment.Entries.Add(Interaction);

    FGamePlatformPresentationCatalogEntry Guidance;
    Guidance.EntryId = TEXT("DBA.Presentation.Village.Guidance.Ready.Default");
    Guidance.SemanticTag =
        DivineBeastsPresentationTags::Village_Guidance_Ready;
    Guidance.ContextQuery.ProjectId =
        FDivineBeastsProjectCatalog::GetProjectId();
    Guidance.ProviderChannel = TEXT("VFX");
    Guidance.DefinitionId =
        TEXT("Presentation.DBA.Village.Guidance.Ready.Default");
    Guidance.Scope = EGamePlatformPresentationCatalogScope::Project;
    Guidance.Specificity = 1;
    Guidance.Priority = 0;
    Guidance.ContentRevision = TEXT("1");
    Fragment.Entries.Add(Guidance);

    return Fragment;
}
