// SSLM项目演示目录回归；瞬态定义和插件值夹具只验证身份/发布/查询，不加载UE关卡。
// 失败意味着可能发布半目录、隐藏禁用插件或按中文名称错误选取案例；原生执行待统一构建。
#if WITH_DEV_AUTOMATION_TESTS
#include "Catalog/DivineBeastsDemoCatalogSnapshot.h"
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 测试专用完整关系：同名两功能、同图两案例，全部拥有独立检查合同。 */
void PopulateCatalogQueryFixture(UDivineBeastsDemoCatalogDefinition& Definition)
{
    FGamePlatformId::TryCreate(TEXT("divinebeasts.demo"), TEXT("catalog"), 1, Definition.LogicalId);
    Definition.SourceFingerprint = FString::ChrN(64, TEXT('a'));
    auto& Plugin = Definition.Plugins.AddDefaulted_GetRef();
    Plugin.PluginId = TEXT("GamePlatformUI");
    Plugin.DescriptorPath = TEXT("Game/Plugins/GamePlatform/Presentation/GamePlatformUI/GamePlatformUI.uplugin");
    Plugin.ArchitectureLayer = TEXT("Platform");
    for (const TCHAR* Suffix : {TEXT("First"), TEXT("Second")})
    {
        auto& Feature = Definition.Features.AddDefaulted_GetRef();
        Feature.FeatureId = FName(*(FString(TEXT("GamePlatformUI.")) + Suffix));
        Feature.PluginId = Plugin.PluginId;
        Feature.Title = FText::FromString(TEXT("相同中文名称"));
        Feature.Purpose = TEXT("仅测试身份查询");
        Feature.OwnerPath = TEXT("Game/Plugins/GamePlatform/Presentation/GamePlatformUI");
        auto& Scenario = Definition.Scenarios.AddDefaulted_GetRef();
        Scenario.ScenarioId = FName(*(Feature.FeatureId.ToString() + TEXT(".Normal")));
        Scenario.FeatureId = Feature.FeatureId;
        Scenario.Title = FText::FromString(TEXT("相同案例名称"));
        Scenario.AdapterId = TEXT("ResourceBar");
        Scenario.TemplateId = TEXT("UIWidgetGallery");
        Scenario.RunMode = TEXT("fixture");
        Scenario.Targets = {TEXT("Editor")};
        Scenario.RequiredPlugins = {Plugin.PluginId};
        Scenario.Preconditions = {TEXT("测试夹具，不是运行场景")};
        Scenario.MapAsset = FSoftObjectPath(TEXT("/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.L_DBA_PluginDemoHub"));
        Scenario.Operations = {TEXT("Execute"), TEXT("Stop")};
        Scenario.PrepareTimeoutSeconds = 10.0;
        Scenario.StopTimeoutSeconds = 10.0;
        Feature.ScenarioIds = {Scenario.ScenarioId};
        auto& Review = Definition.ReviewCases.AddDefaulted_GetRef();
        Review.ReviewCaseId = FName(*(Scenario.ScenarioId.ToString() + TEXT(".Review")));
        Review.ScenarioId = Scenario.ScenarioId;
        Review.FeatureId = Feature.FeatureId;
        Review.Steps = {TEXT("按稳定ID选择")};
        Review.ExpectedResult = TEXT("只返回指定案例");
        Review.RequiredEvidence = {TEXT("log")};
        Scenario.ReviewCaseIds = {Review.ReviewCaseId};
    }
    auto& Route = Definition.LegacyMapRoutes.AddDefaulted_GetRef();
    Route.MapAsset = Definition.Scenarios[0].MapAsset;
    Route.ScenarioIds = {Definition.Scenarios[0].ScenarioId, Definition.Scenarios[1].ScenarioId};
    Route.bRequiresSelection = true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoCatalogTest, "DivineBeasts.PluginDemos.Catalog",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoCatalogTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoCatalogDefinition> Definition(NewObject<UDivineBeastsDemoCatalogDefinition>());
    PopulateCatalogQueryFixture(*Definition);
    FDivineBeastsDemoPlugin Discovered;
    Discovered.PluginId = TEXT("GamePlatformUI");
    Discovered.bEnabled = false;
    DivineBeasts::Demo::FCatalogSnapshot Snapshot;
    TestTrue(TEXT("禁用插件仍发布只读信息"), Snapshot.Publish(*Definition, {Discovered}).IsSuccess());
    TestEqual(TEXT("同中文名保留两个功能"), Snapshot.GetFeatures(Discovered.PluginId).Num(), 2);
    TestEqual(TEXT("按第一个ID选取"), Snapshot.GetFeatureScenarios(TEXT("GamePlatformUI.First")).Num(), 1);
    TestEqual(TEXT("筛选不更改总数"), Snapshot.GetAllFeatures().Num(), 2);
    FName Selected;
    TestFalse(TEXT("旧路径一对多拒绝任取首项"), Snapshot.ResolveIdentity(
        FName(*Definition->Scenarios[0].MapAsset.GetLongPackageName()), Selected).IsSuccess());
    TestTrue(TEXT("独立案例ID可解析"), Snapshot.ResolveIdentity(Definition->Scenarios[1].ScenarioId, Selected).IsSuccess());
    TestEqual(TEXT("解析保持正确身份"), Selected, Definition->Scenarios[1].ScenarioId);
    Definition->Scenarios[0].AdapterId = NAME_None;
    TestFalse(TEXT("非法候选拒绝发布"), Snapshot.Publish(*Definition, {Discovered}).IsSuccess());
    TestEqual(TEXT("失败保留此前完整快照"), Snapshot.GetAllFeatures().Num(), 2);
    Definition->Scenarios[0].AdapterId = TEXT("ResourceBar");
    TestFalse(TEXT("插件集合不完整拒绝"), Snapshot.Publish(*Definition, {}).IsSuccess());
    return true;
}
#endif
