#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTagsManager.h"
#include "GamePlatformPresentationClientSubsystem.h"


namespace
{
    FGameplayTag PresentationCatalogTestTag()
    {
        return UGameplayTagsManager::Get().RequestGameplayTag(
            TEXT("Presentation.Test"),
            true);
    }

    FGameplayTag PresentationCatalogTestChildTag()
    {
        return UGameplayTagsManager::Get().RequestGameplayTag(
            TEXT("Presentation.Test.Child"),
            true);
    }

    class FTestPresentationContributor final
        : public IGamePlatformPresentationContextContributor
    {
    public:
        FName Id = NAME_None;
        int32 Priority = 0;
        EGamePlatformPresentationConflictPolicy Policy =
            EGamePlatformPresentationConflictPolicy::FillMissing;
        FGamePlatformPresentationContextPatch Patch;

        virtual FName GetContributorId() const override { return Id; }
        virtual int32 GetPriority() const override { return Priority; }
        virtual EGamePlatformPresentationContextScope GetScope() const override
        {
            return EGamePlatformPresentationContextScope::LocalPlayer;
        }
        virtual EGamePlatformPresentationConflictPolicy GetConflictPolicy() const override
        {
            return Policy;
        }
        virtual FGamePlatformPresentationContextPatch BuildPatch() const override
        {
            return Patch;
        }
    };

