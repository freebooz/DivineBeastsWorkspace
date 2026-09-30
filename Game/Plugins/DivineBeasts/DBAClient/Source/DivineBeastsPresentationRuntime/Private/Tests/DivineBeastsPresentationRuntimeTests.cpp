#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "Context/DivineBeastsPresentationContext.h"
#include "Facts/DivineBeastsPresentationFacts.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"
#include "VFX/DivineBeastsHeroVFXProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsPresentationRuntimeContractTest,
    "DivineBeasts.Presentation.RuntimeContracts",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPresentationRuntimeContractTest::RunTest(const FString&)
{
    TestTrue(
        TEXT("World interaction semantic valid"),
        DivineBeastsPresentationTags::World_Interaction_Committed.GetTag().IsValid());
    TestTrue(
        TEXT("Village guidance semantic valid"),
        DivineBeastsPresentationTags::Village_Guidance_Ready.GetTag().IsValid());

    FDivineBeastsPresentationProjectContext Context;
    Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    Context.WorldGeneration = 1;
    FString Error;
    TestTrue(TEXT("Project context valid"), Context.IsValid(Error));

    Context.ProjectId = TEXT("Wrong.Project");
    Error.Reset();
    TestFalse(TEXT("Wrong project rejected"), Context.IsValid(Error));

    const FGamePlatformPresentationCatalogFragment DefaultCatalog =
        FDivineBeastsPresentationProjectCatalog::BuildDefaultFragment();
    TestTrue(TEXT("Default catalog valid"), DefaultCatalog.IsValid());
    TestEqual(TEXT("Default catalog entries"), DefaultCatalog.Entries.Num(), 2);
    TestEqual(
        TEXT("Default catalog scope"),
        DefaultCatalog.Scope,
        EGamePlatformPresentationCatalogScope::Project);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsPresentationContentPackContractTest,
    "DivineBeasts.Presentation.ContentPackContract",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPresentationContentPackContractTest::RunTest(const FString&)
{
    FDivineBeastsPresentationContentPackFragment Pack;
    Pack.ContentPackId = TEXT("DBA.Test.Pack");
    Pack.Revision = 2;
    Pack.LifecycleScope = EGamePlatformPresentationContextScope::World;
    Pack.CatalogFragment.FragmentId = TEXT("DBA.Test.Pack.Catalog");
    Pack.CatalogFragment.Revision = 2;
    Pack.CatalogFragment.Scope =
        EGamePlatformPresentationCatalogScope::ContentPack;
    Pack.CatalogFragment.OwnerScopeId = Pack.ContentPackId;
    Pack.CatalogFragment.LifecycleScope = Pack.LifecycleScope;

    FGamePlatformPresentationCatalogEntry Entry;
    Entry.EntryId = TEXT("DBA.Test.Pack.Entry");
    Entry.SemanticTag =
        DivineBeastsPresentationTags::World_Interaction_Committed;
    Entry.ProviderChannel = TEXT("VFX");
    Entry.DefinitionId = TEXT("Presentation.Test.Definition");
    Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    Entry.ContentRevision = TEXT("2");
    Pack.CatalogFragment.Entries.Add(Entry);
    Pack.LogicalPreloadDefinitionIds.Add(Entry.DefinitionId);

    FString Error;
    TestTrue(TEXT("ContentPack valid"), Pack.IsValid(Error));

    Pack.CatalogFragment.OwnerScopeId = TEXT("Wrong.Pack");
    Error.Reset();
    TestFalse(TEXT("Wrong pack owner rejected"), Pack.IsValid(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsPresentationFactValidationTest,
    "DivineBeasts.Presentation.ProjectFacts",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPresentationFactValidationTest::RunTest(const FString&)
{
    FDivineBeastsWorldInteractionPresentationFact Interaction;
    Interaction.FactId = FGuid::NewGuid();
    Interaction.OptionId = TEXT("Interaction.Test");
    TestTrue(TEXT("Interaction fact valid"), Interaction.IsValid());

    FDivineBeastsVillageFeedbackPresentationFact Tutorial;
    Tutorial.FactId = FGuid::NewGuid();
    Tutorial.FeedbackId = TEXT("Village.Guidance.Ready");
    Tutorial.ExperienceId = TEXT("Experience.Village.Tutorial");
    TestTrue(TEXT("Tutorial feedback valid"), Tutorial.IsValid());

    Tutorial.ExperienceId = TEXT("Experience.OpenWorld.Main");
    TestFalse(TEXT("Non-Village feedback rejected"), Tutorial.IsValid());
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsHeroVFXProfileCatalogTest,
    "DivineBeasts.Presentation.HeroVFXProfiles",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHeroVFXProfileCatalogTest::RunTest(const FString&)
{
    const TArray<FName>& HeroIds = FDivineBeastsProjectCatalog::GetHeroDefinitionIds();
    const TArray<FDivineBeastsHeroVFXProfile>& Profiles =
        FDivineBeastsHeroVFXProfileCatalog::GetProfiles();

    TestEqual(TEXT("十二生肖Hero定义数量"), HeroIds.Num(), 12);
    TestEqual(TEXT("VFX Profile覆盖全部核心Hero"), Profiles.Num(), HeroIds.Num());

    TSet<FName> UniqueProfileIds;
    for (const FName HeroId : HeroIds)
    {
        const FDivineBeastsHeroVFXProfile* Profile =
            FDivineBeastsHeroVFXProfileCatalog::Find(HeroId);
        TestNotNull(TEXT("每个Hero都有VFX Profile"), Profile);
        if (!Profile)
        {
            continue;
        }

        TestTrue(TEXT("VFX Profile字段完整"), Profile->IsValid());
        TestEqual(TEXT("Profile HeroId保持原始身份"), Profile->HeroDefinitionId, HeroId);
        TestTrue(
            TEXT("ProfileId使用项目VFX稳定命名空间"),
            Profile->ProfileId.ToString().StartsWith(
                TEXT("Presentation.VFX.Hero.Zodiac."),
                ESearchCase::CaseSensitive));
        TestFalse(
            TEXT("ProfileId不得重复"),
            UniqueProfileIds.Contains(Profile->ProfileId));
        UniqueProfileIds.Add(Profile->ProfileId);
        TestEqual(
            TEXT("默认Profile查询一致"),
            FDivineBeastsHeroVFXProfileCatalog::GetDefaultProfileId(HeroId),
            Profile->ProfileId);
    }

    TestTrue(
        TEXT("未知Hero不返回VFX Profile"),
        FDivineBeastsHeroVFXProfileCatalog::Find(TEXT("Hero.Unknown")) == nullptr);
    TestTrue(
        TEXT("未知Hero默认Profile为空"),
        FDivineBeastsHeroVFXProfileCatalog::GetDefaultProfileId(TEXT("Hero.Unknown")).IsNone());
    return true;
}

#endif
