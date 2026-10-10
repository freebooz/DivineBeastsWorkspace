// 本文件属于DivineBeasts项目层 DivineBeastsPresentationRuntime，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 项目默认目录合同：逻辑ID合法不代表已交付资产，客户端真实Data预载成功才发布。
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
        TEXT("presentation.dba.world.interaction.committed.default@1");
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
        TEXT("presentation.dba.village.guidance.ready.default@1");
    Guidance.Scope = EGamePlatformPresentationCatalogScope::Project;
    Guidance.Specificity = 1;
    Guidance.Priority = 0;
    Guidance.ContentRevision = TEXT("1");
    Fragment.Entries.Add(Guidance);

    return Fragment;
}

namespace
{
/** 只构造无资产路径的中立项目天气目录，真实可见性由已有Data租约与发布门禁决定。 */
FGamePlatformPresentationCatalogFragment MakeWeatherFragment(
    const bool bVisual)
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = bVisual
        ? TEXT("DBA.Presentation.Weather.VFX") : TEXT("DBA.Presentation.Weather.SFX");
    Fragment.Revision = 1;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;

    auto Add = [&Fragment, bVisual](const TCHAR* Suffix, const TCHAR* Definition)
    {
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId = FName(*(FString(TEXT("DBA.Weather.")) +
            Suffix + (bVisual ? TEXT(".VFX") : TEXT(".SFX"))));
        const FString Tag = FString(TEXT("Presentation.Weather.")) +
            Suffix + (bVisual ? TEXT(".VFX") : TEXT(".SFX"));
        Entry.SemanticTag = FGameplayTag::RequestGameplayTag(FName(*Tag), false);
        Entry.ProviderChannel = bVisual ? TEXT("VFX") : TEXT("SFX");
        Entry.DefinitionId = FName(Definition);
        Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Entry.Scope = EGamePlatformPresentationCatalogScope::Project;
        Entry.Priority = 0;
        Entry.ContentRevision = TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    };
    if (bVisual)
    {
        Add(TEXT("Rain"), TEXT("platform.weather.rain@1"));
        Add(TEXT("Snow"), TEXT("platform.weather.snow@1"));
    }
    else
    {
        Add(TEXT("Rain"), TEXT("presentation.dba.weather.rain@1"));
        Add(TEXT("Snow"), TEXT("presentation.dba.weather.snow@1"));
    }
    return Fragment;
}
}

FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildWeatherVFXFragment()
{
    return MakeWeatherFragment(true);
}

FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildWeatherSFXFragment()
{
    return MakeWeatherFragment(false);
}
