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
        Entry.bAllowParentFallback = true;
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

// F08：完整注册与解析回归；删除精确优先或恢复按局部EntryId消歧会失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPresentationResolutionRegressionTest,
    "GamePlatform.Presentation.Regression.ExactAndFragmentConflict",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformPresentationResolutionRegressionTest::RunTest(const FString&)
{
    auto* Subsystem = NewObject<UGamePlatformPresentationClientSubsystem>();
    auto Parent = MakeFragment(TEXT("Parent"), TEXT("Same"), EGamePlatformPresentationCatalogScope::ContentPack,
        TEXT("Definition.Parent"), 0, PresentationCatalogTestTag());
    Parent.Entries[0].bAllowParentFallback = true;
    Subsystem->RegisterCatalogFragment(Parent);
    Subsystem->RegisterCatalogFragment(MakeFragment(TEXT("Exact"), TEXT("Exact"),
        EGamePlatformPresentationCatalogScope::Project, TEXT("Definition.Exact"), 0, PresentationCatalogTestChildTag()));
    FGamePlatformPresentationContext Context; Context.ProjectId=TEXT("Project.Test");
    FGamePlatformPresentationResolvedEntry Resolved;
    TestEqual(TEXT("精确语义必须优先高Scope父语义"), Subsystem->ResolveCatalog(PresentationCatalogTestChildTag(), Context, Resolved),
        EGamePlatformPresentationCatalogResolveResult::Resolved);
    TestEqual(TEXT("实际使用精确定义"), Resolved.DefinitionId, FName(TEXT("Definition.Exact")));
    auto* Conflict = NewObject<UGamePlatformPresentationClientSubsystem>();
    Conflict->RegisterCatalogFragment(Parent);
    auto Duplicate=Parent; Duplicate.FragmentId=TEXT("OtherFragment"); Duplicate.OwnerScopeId=TEXT("OtherOwner");
    Duplicate.Entries[0].DefinitionId=TEXT("Definition.Other");
    Conflict->RegisterCatalogFragment(Duplicate);
    TestEqual(TEXT("跨fragment同局部EntryId不掩盖歧义"), Conflict->ResolveCatalog(PresentationCatalogTestTag(), Context, Resolved),
        EGamePlatformPresentationCatalogResolveResult::Ambiguous);
    auto* NoFallback = NewObject<UGamePlatformPresentationClientSubsystem>();
    Parent.Entries[0].bAllowParentFallback=false; NoFallback->RegisterCatalogFragment(Parent);
    TestEqual(TEXT("父语义未显式允许时拒绝回退"), NoFallback->ResolveCatalog(PresentationCatalogTestChildTag(), Context, Resolved),
        EGamePlatformPresentationCatalogResolveResult::NoMatch);
    auto* Parents=NewObject<UGamePlatformPresentationClientSubsystem>();
    auto Near=MakeFragment(TEXT("Near"),TEXT("Near"),EGamePlatformPresentationCatalogScope::Platform,TEXT("Definition.Near"));
    auto Far=MakeFragment(TEXT("Far"),TEXT("Far"),EGamePlatformPresentationCatalogScope::ContentPack,TEXT("Definition.Far"),0,
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Presentation"),true));
    Parents->RegisterCatalogFragment(Far); Parents->RegisterCatalogFragment(Near);
    TestEqual(TEXT("父回退逐级解析"),Parents->ResolveCatalog(PresentationCatalogTestChildTag(),Context,Resolved),
        EGamePlatformPresentationCatalogResolveResult::Resolved);
    TestEqual(TEXT("最近父语义优先远父高Scope"),Resolved.DefinitionId,FName(TEXT("Definition.Near")));
    return true;
}

#endif
