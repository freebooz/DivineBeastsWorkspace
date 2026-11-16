// SSLM项目开发菜单选择回归：Editor原生测试，以真实ListView选择委托触发页面→VM路径。
// 仅使用瞬态控件和值夹具，不制作/保存视觉资产，不联网；不代替真实Slate键盘输入与多视口验证。
// 页面持有测试控件，模型持有只读菜单快照；全部游戏线程同步，作用域结束释放，不保存跨世界引用。
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/DivineBeastsDemoHubScreen.h"
#include "UI/DivineBeastsDemoMenuItem.h"
#include "Components/ListView.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoKeyboardSelectionTest,
    "DivineBeasts.PluginDemos.KeyboardSelectionBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoKeyboardSelectionTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsDemoHubScreen> Page(NewObject<UDivineBeastsDemoHubScreen>());
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    Page->PluginList = NewObject<UListView>(Page.Get());
    Page->FeatureList = NewObject<UListView>(Page.Get());
    Page->ScenarioList = NewObject<UListView>(Page.Get());
    Page->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"), Model.Get(), TEXT("PluginSearchInput"),
        EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    Page->NativeOnInitialized();
    TestTrue(TEXT("门户插件项激活必须接入页面打开回调"), Page->PluginList->OnItemClicked().IsBoundToObject(Page.Get()));
    TestFalse(TEXT("功能项点击不绑定门户插件打开回调"), Page->FeatureList->OnItemClicked().IsBoundToObject(Page.Get()));
    Page->DemoModel = Model.Get();
    Model->BeginPage();
    const auto SeedValues = [&Model]()
    {
        // 无Catalog时选择会刷新为空并递增修订；因此以修订变化验证真实命令抵达，而非断言伪目录成功。
        Model->Snapshot = {};
        Model->Snapshot.Plugins.AddDefaulted_GetRef().PluginId = TEXT("Sample.Plugin");
        Model->Snapshot.Features.AddDefaulted_GetRef().Feature.FeatureId = TEXT("Sample.Feature");
        Model->Snapshot.Scenarios.AddDefaulted_GetRef().Scenario.ScenarioId = TEXT("Sample.Scenario");
    };
    auto* Item = NewObject<UDivineBeastsDemoMenuItem>(Page.Get());
    const TArray<TPair<UListView*, FName>> Cases = {
        {Page->PluginList.Get(), TEXT("Sample.Plugin")}, {Page->FeatureList.Get(), TEXT("Sample.Feature")},
        {Page->ScenarioList.Get(), TEXT("Sample.Scenario")}};
    for (const auto& Case : Cases)
    {
        SeedValues(); Item->Identity = Case.Value;
        const int32 Before = Model->GetRevision();
        Case.Key->OnItemSelectionChanged().Broadcast(Item);
        TestEqual(TEXT("选择变化必须抵达实际VM一次"), Model->GetRevision(), Before + 1);
        // 鼠标选择已会发出SelectionChanged；再走Clicked不能重复重建/发送第二个选择命令。
        SeedValues();
        const int32 BeforeClick = Model->GetRevision();
        Case.Key->OnItemClicked().Broadcast(Item);
        TestEqual(TEXT("点击不重复转发选择"), Model->GetRevision(), BeforeClick);
    }
    SeedValues(); Item->Identity = TEXT("Sample.Plugin");
    Page->bSynchronizing = true;
    const int32 BeforeRefresh = Model->GetRevision();
    Page->PluginList->OnItemSelectionChanged().Broadcast(Item);
    TestEqual(TEXT("列表刷新同步事件不得发命令"), Model->GetRevision(), BeforeRefresh);
    Page->bSynchronizing = false;
    Model->EndPage();
    Page->PluginList->OnItemSelectionChanged().Broadcast(Item);
    TestEqual(TEXT("失活模型不得接收选择命令"), Model->GetRevision(), BeforeRefresh);
    return true;
}
#endif
