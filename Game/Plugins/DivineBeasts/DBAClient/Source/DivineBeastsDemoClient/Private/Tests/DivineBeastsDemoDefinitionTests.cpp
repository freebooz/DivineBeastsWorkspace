// SSLM项目开发目录合同回归；仅构造瞬态测试定义，不创建UE资产或启动世界。
// 验证版本/身份/关系/数值拒绝路径；需要真实Editor构建后运行，源码存在不等于通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace
{
/** 构造独立有效夹具；默认值是开发输入，不是实际玩家或已验收插件事实。 */
void PopulateDemoCatalog(UDivineBeastsDemoCatalogDefinition& Catalog)
{
    FGamePlatformId::TryCreate(TEXT("divinebeasts.demo"), TEXT("catalog"), 1, Catalog.LogicalId);
    Catalog.SourceFingerprint = FString::ChrN(64, TEXT('a'));
    auto& Plugin = Catalog.Plugins.AddDefaulted_GetRef();
    Plugin.PluginId = TEXT("GamePlatformUI");
    Plugin.DescriptorPath = TEXT("Game/Plugins/GamePlatform/Presentation/GamePlatformUI/GamePlatformUI.uplugin");
    Plugin.ArchitectureLayer = TEXT("Platform");
    Plugin.PluginKind = TEXT("code");
    auto& Feature = Catalog.Features.AddDefaulted_GetRef();
    Feature.FeatureId = TEXT("GamePlatformUI.ResourceBar");
    Feature.PluginId = Plugin.PluginId;
    Feature.Title = FText::FromString(TEXT("资源条测试"));
    Feature.Purpose = TEXT("验证定义结构，不执行UI。");
    Feature.OwnerPath = TEXT("Game/Plugins/GamePlatform/Presentation/GamePlatformUI");
    Feature.ScenarioIds.Add(TEXT("GamePlatformUI.ResourceBar.Normal"));
    auto& Scenario = Catalog.Scenarios.AddDefaulted_GetRef();
    Scenario.ScenarioId = Feature.ScenarioIds[0];
    Scenario.FeatureId = Feature.FeatureId;
    Scenario.Title = FText::FromString(TEXT("正常输入"));
    Scenario.AdapterId = TEXT("ResourceBar");
    Scenario.TemplateId = TEXT("UIWidgetGallery");
    Scenario.RunMode = TEXT("fixture");
    Scenario.Targets.Add(TEXT("Editor"));
    Scenario.RequiredPlugins.Add(Plugin.PluginId);
    Scenario.Preconditions.Add(TEXT("开发测试环境"));
    Scenario.MapAsset = FSoftObjectPath(TEXT("/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.L_DBA_PluginDemoHub"));
    Scenario.Operations = {TEXT("Execute"), TEXT("Stop")};
    Scenario.PrepareTimeoutSeconds = 10.0;
    Scenario.StopTimeoutSeconds = 10.0;
    Scenario.ReviewCaseIds.Add(TEXT("GamePlatformUI.ResourceBar.Normal.Review"));
    auto& Review = Catalog.ReviewCases.AddDefaulted_GetRef();
    Review.ReviewCaseId = Scenario.ReviewCaseIds[0];
    Review.ScenarioId = Scenario.ScenarioId;
    Review.FeatureId = Feature.FeatureId;
    Review.Steps.Add(TEXT("提交50/100"));
    Review.ExpectedResult = TEXT("比例0.5");
    Review.RequiredEvidence.Add(TEXT("log"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoDefinitionTest, "DivineBeasts.PluginDemos.Definition",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsDemoDefinitionTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoCatalogDefinition> Catalog(NewObject<UDivineBeastsDemoCatalogDefinition>());
    PopulateDemoCatalog(*Catalog);
    TestTrue(TEXT("Schema2关系有效"), Catalog->ValidateDefinition().IsSuccess());
    for (int32 Schema : {1, 3})
    {
        Catalog->DataVersion.SchemaVersion = Schema;
        TestFalse(TEXT("拒绝其他结构版本"), Catalog->ValidateDefinition().IsSuccess());
    }
    Catalog->DataVersion.SchemaVersion = 2;
    const auto OriginalFeature = Catalog->Features[0];
    Catalog->Features.Add(OriginalFeature);
    TestEqual(TEXT("重复功能身份拒绝"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.DuplicateIdentity")));
    Catalog->Features.Pop();
    const auto OriginalScenario = Catalog->Scenarios[0];
    Catalog->Scenarios.Add(OriginalScenario);
    TestEqual(TEXT("重复案例身份拒绝"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.DuplicateIdentity")));
    Catalog->Scenarios.Pop();
    for (double Timeout : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        Catalog->Scenarios[0].PrepareTimeoutSeconds = Timeout;
        TestFalse(TEXT("有限正超时约束"), Catalog->ValidateDefinition().IsSuccess());
    }
    Catalog->Scenarios[0].PrepareTimeoutSeconds = 10.0;
    Catalog->Scenarios[0].Operations.Reset();
    TestFalse(TEXT("不允许空操作伪支持"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].Operations = {TEXT("Execute"), TEXT("Stop")};
    Catalog->ReviewCases[0].RequiredEvidence.Reset();
    TestFalse(TEXT("用例必须声明证据"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->ReviewCases[0].RequiredEvidence.Add(TEXT("log"));

    auto& Parameter = Catalog->Scenarios[0].Parameters.AddDefaulted_GetRef();
    Parameter.ParameterId = TEXT("Count");
    Parameter.Unit = TEXT("个");
    Parameter.DefaultSource = TEXT("仅测试夹具");
    Parameter.DefaultValue.Type = TEXT("integer");
    Parameter.DefaultValue.IntegerValue = 50;
    Parameter.Minimum = 0;
    Parameter.Maximum = 100;
    TestTrue(TEXT("合法整数默认值"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.Maximum = 40;
    TestFalse(TEXT("越界默认值拒绝"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.Maximum = 9007199254740992.0;
    Parameter.DefaultValue.IntegerValue = 9007199254740992LL;
    TestFalse(TEXT("整数精度边界拒绝"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].Parameters.Reset();

    auto Second = Catalog->Scenarios[0];
    Second.ScenarioId = TEXT("GamePlatformUI.ResourceBar.InvalidMaximum");
    Second.ReviewCaseIds = {TEXT("GamePlatformUI.ResourceBar.InvalidMaximum.Review")};
    auto Review = Catalog->ReviewCases[0];
    Review.ReviewCaseId = Second.ReviewCaseIds[0];
    Review.ScenarioId = Second.ScenarioId;
    Catalog->Features[0].ScenarioIds.Add(Second.ScenarioId);
    Catalog->Scenarios.Add(Second);
    Catalog->ReviewCases.Add(Review);
    TestTrue(TEXT("同地图的两个独立案例允许"), Catalog->ValidateDefinition().IsSuccess());
    auto& Route = Catalog->LegacyMapRoutes.AddDefaulted_GetRef();
    Route.MapAsset = Second.MapAsset;
    Route.ScenarioIds = Catalog->Features[0].ScenarioIds;
    TestFalse(TEXT("一对多不能自动选择"), Catalog->ValidateDefinition().IsSuccess());
    Route.bRequiresSelection = true;
    TestTrue(TEXT("明确要求选择后允许"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].RequiredPlugins = {TEXT("UnknownPlugin")};
    TestFalse(TEXT("依赖未知插件拒绝"), Catalog->ValidateDefinition().IsSuccess());
    return true;
}
#endif
