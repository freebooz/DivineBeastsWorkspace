// 本文件属于GamePlatform平台层 GamePlatformPresentation，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#include "GameplayTagsManager.h"
#include "GamePlatformPresentationClientSubsystem.h"


namespace
{
    /**
     * 仅测试使用的注册表作用域：ULocalPlayer 的 Within 要求 Engine，表现子系统的
     * Within 要求 LocalPlayer，不能用默认 Package Outer 冒充合法夹具。强持有两者，
     * 正常返回及前提失败都先 Deinitialize 子系统，再释放玩家。此夹具只测注册/解析，
     * 不 PlayerAdded、不创建世界/视口、不自动初始化依赖，也不代表资源或播放器验收。
     */
    struct FPresentationLocalPlayerFixture
    {
        TStrongObjectPtr<ULocalPlayer> Player;
        TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Subsystem;

        bool Initialize(FAutomationTestBase& Test)
        {
            if (!Test.TestNotNull(TEXT("注册表夹具需要真实Engine宿主"), GEngine))
            {
                return false;
            }
            Player.Reset(NewObject<ULocalPlayer>(GEngine));
            if (!Test.TestNotNull(TEXT("本地玩家具有合法Engine Outer"), Player.Get()))
            {
                return false;
            }
            Subsystem.Reset(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
            return Test.TestNotNull(TEXT("表现子系统具有合法LocalPlayer Outer"), Subsystem.Get());
        }

        ~FPresentationLocalPlayerFixture()
        {
            if (Subsystem.IsValid())
            {
                Subsystem->Deinitialize();
            }
            Subsystem.Reset();
            Player.Reset();
        }
    };

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
    FPresentationLocalPlayerFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
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
    FPresentationLocalPlayerFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
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

