// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "MobaPresentationRequestBuilder.h"
#include "MobaPresentationSemanticRegistry.h"
#include "Tags/MobaPresentationTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationSemanticRegistryTest,
    "Moba.Presentation.Runtime.SemanticRegistry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationSemanticRegistryTest::RunTest(const FString&)
{
    TArray<FString> Errors;
    TestTrue(TEXT("语义注册表必须有效"), FMobaPresentationSemanticRegistry::Validate(Errors));
    TestEqual(TEXT("第一版语义数量"), FMobaPresentationSemanticRegistry::GetAll().Num(), 17);
    TestTrue(TEXT("Hit标签有效"), MobaPresentationTags::Combat_Hit.GetTag().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationRequestBuilderTest,
    "Moba.Presentation.Runtime.RequestBuilder",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationRequestBuilderTest::RunTest(const FString&)
{
    FMobaPresentationAdaptedFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Semantic = MobaPresentationTags::Combat_Hit;
    Fact.Context.WorldGeneration = 3;
    Fact.Context.Magnitude = 25.0f;
    // F12：旁观他人事实必须携带事实源的资格，不能丢失后被本地英雄上下文替换。
    Fact.Context.HeroDefinitionId = TEXT("hero.observed@1");
    Fact.Context.AbilityId = TEXT("ability.observed@1");
    Fact.Context.ArenaModeId = TEXT("Arena.5v5");
    Fact.Context.AvatarGeneration = 9;
    const FGamePlatformPresentationRequest Request =
        FMobaPresentationRequestBuilder::Build(Fact, 7);

    TestTrue(TEXT("请求应有效"), Request.IsValid());
    TestEqual(TEXT("WorldGeneration保持"), Request.WorldGeneration, 3);
    TestEqual(TEXT("RequestGeneration保持"), Request.RequestGeneration, 7);
    TestEqual(TEXT("Magnitude保持"), Request.Magnitude, 25.0f);
    TestEqual(TEXT("事实源英雄保持"), Request.Context.HeroDefinitionId, FName(*Fact.Context.HeroDefinitionId));
    TestEqual(TEXT("事实源技能保持"), Request.Context.AbilityId, FName(*Fact.Context.AbilityId));
    TestEqual(TEXT("模式保持"), Request.Context.ArenaModeId, Fact.Context.ArenaModeId);
    TestEqual(TEXT("Avatar代次保持"), Request.Context.AvatarGeneration, 9);
    TestEqual(TEXT("类型化世界代次保持"), Request.Context.WorldGeneration, 3);
    return true;
}

#endif
