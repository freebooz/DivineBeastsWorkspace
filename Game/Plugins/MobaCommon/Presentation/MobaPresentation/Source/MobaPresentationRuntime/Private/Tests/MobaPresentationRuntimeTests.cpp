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
    const FGamePlatformPresentationRequest Request =
        FMobaPresentationRequestBuilder::Build(Fact, 7);

    TestTrue(TEXT("请求应有效"), Request.IsValid());
    TestEqual(TEXT("WorldGeneration保持"), Request.WorldGeneration, 3);
    TestEqual(TEXT("RequestGeneration保持"), Request.RequestGeneration, 7);
    TestEqual(TEXT("Magnitude保持"), Request.Magnitude, 25.0f);
    return true;
}

#endif
