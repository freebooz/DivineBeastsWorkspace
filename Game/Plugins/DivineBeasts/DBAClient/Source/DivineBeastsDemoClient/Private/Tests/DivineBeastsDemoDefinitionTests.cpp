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
    Parameter.DefaultValue.Type = EDivineBeastsDemoParameterType::Integer;
    Parameter.DefaultValue.IntegerValue = 50;
    Parameter.Minimum = 0;
    Parameter.Maximum = 100;
    TestTrue(TEXT("合法整数默认值"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.Maximum = 40;
    TestFalse(TEXT("越界默认值拒绝"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.Maximum = 9007199254740992.0;
    Parameter.DefaultValue.IntegerValue = 9007199254740992LL;
    TestFalse(TEXT("整数精度边界拒绝"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.DefaultValue.Type = static_cast<EDivineBeastsDemoParameterType>(255);
    TestFalse(TEXT("非法参数枚举拒绝，不落入默认成功分支"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.DefaultValue.Type = EDivineBeastsDemoParameterType::Boolean;
    TestTrue(TEXT("布尔类型只解释布尔载荷"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.DefaultValue.Type = EDivineBeastsDemoParameterType::Identifier;
    Parameter.DefaultValue.IdentifierValue = FName(TEXT("None"));
    TestFalse(TEXT("标识默认值拒绝UE空名"), Catalog->ValidateDefinition().IsSuccess());
    Parameter.DefaultValue.IdentifierValue = TEXT("ValidChoice");
    TestTrue(TEXT("合法标识参数"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].Parameters.Reset();

    Catalog->Scenarios[0].RequiredPlugins.Add(TEXT("gameplatformui"));
    TestFalse(TEXT("插件引用按FName大小写去重"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].RequiredPlugins.Pop();
    Catalog->Features[0].ScenarioIds.Add(TEXT("gameplatformui.resourcebar.normal"));
    TestFalse(TEXT("案例引用按FName大小写去重"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Features[0].ScenarioIds.Pop();
    Catalog->Scenarios[0].AdapterId = FName(TEXT("NONE"));
    TestFalse(TEXT("适配器拒绝空FName"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Scenarios[0].AdapterId = TEXT("ResourceBar");
    Catalog->ReviewCases[0].Version = MAX_int32;
    TestTrue(TEXT("用例最大int32版本合法"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->ReviewCases[0].Version = 0;
    TestFalse(TEXT("用例版本从1开始"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->ReviewCases[0].Version = 1;

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsIndependentPluginPageContractTest,
    "DivineBeasts.PluginDemos.IndependentPageContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsIndependentPluginPageContractTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoCatalogDefinition> Catalog(NewObject<UDivineBeastsDemoCatalogDefinition>());
    PopulateDemoCatalog(*Catalog);
    TestTrue(TEXT("旧目录未声明独立页时保持兼容"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->ManualPageContractVersion = 1;
    TestEqual(TEXT("新合同不能缺少实际页面声明"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.ManualPageIdentity")));
    Catalog->Plugins[0].TestPageAsset = FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GamePlatformUI.WBP_DBA_PluginTest_GamePlatformUI"));
    TestFalse(TEXT("Blueprint对象不能代替运行时生成类"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Plugins[0].TestPageAsset = FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GamePlatformUI.WBP_DBA_PluginTest_GamePlatformUI_C"));
    TestTrue(TEXT("独立页精确匹配插件身份"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->Plugins[0].TestPageAsset = FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GamePlatformData.WBP_DBA_PluginTest_GamePlatformData"));
    TestEqual(TEXT("禁止将另一插件的页面作为本页"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.ManualPageIdentity")));
    Catalog->Plugins[0].TestPageAsset = FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GamePlatformUI.WBP_DBA_PluginTest_GamePlatformUI_C"));
    FDivineBeastsDemoPluginEntry ThirdParty;
    ThirdParty.PluginId = TEXT("GlooPrint");
    ThirdParty.DescriptorPath = TEXT("Game/Plugins/ThirdParty/Editor/GlooPrint/GlooPrint.uplugin");
    ThirdParty.ArchitectureLayer = TEXT("Project");
    ThirdParty.TestPageAsset = FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GlooPrint.WBP_DBA_PluginTest_GlooPrint_C"));
    Catalog->Plugins.Add(ThirdParty);
    TestEqual(TEXT("第三方不能冒用一方来源"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.ManualPageIdentity")));
    Catalog->Plugins.Last().SourceKind = TEXT("third_party");
    TestTrue(TEXT("第三方独立页保留既有三层合同"), Catalog->ValidateDefinition().IsSuccess());
    Catalog->ManualPageContractVersion = 2;
    TestEqual(TEXT("未知独立页版本明确拒绝"), Catalog->ValidateDefinition().Code, FName(TEXT("Demo.ManualPageVersion")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsNamedOperationContractTest,
    "DivineBeasts.PluginDemos.NamedOperationContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsNamedOperationContractTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoCatalogDefinition> Catalog(NewObject<UDivineBeastsDemoCatalogDefinition>());
    PopulateDemoCatalog(*Catalog);auto& Scenario=Catalog->Scenarios[0];
    Scenario.Operations={TEXT("AcquireDefinition"),TEXT("Stop")};
    TestFalse(TEXT("具名操作必须有可读标签"),Catalog->ValidateDefinition().IsSuccess());
    Scenario.OperationLabels.Add(TEXT("AcquireDefinition"),FText::FromString(TEXT("申请定义")));
    TestTrue(TEXT("稳定具名操作及中文标签通过目录合同"),Catalog->ValidateDefinition().IsSuccess());
    Scenario.OperationLabels.Add(TEXT("Other"),FText::FromString(TEXT("其他")));
    TestFalse(TEXT("标签不能增添未声明操作"),Catalog->ValidateDefinition().IsSuccess());
    Scenario.OperationLabels.Remove(TEXT("Other"));Scenario.OperationLabels[TEXT("AcquireDefinition")]=FText::FromString(TEXT("   "));
    TestFalse(TEXT("空白操作标签拒绝"),Catalog->ValidateDefinition().IsSuccess());
    Scenario.OperationLabels[TEXT("AcquireDefinition")]=FText::FromString(TEXT("申请定义"));
    Scenario.Operations[0]=TEXT("AcquireDefinition;quit");
    TestFalse(TEXT("脚本文本不是操作身份"),Catalog->ValidateDefinition().IsSuccess());
    return true;
}
#endif
