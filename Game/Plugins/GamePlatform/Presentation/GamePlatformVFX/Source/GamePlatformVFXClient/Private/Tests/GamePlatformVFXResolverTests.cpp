// 本文件属于GamePlatform平台层 GamePlatformVFX，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Resolution/GamePlatformVFXResolver.h"
#include "GameplayTagsManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXResolverExactTest,
    "GamePlatform.VFX.Resolver.Exact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXResolverExactTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXCatalog* Catalog = NewObject<UGamePlatformVFXCatalog>();
    UGamePlatformVFXInstantDefinition* Definition = NewObject<UGamePlatformVFXInstantDefinition>();

    FGamePlatformVFXCatalogEntry& Entry = Catalog->Entries.AddDefaulted_GetRef();
    Entry.DefinitionId = TEXT("VFX.Test.Exact");
    Entry.Definition = Definition;
    Entry.Priority = 10;

    FGamePlatformVFXCatalogRegistry Registry;
    const uint64 RevisionBefore = Registry.GetRevision();
    const FGamePlatformVFXRegistrationHandle Handle = Registry.Register(Catalog);
    TestTrue(TEXT("Catalog 注册成功"), Handle.IsValid());
    TestTrue(TEXT("Catalog 注册使Revision前进"), Registry.GetRevision() > RevisionBefore);

    FGamePlatformVFXRequest Request;
    Request.DefinitionId = Entry.DefinitionId;
    const FGamePlatformVFXResolvedDefinition Resolved = FGamePlatformVFXResolver(Registry).Resolve(Request);
    TestTrue(TEXT("Resolver按DefinitionId命中Definition"), Resolved.Definition.Get() == Definition);
    TestFalse(TEXT("单一最佳结果不歧义"), Resolved.bAmbiguous);

    UGamePlatformVFXCatalog* AmbiguousCatalog = NewObject<UGamePlatformVFXCatalog>();
    UGamePlatformVFXInstantDefinition* AmbiguousDefinition = NewObject<UGamePlatformVFXInstantDefinition>();
    FGamePlatformVFXCatalogEntry& AmbiguousEntry = AmbiguousCatalog->Entries.AddDefaulted_GetRef();
    AmbiguousEntry.DefinitionId = Entry.DefinitionId;
    AmbiguousEntry.Definition = AmbiguousDefinition;
    AmbiguousEntry.Priority = Entry.Priority;
    TestTrue(TEXT("第二Catalog注册成功"), Registry.Register(AmbiguousCatalog).IsValid());

    const FGamePlatformVFXResolvedDefinition Ambiguous = FGamePlatformVFXResolver(Registry).Resolve(Request);
    TestTrue(TEXT("同级歧义必须Fail Closed"), Ambiguous.bAmbiguous);
    TestFalse(TEXT("歧义时不得任意选择Definition"), Ambiguous.IsValid());
    return true;
}

// F03：实际Registry先资格过滤，再按语义→Scope→六等值项→Priority；集合标签不计具体度。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXResolverScopeRegressionTest,
    "GamePlatform.VFX.Resolver.P13ScopeAndTypedEligibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXResolverScopeRegressionTest::RunTest(const FString&)
{
    // ClientOnly不得定义原生全局标签；读取现有DefaultGameplayTags.ini测试标签，避免本测试新增客户端独占标签。
    // 未加载夹具配置时明确失败，不让无效空标签把语义筛选变成DefinitionId路径的伪通过。
    const FGameplayTag SemanticTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Presentation.Test"), false);
    if (!TestTrue(TEXT("中立表现测试标签已通过正式配置加载"), SemanticTag.IsValid())) { return false; }
    auto* LowCatalog = NewObject<UGamePlatformVFXCatalog>(); auto* PackCatalog = NewObject<UGamePlatformVFXCatalog>();
    auto* LowDefinition = NewObject<UGamePlatformVFXInstantDefinition>(); auto* PackDefinition = NewObject<UGamePlatformVFXInstantDefinition>();
    FGamePlatformVFXCatalogEntry Low; Low.SemanticTag = SemanticTag; Low.DefinitionId = TEXT("presentation.test.low@1");
    Low.Definition = LowDefinition; Low.Scope = EGamePlatformVFXCatalogScope::Platform; Low.ContextId = TEXT("Detailed"); Low.HeroDefinitionId = TEXT("Hero.Source");
    LowCatalog->Entries.Add(Low);
    auto Pack = Low; Pack.Scope = EGamePlatformVFXCatalogScope::ContentPack; Pack.ContextId = NAME_None; Pack.HeroDefinitionId = NAME_None;
    Pack.DefinitionId = TEXT("presentation.test.pack@1"); Pack.Definition = PackDefinition; PackCatalog->Entries.Add(Pack);
    FGamePlatformVFXCatalogRegistry Registry; Registry.Register(LowCatalog); Registry.Register(PackCatalog);
    FGamePlatformVFXRequest Request; Request.SemanticTag = SemanticTag; Request.ContextId = TEXT("Detailed"); Request.HeroDefinitionId = TEXT("Hero.Source");
    TestEqual(TEXT("高Scope包默认优先低层具体英雄"), Registry.Resolve(Request).Definition.Get(), static_cast<UGamePlatformVFXDefinition*>(PackDefinition));
    auto Other = Pack; Other.HeroDefinitionId = TEXT("Hero.Other"); Other.Definition = LowDefinition;
    PackCatalog->Entries.Add(Other); Registry.Reset(); Registry.Register(PackCatalog);
    TestEqual(TEXT("不匹配事实源英雄的更具体条目先被过滤"), Registry.Resolve(Request).Definition.Get(), static_cast<UGamePlatformVFXDefinition*>(PackDefinition));
    return true;
}

#endif
