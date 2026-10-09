// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "MobaPresentationRequestBuilder.h"
#include "MobaPresentationSemanticRegistry.h"
#include "Tags/MobaPresentationTags.h"

/**
 * 校验当前有效语义合同，不把保留的历史标签路径当作新的表现事实。
 * Runtime通过正式Native Tag注册；本测试只读真实注册表，不新增临时标签或伪造加载成功。
 * 缺任意现行语义、重复/非法标签或历史暴击重新进入有效表均必须失败。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationSemanticRegistryTest,
    "Moba.Presentation.Runtime.SemanticRegistry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationSemanticRegistryTest::RunTest(const FString&)
{
    TArray<FString> Errors;
    TestTrue(TEXT("语义注册表必须有效"), FMobaPresentationSemanticRegistry::Validate(Errors));
    for (const FString& Error : Errors)
    {
        AddError(Error); // 校验失败保留具体诊断，不能仅靠数量断言掩盖注册/重复问题。
    }

    // 2026-10-09取消暴击输出后，17条已发布Native路径中仅16条是有效语义；另一条只供旧资产兼容。
    // 期望值按公开现行合同列出，不从被测GetAll推导，替换/遗漏任一正式语义都能被Find断言捕获。
    const FGameplayTag ExpectedSemantics[] =
    {
        MobaPresentationTags::Combat_Hit,
        MobaPresentationTags::Combat_Heal,
        MobaPresentationTags::Combat_Shield_Hit,
        MobaPresentationTags::Combat_Control_Apply,
        MobaPresentationTags::Ability_Cast_Start,
        MobaPresentationTags::Ability_Cast_Release,
        MobaPresentationTags::Ability_Projectile_Spawn,
        MobaPresentationTags::Ability_Area_Warning,
        MobaPresentationTags::Status_Apply,
        MobaPresentationTags::Status_Remove,
        MobaPresentationTags::Character_Death,
        MobaPresentationTags::Character_Respawn,
        MobaPresentationTags::Arena_Match_Start,
        MobaPresentationTags::Arena_Match_End,
        MobaPresentationTags::Arena_Score_Changed,
        MobaPresentationTags::Arena_Objective_Completed
    };
    TestEqual(TEXT("现行有效语义数量"), FMobaPresentationSemanticRegistry::GetAll().Num(), 16);
    for (const FGameplayTag& Tag : ExpectedSemantics)
    {
        TestNotNull(FString::Printf(TEXT("现行语义必须可查：%s"), *Tag.ToString()),
            FMobaPresentationSemanticRegistry::Find(Tag));
    }
    TestTrue(TEXT("Hit标签有效"), MobaPresentationTags::Combat_Hit.GetTag().IsValid());
    TestTrue(TEXT("历史暴击Native标签保持旧资产兼容"), MobaPresentationTags::Combat_Critical.GetTag().IsValid());
    TestNull(TEXT("历史暴击不再登记为有效表现语义"),
        FMobaPresentationSemanticRegistry::Find(MobaPresentationTags::Combat_Critical));
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