    FGamePlatformPresentationCatalogFragment MakeFragment(
        FName FragmentId,
        FName EntryId,
        EGamePlatformPresentationCatalogScope Scope,
        FName DefinitionId,
        int32 Priority = 0,
        const FGameplayTag& Semantic = PresentationCatalogTestTag())
    {
        FGamePlatformPresentationCatalogFragment Fragment;
        Fragment.FragmentId = FragmentId;
        Fragment.Revision = 1;
        Fragment.Scope = Scope;
        Fragment.OwnerScopeId = FragmentId;
        Fragment.LifecycleScope =
            EGamePlatformPresentationContextScope::Session;

        FGamePlatformPresentationCatalogEntry Entry;
        Entry.EntryId = EntryId;
        Entry.SemanticTag = Semantic;
        Entry.ContextQuery.ProjectId = TEXT("Project.Test");
        Entry.ProviderChannel = TEXT("VFX");
        Entry.DefinitionId = DefinitionId;
        Entry.Scope = Scope;
        Entry.Priority = Priority;
        Entry.ContentRevision = TEXT("1");
        Fragment.Entries.Add(Entry);
        return Fragment;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPresentationContextRegistryTest,
    "GamePlatform.Presentation.ContextRegistry",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformPresentationContextRegistryTest::RunTest(const FString&)
{
    UGamePlatformPresentationClientSubsystem* Subsystem =
        NewObject<UGamePlatformPresentationClientSubsystem>();
    TestNotNull(TEXT("Presentation subsystem"), Subsystem);
    if (!Subsystem)
    {
        return false;
    }

    TSharedRef<FTestPresentationContributor> Base =
        MakeShared<FTestPresentationContributor>();
    Base->Id = TEXT("Test.Base");
    Base->Priority = 10;
    Base->Patch.Values.ProjectId = TEXT("Project.Test");

    TSharedRef<FTestPresentationContributor> Hero =
        MakeShared<FTestPresentationContributor>();
    Hero->Id = TEXT("Test.Hero");
    Hero->Priority = 5;
    Hero->Patch.Values.HeroDefinitionId = TEXT("Hero.Test");

    const FGamePlatformPresentationRegistrationHandle BaseHandle =
        Subsystem->RegisterContextContributor(Base);
    const FGamePlatformPresentationRegistrationHandle HeroHandle =
        Subsystem->RegisterContextContributor(Hero);
    TestTrue(TEXT("Base contributor registered"), BaseHandle.IsValid());
    TestTrue(TEXT("Hero contributor registered"), HeroHandle.IsValid());

    FGamePlatformPresentationContext Context;
    TestTrue(TEXT("Context builds"), Subsystem->BuildContext(Context));
    TestEqual(TEXT("Project context"), Context.ProjectId, FName(TEXT("Project.Test")));
    TestEqual(TEXT("Hero context"), Context.HeroDefinitionId, FName(TEXT("Hero.Test")));

    TSharedRef<FTestPresentationContributor> Conflict =
        MakeShared<FTestPresentationContributor>();
    Conflict->Id = TEXT("Test.Conflict");
    Conflict->Priority = 20;
    Conflict->Policy = EGamePlatformPresentationConflictPolicy::RejectConflict;
    Conflict->Patch.Values.ProjectId = TEXT("Project.Other");
    TestTrue(
        TEXT("Conflict contributor registered"),
        Subsystem->RegisterContextContributor(Conflict).IsValid());

    FGamePlatformPresentationContext Conflicted;
    Conflicted.ProjectId = TEXT("Project.Test");
    TestFalse(
        TEXT("RejectConflict fails deterministic merge"),
        Subsystem->BuildContext(Conflicted));

    TestTrue(
        TEXT("Unregister original handle"),
        Subsystem->UnregisterContextContributor(BaseHandle));
    TestFalse(
        TEXT("Stale handle rejected"),
        Subsystem->UnregisterContextContributor(BaseHandle));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPresentationCatalogResolutionTest,
    "GamePlatform.Presentation.CatalogResolution",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformPresentationCatalogResolutionTest::RunTest(const FString&)
{
    UGamePlatformPresentationClientSubsystem* Subsystem =
        NewObject<UGamePlatformPresentationClientSubsystem>();
    TestNotNull(TEXT("Presentation subsystem"), Subsystem);
    if (!Subsystem)
    {
        return false;
    }

    TestTrue(
        TEXT("Project fragment registered"),
        Subsystem->RegisterCatalogFragment(
            MakeFragment(
                TEXT("Project.Fragment"),
                TEXT("Project.Entry"),
                EGamePlatformPresentationCatalogScope::Project,
                TEXT("Definition.Project"))).IsValid());

    TestTrue(
        TEXT("ContentPack fragment registered"),
        Subsystem->RegisterCatalogFragment(
            MakeFragment(
                TEXT("Pack.Fragment"),
                TEXT("Pack.Entry"),
                EGamePlatformPresentationCatalogScope::ContentPack,
                TEXT("Definition.Pack"))).IsValid());

    FGamePlatformPresentationContext Context;
    Context.ProjectId = TEXT("Project.Test");

    FGamePlatformPresentationResolvedEntry Resolved;
    TestEqual(
        TEXT("ContentPack precedence resolves"),
        Subsystem->ResolveCatalog(
            PresentationCatalogTestTag(),
            Context,
            Resolved),
        EGamePlatformPresentationCatalogResolveResult::Resolved);
    TestEqual(
        TEXT("ContentPack wins Project"),
        Resolved.DefinitionId,
        FName(TEXT("Definition.Pack")));

    TestTrue(
        TEXT("Second equal ContentPack registered"),
        Subsystem->RegisterCatalogFragment(
            MakeFragment(
                TEXT("Pack.Fragment.2"),
                TEXT("Pack.Entry.2"),
                EGamePlatformPresentationCatalogScope::ContentPack,
                TEXT("Definition.Pack2"))).IsValid());

    TestEqual(
        TEXT("Equivalent top candidates fail ambiguity"),
        Subsystem->ResolveCatalog(
            PresentationCatalogTestTag(),
            Context,
            Resolved),
        EGamePlatformPresentationCatalogResolveResult::Ambiguous);

    UGamePlatformPresentationClientSubsystem* ParentSubsystem =
        NewObject<UGamePlatformPresentationClientSubsystem>();
    TestTrue(
        TEXT("Parent semantic registered"),
        ParentSubsystem->RegisterCatalogFragment(
            MakeFragment(
                TEXT("Parent.Fragment"),
                TEXT("Parent.Entry"),
                EGamePlatformPresentationCatalogScope::Platform,
                TEXT("Definition.Parent"),
                0,
                PresentationCatalogTestTag())).IsValid());

    TestEqual(
        TEXT("Parent fallback resolves"),
        ParentSubsystem->ResolveCatalog(
            PresentationCatalogTestChildTag(),
            Context,
            Resolved),
        EGamePlatformPresentationCatalogResolveResult::Resolved);
    TestEqual(
        TEXT("Parent fallback definition"),
        Resolved.DefinitionId,
        FName(TEXT("Definition.Parent")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPresentationCapacityTest,
    "GamePlatform.Presentation.CapacityGuards",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformPresentationCapacityTest::RunTest(const FString&)
{
    UGamePlatformPresentationClientSubsystem* Subsystem =
        NewObject<UGamePlatformPresentationClientSubsystem>();
    TestNotNull(TEXT("Presentation subsystem"), Subsystem);
    if (!Subsystem)
    {
        return false;
    }

    for (int32 Index = 0; Index < 32; ++Index)
    {
        const FName ProviderId(*FString::Printf(TEXT("Provider.%d"), Index));
        const FGuid Handle = Subsystem->RegisterProvider(
            ProviderId,
            Index,
            FGamePlatformPresentationProviderHandler::CreateLambda(
                [](const FGamePlatformPresentationRequest&) { return false; }));
        TestTrue(TEXT("前32个Provider允许注册"), Handle.IsValid());
    }
    TestFalse(
        TEXT("第33个Provider被容量门禁拒绝"),
        Subsystem->RegisterProvider(
            TEXT("Provider.Overflow"),
            0,
            FGamePlatformPresentationProviderHandler::CreateLambda(
                [](const FGamePlatformPresentationRequest&) { return false; })).IsValid());

    FGamePlatformPresentationCatalogFragment Oversized = MakeFragment(
        TEXT("Oversized.Fragment"),
        TEXT("Oversized.0"),
        EGamePlatformPresentationCatalogScope::Project,
        TEXT("Definition.0"));
    const FGamePlatformPresentationCatalogEntry Template = Oversized.Entries[0];
    for (int32 Index = 1; Index < 513; ++Index)
    {
        FGamePlatformPresentationCatalogEntry Entry = Template;
        Entry.EntryId = FName(*FString::Printf(TEXT("Oversized.%d"), Index));
        Entry.DefinitionId = FName(*FString::Printf(TEXT("Definition.%d"), Index));
        Oversized.Entries.Add(MoveTemp(Entry));
    }
    TestFalse(TEXT("单Catalog Fragment超过512条目被拒绝"), Oversized.IsValid());
    return true;
}

#endif
