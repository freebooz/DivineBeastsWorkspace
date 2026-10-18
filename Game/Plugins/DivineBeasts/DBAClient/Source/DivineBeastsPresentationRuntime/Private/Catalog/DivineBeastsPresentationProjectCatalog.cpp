// 本文件属于DivineBeasts项目层 DivineBeastsPresentationRuntime，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 项目默认目录合同：逻辑ID合法不代表已交付资产，客户端真实Data预载成功才发布。
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"

/** 只映射一种项目外观到同一平台VFX语义，避免多个变体同键目录冲突。 */
FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildFrontEndFoliageFragment(const FName StyleId)
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = TEXT("DBA.Presentation.FrontEnd.FallingFoliage");
    Fragment.Revision = CatalogRevision;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;

    FName LogicalId = NAME_None;
    if (StyleId == TEXT("Peach"))
        LogicalId = TEXT("presentation.dba.environment.fallingfoliage.peach@1");
    else if (StyleId == TEXT("Maple"))
        LogicalId = TEXT("presentation.dba.environment.fallingfoliage.maple@1");
    else if (StyleId == TEXT("Bamboo"))
        LogicalId = TEXT("presentation.dba.environment.fallingfoliage.bamboo@1");
    else if (StyleId == TEXT("Ginkgo"))
        LogicalId = TEXT("presentation.dba.environment.fallingfoliage.ginkgo@1");

    if (LogicalId.IsNone())
        return Fragment; // 非法Style不能获得可发布的目录条目。

    FGamePlatformPresentationCatalogEntry Entry;
    Entry.EntryId = TEXT("DBA.FrontEnd.FallingFoliage.Visual");
    Entry.SemanticTag = DivineBeastsPresentationTags::FrontEnd_FallingFoliage;
    Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    Entry.ProviderChannel = TEXT("VFX");
    Entry.DefinitionId = LogicalId;
    Entry.Scope = EGamePlatformPresentationCatalogScope::Project;
    Entry.Specificity = 1;
    Entry.Priority = 0;
    Entry.ContentRevision = TEXT("1");
    Fragment.Entries.Add(Entry);
    return Fragment;
}

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

// 项目水体VFX只登记逻辑身份，不包含Niagara系统路径，避免绕过平台内容预检。
FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildWaterVFXFragment()
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = TEXT("DBA.Presentation.Water.VFX");
    Fragment.Revision = CatalogRevision;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;
    auto Add = [&Fragment](const TCHAR* Suffix, const TCHAR* LogicalId)
    {
        const FString TagName = FString(TEXT("Presentation.Water.")) + Suffix + TEXT(".VFX");
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
        if (!Tag.IsValid())
            return; // GameplayTag注册缺失时安全关闭，不发布无效语义。
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId = FName(*(FString(TEXT("DBA.Water.")) + Suffix + TEXT(".VFX")));
        Entry.SemanticTag = Tag;
        Entry.ProviderChannel = TEXT("VFX");
        Entry.DefinitionId = FName(LogicalId);
        Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Entry.Scope = EGamePlatformPresentationCatalogScope::Project;
        Entry.Specificity = 1;
        Entry.ContentRevision = TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    };
    Add(TEXT("Ripple"), TEXT("platform.water.ripple@1"));
    Add(TEXT("Splash"), TEXT("platform.water.splash@1"));
    return Fragment;
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
/** SSLM首批音频数据目录：只映射语义到SFX定义身份，具体声音始终属于项目内容包。 */
FGamePlatformPresentationCatalogFragment
FDivineBeastsPresentationProjectCatalog::BuildStarterAudioFragment()
{
    FGamePlatformPresentationCatalogFragment Fragment;
    Fragment.FragmentId = TEXT("DBA.Presentation.Audio.Starter");
    Fragment.Revision = 1;
    Fragment.Scope = EGamePlatformPresentationCatalogScope::Project;
    Fragment.OwnerScopeId = FDivineBeastsProjectCatalog::GetProjectId();
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;
    struct FAudioEntry
    {
        const TCHAR* Name;
        const TCHAR* Tag;
        const TCHAR* DefinitionId;
    };
    static const FAudioEntry Entries[] =
    {
        { TEXT("UI.Click"), TEXT("Presentation.SSLM.Audio.UI.Click"), TEXT("presentation.dba.audio.ui.click@1") },
        { TEXT("UI.Confirm"), TEXT("Presentation.SSLM.Audio.UI.Confirm"), TEXT("presentation.dba.audio.ui.confirm@1") },
        { TEXT("UI.Cancel"), TEXT("Presentation.SSLM.Audio.UI.Cancel"), TEXT("presentation.dba.audio.ui.cancel@1") },
        { TEXT("Spell.Frost.Cast"), TEXT("Presentation.SSLM.Audio.Spell.Frost.Cast"), TEXT("presentation.dba.audio.spell.frost.cast@1") },
        { TEXT("Spell.Frost.Impact"), TEXT("Presentation.SSLM.Audio.Spell.Frost.Impact"), TEXT("presentation.dba.audio.spell.frost.impact@1") },
        { TEXT("Spell.Fire.Impact"), TEXT("Presentation.SSLM.Audio.Spell.Fire.Impact"), TEXT("presentation.dba.audio.spell.fire.impact@1") },
        { TEXT("Music.Login"), TEXT("Presentation.SSLM.Audio.Music.Login"), TEXT("presentation.dba.audio.music.login@1") },
        { TEXT("Music.Village"), TEXT("Presentation.SSLM.Audio.Music.Village"), TEXT("presentation.dba.audio.music.village@1") },
        { TEXT("Ambience.Forest"), TEXT("Presentation.SSLM.Audio.Ambience.Forest"), TEXT("presentation.dba.audio.ambience.forest@1") }
    };
    for (const FAudioEntry& Audio : Entries)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Audio.Tag), false);
        if (!Tag.IsValid())
        {
            // 任意必要标签未登记时不发布半套目录；内容包必须原子预载和注册。
            Fragment.Entries.Reset();
            return Fragment;
        }
        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId = FName(*(FString(TEXT("DBA.Audio.")) + Audio.Name));
        Entry.SemanticTag = Tag;
        Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Entry.ProviderChannel = TEXT("SFX");
        Entry.DefinitionId = FName(Audio.DefinitionId);
        Entry.Scope = EGamePlatformPresentationCatalogScope::Project;
        Entry.ContentRevision = TEXT("1");
        Fragment.Entries.Add(MoveTemp(Entry));
    }
    return Fragment;
}

