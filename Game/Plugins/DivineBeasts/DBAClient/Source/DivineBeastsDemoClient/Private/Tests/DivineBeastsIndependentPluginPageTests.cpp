// 独立插件页安全回归：真实VM/目录事件/页面对象，不加载资产、不联网、不代替人工签收。
// 捕获缺少固定身份防护、旧订阅回调覆盖新页及入口绕过显式资格的回归。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsDemoHubScreen.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsIndependentPluginPageTest,
    "DivineBeasts.PluginDemos.IndependentPluginPage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsIndependentPluginPageTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UDivineBeastsDemoCatalogSubsystem> Catalog(NewObject<UDivineBeastsDemoCatalogSubsystem>(Instance.Get()));
    Catalog->Plugins.AddDefaulted_GetRef().PluginId = TEXT("GamePlatformCore");
    Catalog->Plugins.AddDefaulted_GetRef().PluginId = TEXT("GamePlatformUI");
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    TestFalse(TEXT("非法插件路径不能配置为固定身份"), Model->ConfigurePluginContext(TEXT("/Game/Other")));
    TestTrue(TEXT("固定身份配置先于目录绑定"), Model->ConfigurePluginContext(TEXT("GamePlatformCore")));
    Model->BindCatalog(Catalog.Get()); Model->BeginPage();
    FDivineBeastsDemoFilter Search; Search.SearchText = TEXT("GamePlatformUI"); Search.Layer = TEXT("Project");
    Model->SetFilter(Search);
    auto Snapshot = Model->GetMenuSnapshot();
    TestEqual(TEXT("搜索/层筛选不能将固定页换到别的插件"), Snapshot.Plugins.Num(), 1);
    TestEqual(TEXT("固定页仍显示其原PluginId"), Snapshot.SelectedPluginId, FName(TEXT("GamePlatformCore")));
    TestFalse(TEXT("拒绝跨插件选择"), Model->SelectPlugin(TEXT("GamePlatformUI")));
    TestFalse(TEXT("活动已绑定页不可改固定身份"), Model->ConfigurePluginContext(TEXT("GamePlatformUI")));
    Model->Snapshot.Features.AddDefaulted_GetRef().Feature.FeatureId = TEXT("UI.Foreign");
    Model->Snapshot.Features.Last().Feature.PluginId = TEXT("GamePlatformUI");
    TestFalse(TEXT("旧值项不能跨PluginId选择功能"), Model->SelectFeature(TEXT("UI.Foreign")));
    Model->Snapshot.Scenarios.AddDefaulted_GetRef().Scenario.ScenarioId = TEXT("UI.Foreign.Normal");
    Model->Snapshot.Scenarios.Last().Scenario.FeatureId = TEXT("UI.Foreign");
    TestFalse(TEXT("关联案例不能越过固定插件执行归属"), Model->SelectScenario(TEXT("UI.Foreign.Normal")));
    const auto OldNotification = Catalog->OnCatalogChanged;
    Model->EndPage();
    const int32 InactiveRevision = Model->GetRevision();
    OldNotification.Broadcast();
    TestEqual(TEXT("失活后已投递旧目录回调无效"), Model->GetRevision(), InactiveRevision);
    TestTrue(TEXT("失活清除旧参数投影"), Model->GetMenuSnapshot().Features.IsEmpty());
    TestFalse(TEXT("失活页不能选择固定插件"), Model->SelectPlugin(TEXT("GamePlatformCore")));
    TestFalse(TEXT("失活页不能执行"), Model->RequestOperation(TEXT("Execute"), {}));
    Model->BeginPage();
    const int32 ReboundRevision = Model->GetRevision();
    OldNotification.Broadcast();
    TestEqual(TEXT("上一页代次不能刷新重开页面"), Model->GetRevision(), ReboundRevision);
    Catalog->OnCatalogChanged.Broadcast();
    TestEqual(TEXT("重新激活只订阅一次"), Model->GetRevision(), ReboundRevision + 1);
    Model->BindCatalog(Catalog.Get());
    const int32 RebindRevision = Model->GetRevision();
    Catalog->OnCatalogChanged.Broadcast();
    TestEqual(TEXT("同目录重绑不会重复订阅"), Model->GetRevision(), RebindRevision + 1);

    TStrongObjectPtr<UDivineBeastsDemoHubScreen> Page(NewObject<UDivineBeastsDemoHubScreen>());
    Page->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"), Model.Get(), NAME_None,
        EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    Page->DemoModel = Model.Get();
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Replacement(NewObject<UDivineBeastsDemoHubViewModel>());
    Page->InitializeActivatableViewModel(Replacement.Get());
    Page->OnViewModelStateChanged(0, Model->GetPageGeneration());
    TestFalse(TEXT("换VM后旧模型不得恢复命令源"), Page->DemoModel == Model.Get());

    TStrongObjectPtr<UWorld> World(NewObject<UWorld>());
    World->SetGameInstance(Instance.Get());
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> Context(NewObject<UDivineBeastsDemoWorldSubsystem>(World.Get()));
    Context->SourceWorld = World.Get(); Context->OverlayGeneration = 4;
    Context->LastFeedback = FText::FromString(TEXT("当前页状态"));
    Context->OnScenarioOverlayReady(3, {}, FGamePlatformResult::Failure(TEXT("Old.Load"), TEXT("迟到失败")));
    TestEqual(TEXT("旧资源回调不能覆盖新页结果"), Context->GetLastFeedback().ToString(), FString(TEXT("当前页状态")));
    Context->PendingScreenRequestId = FGuid::NewGuid();
    Context->OnDemoScreenOpened(FGuid::NewGuid(), TEXT("DivineBeasts.Demo.Hub"), Page.Get());
    TestFalse(TEXT("旧请求GUID不能接管新活动页"), Context->ActiveScreen.IsValid());
    TestFalse(TEXT("页面清理中拒绝重入释放"), (Context->bReleasingDemoOverlay = true, Context->ReleaseDemoOverlay()));
    Context->bReleasingDemoOverlay = false;
    Catalog->PendingPluginPageId = TEXT("GamePlatformUI");
    Catalog->bPendingPluginPage = true;
    FDivineBeastsDemoRunSnapshot Failure;
    Failure.CleanupResult = FGamePlatformResult::Failure(TEXT("Demo.TestCleanupFailed"), TEXT("保留清理失败"));
    Catalog->OnScenarioStopped(*World, Failure);
    TestFalse(TEXT("清理失败撤销切页请求"), Catalog->bPendingPluginPage);
    TestTrue(TEXT("清理失败保留可读诊断"), Catalog->GetLastOperationMessage().ToString().Contains(TEXT("清理失败")));

    const FString Hub = FDivineBeastsPluginDemoMode::GetHubMapPackage();
    TestTrue(TEXT("合法开发Hub与显式资格可请求插件直达"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("插件直达不能单独取得免登录资格"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, false, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("镜头审核地图不能取得独立插件直达资格"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false,
            FDivineBeastsPluginDemoMode::GetCameraReviewMapPackage(), TEXT("GamePlatformCore")));
    for (const TCHAR* Invalid : {TEXT(""), TEXT("None"), TEXT("/Game/Core"), TEXT("Core UI"), TEXT("中文")})
        TestFalse(TEXT("非法直接PluginId拒绝"),
            FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false, Hub, Invalid));
    TestFalse(TEXT("发布构建不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(false, true, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("专服不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, false, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("冲突模式不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, true, Hub, TEXT("GamePlatformCore")));
    Model->EndPage();
    return true;
}
#endif