    FPresentationLocalPlayerFixture ParentSubsystemFixture;
    if (!ParentSubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* ParentSubsystem = ParentSubsystemFixture.Subsystem.Get();
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
    FPresentationLocalPlayerFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
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
    FPresentationLocalPlayerFixture SubsystemFixture;
    if (!SubsystemFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Subsystem = SubsystemFixture.Subsystem.Get();
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
    FPresentationLocalPlayerFixture ConflictFixture;
    if (!ConflictFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Conflict = ConflictFixture.Subsystem.Get();
    Conflict->RegisterCatalogFragment(Parent);
    auto Duplicate=Parent; Duplicate.FragmentId=TEXT("OtherFragment"); Duplicate.OwnerScopeId=TEXT("OtherOwner");
    Duplicate.Entries[0].DefinitionId=TEXT("Definition.Other");
    Conflict->RegisterCatalogFragment(Duplicate);
    TestEqual(TEXT("跨fragment同局部EntryId不掩盖歧义"), Conflict->ResolveCatalog(PresentationCatalogTestTag(), Context, Resolved),
        EGamePlatformPresentationCatalogResolveResult::Ambiguous);
    FPresentationLocalPlayerFixture NoFallbackFixture;
    if (!NoFallbackFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* NoFallback = NoFallbackFixture.Subsystem.Get();
    Parent.Entries[0].bAllowParentFallback=false; NoFallback->RegisterCatalogFragment(Parent);
    TestEqual(TEXT("父语义未显式允许时拒绝回退"), NoFallback->ResolveCatalog(PresentationCatalogTestChildTag(), Context, Resolved),
        EGamePlatformPresentationCatalogResolveResult::NoMatch);
    FPresentationLocalPlayerFixture ParentsFixture;
    if (!ParentsFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Parents = ParentsFixture.Subsystem.Get();
    auto Near=MakeFragment(TEXT("Near"),TEXT("Near"),EGamePlatformPresentationCatalogScope::Platform,TEXT("Definition.Near"));
    auto Far=MakeFragment(TEXT("Far"),TEXT("Far"),EGamePlatformPresentationCatalogScope::ContentPack,TEXT("Definition.Far"),0,
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Presentation"),true));
    Parents->RegisterCatalogFragment(Far); Parents->RegisterCatalogFragment(Near);
    TestEqual(TEXT("父回退逐级解析"),Parents->ResolveCatalog(PresentationCatalogTestChildTag(),Context,Resolved),
        EGamePlatformPresentationCatalogResolveResult::Resolved);
    TestEqual(TEXT("最近父语义优先远父高Scope"),Resolved.DefinitionId,FName(TEXT("Definition.Near")));
    return true;
}

// F14发布门禁：同键资格相交必须阻断，互斥英雄允许各自映射；不访问任何资产。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPresentationPreflightRegressionTest,
    "GamePlatform.Presentation.Catalog.PreflightConflictingQualification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformPresentationPreflightRegressionTest::RunTest(const FString&)
{
    FPresentationLocalPlayerFixture ServiceFixture;
    if (!ServiceFixture.Initialize(*this))
    {
        return false;
    }
    UGamePlatformPresentationClientSubsystem* Service = ServiceFixture.Subsystem.Get();
    FGamePlatformPresentationContextQuery Specificity;
    Specificity.ProjectId = TEXT("Project"); Specificity.ExperienceId = TEXT("Experience"); Specificity.RegionId = TEXT("Region");
    Specificity.ArenaModeId = TEXT("Arena"); Specificity.ContentPackId = TEXT("Pack");
    TestEqual(TEXT("项目/体验/区域/模式/包仅资格，不计P13具体度"), Specificity.GetSpecificity(), 0);
    Specificity.HeroDefinitionId = TEXT("Hero"); Specificity.AbilityId = TEXT("Ability"); Specificity.SkinId = TEXT("Skin");
    Specificity.WorldId = TEXT("World"); Specificity.PlatformId = TEXT("Platform"); Specificity.QualityTier = EGamePlatformPresentationQualityTier::High;
    TestEqual(TEXT("P13只计六个明确等值约束"), Specificity.GetSpecificity(), 6);
    auto A = MakeFragment(TEXT("A"), TEXT("AEntry"), EGamePlatformPresentationCatalogScope::ContentPack, TEXT("presentation.test.a@1"));
    A.Entries[0].ContextQuery.HeroDefinitionId = TEXT("Hero.A");
    auto B = A; B.FragmentId = TEXT("B"); B.OwnerScopeId = TEXT("BPack"); B.Entries[0].EntryId = TEXT("BEntry");
    B.Entries[0].ContextQuery.HeroDefinitionId = NAME_None; B.Entries[0].ContextQuery.AbilityId = TEXT("Ability.B");
    FString Error; TestTrue(TEXT("单片段可预检"), Service->PreflightCatalogFragment(A, Error)); Service->RegisterCatalogFragment(A);
    TestFalse(TEXT("Hero.A与Ability.B资格可同时满足，同键必须冲突"), Service->PreflightCatalogFragment(B, Error));
    TestTrue(TEXT("冲突包含目录/条目身份"), Error.Contains(TEXT("AEntry")) && Error.Contains(TEXT("BEntry")));
    B.Entries[0].ContextQuery.AbilityId = NAME_None; B.Entries[0].ContextQuery.HeroDefinitionId = TEXT("Hero.B");
    TestTrue(TEXT("互斥英雄同排序键允许"), Service->PreflightCatalogFragment(B, Error));
    auto Duplicate = B.Entries[0]; Duplicate.EntryId = TEXT("InternalDuplicate"); B.Entries.Add(Duplicate);
    TestFalse(TEXT("片段内部完全同键也必须拒绝"), Service->PreflightCatalogFragment(B, Error));
    Service->RegisterProvider(TEXT("TypedProvider"), 1, FGamePlatformPresentationProviderHandler::CreateLambda(
        [](const FGamePlatformPresentationRequest&) { return true; }), UObject::StaticClass());
    TestEqual(TEXT("中立Provider类型合同保留真实Class"), Service->GetProviderDefinitionClass(TEXT("TypedProvider")), UObject::StaticClass());
    TestNull(TEXT("缺Provider不能猜测定义类型"), Service->GetProviderDefinitionClass(TEXT("MissingProvider")));
    return true;
}
#endif
