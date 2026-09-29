#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Schema/GamePlatformPCGSchema.h"
#include "Services/GamePlatformPCGLinearRules.h"
#include "Services/GamePlatformPCGPriorityRules.h"
#include "Services/GamePlatformPCGTemplateContract.h"
#include "Types/GamePlatformPCGDomainIds.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGSchemaContractTest,
    "GamePlatform.PCG.Schema.V1Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGSchemaContractTest::RunTest(const FString&)
{
    const FGamePlatformPCGSchemaVersion Version = FGamePlatformPCGSchema::CurrentVersion();
    TestEqual(TEXT("Schema主版本固定为1"), Version.Major, 1);
    TestTrue(TEXT("核心字段已注册"), FGamePlatformPCGSchema::IsKnownAttribute(FGamePlatformPCGAttr::LayerName));
    TestTrue(TEXT("排除字段已注册"), FGamePlatformPCGSchema::IsKnownAttribute(FGamePlatformPCGAttr::ExcludeMask));
    TestTrue(TEXT("未登记Pcg字段必须拒绝"),
        !FGamePlatformPCGSchema::ValidateAttributeName(TEXT("Pcg.Unknown.Illegal")).IsSuccess());
    TestTrue(TEXT("非Pcg前缀必须拒绝"),
        !FGamePlatformPCGSchema::ValidateAttributeName(TEXT("Other.Layer")).IsSuccess());

    const UEnum* PrimitiveEnum = StaticEnum<EGamePlatformPCGPrimitive>();
    TestNotNull(TEXT("Primitive枚举存在"), PrimitiveEnum);
    if (PrimitiveEnum)
    {
        // UENUM末尾包含引擎生成的_MAX；真实原语必须严格为P0-P9十项。
        TestEqual(TEXT("P0-P9共十个真实原语"), PrimitiveEnum->NumEnums() - 1, 10);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGDomainAndPriorityTest,
    "GamePlatform.PCG.Domain.PriorityContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGDomainAndPriorityTest::RunTest(const FString&)
{
    TestTrue(TEXT("Forest.Canopy属于稳定领域ID"), FGamePlatformPCGDomainIds::IsKnown(TEXT("Forest.Canopy")));
    TestTrue(TEXT("Agri.Crop属于稳定领域ID"), FGamePlatformPCGDomainIds::IsKnown(TEXT("Agri.Crop")));
    TestFalse(TEXT("未知领域ID拒绝"), FGamePlatformPCGDomainIds::IsKnown(TEXT("DivineBeasts.Village.Peach")));

    TestTrue(TEXT("高优先级且达到Mask阈值时挖洞"), FGamePlatformPCGPriorityRules::ShouldCarve(30, 70, 1.0f, 0.5f));
    TestFalse(TEXT("低优先级不能反切高优先级"), FGamePlatformPCGPriorityRules::ShouldCarve(70, 30, 1.0f, 0.5f));
    TestFalse(TEXT("同优先级不互切"), FGamePlatformPCGPriorityRules::ShouldCarve(50, 50, 1.0f, 0.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGLinearRulesTest,
    "GamePlatform.PCG.Linear.SpanRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGLinearRulesTest::RunTest(const FString&)
{
    TArray<FGamePlatformPCGSpanMeshRule> Rules;
    const auto AddRule = [&Rules](FName MeshSetId, float MinLengthCm, float MaxLengthCm)
    {
        FGamePlatformPCGSpanMeshRule Rule;
        Rule.MeshSetId = MeshSetId;
        Rule.MinLengthCm = MinLengthCm;
        Rule.MaxLengthCm = MaxLengthCm;
        Rules.Add(Rule);
    };
    AddRule(TEXT("Span.200"), 150.0f, 250.0f);
    AddRule(TEXT("Span.100"), 90.0f, 110.0f);
    AddRule(TEXT("Span.Flexible"), 50.0f, 300.0f);

    FName Selected;
    TestTrue(TEXT("100cm跨度存在匹配"), FGamePlatformPCGLinearRules::SelectSpanMeshByLength(100.0f, Rules, Selected));
    TestEqual(TEXT("优先选择最窄覆盖区间"), Selected, FName(TEXT("Span.100")));
    TestTrue(TEXT("达到柱距时保留柱"), FGamePlatformPCGLinearRules::ShouldKeepPost(200.0f, 200.0f));
    TestFalse(TEXT("未达到柱距时过滤柱"), FGamePlatformPCGLinearRules::ShouldKeepPost(120.0f, 200.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGTemplateContractTest,
    "GamePlatform.PCG.Template.ContractIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGTemplateContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("曲面散布模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_ScatterSurface")));
    TestTrue(TEXT("闭合围合模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_EnclosureClosed")));
    TestTrue(TEXT("栏杆附着模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_RailingAttached")));
    TestFalse(TEXT("城市模板尚未进入1.0批准合同"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_UrbanBlock")));
    return true;
}

#endif
