// SSLM三份演示子页回归；真实Monolith类与真实VM，不连接后端、不创建生产视觉树或假人工报告。
// 断言模型生命周期不被子页改变、解绑清除旧详情、实际三个资产可以初始化；失败意味着共享模型桥失效。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsDemoHubWidget.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "UI/DivineBeastsDemoHubScreen.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/NamedSlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInterface.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoPanelTest,
    "DivineBeasts.PluginDemos.SharedPanelLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoPanelTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    Model->BeginPage();const int32 Generation=Model->GetPageGeneration();
    for(const TCHAR* Suffix:{TEXT("Detail"),TEXT("Controls"),TEXT("Report")})
    {
        const FString Name=FString(TEXT("WBP_DBA_PluginDemo"))+Suffix;
        const FString Path=FString(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/"))+Name+TEXT(".")+Name+TEXT("_C");
        UClass* Class=LoadClass<UDivineBeastsDemoHubWidget>(nullptr,*Path);
        if(!TestNotNull(TEXT("真实独立演示子页"),Class))continue;
        TStrongObjectPtr<UDivineBeastsDemoHubWidget> Panel(NewObject<UDivineBeastsDemoHubWidget>(GetTransientPackage(),Class));
        Panel->Initialize();Panel->BindPresentationModel(Model.Get());
        TestEqual(TEXT("嵌入三页不能各自BeginPage"),Model->GetPageGeneration(),Generation);
        TestTrue(TEXT("子页绑定不结束Hub模型"),Model->IsPageActive());
        if(auto* Details=Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("PanelDetailsText"))))
            TestFalse(TEXT("活动详情读取真实空目录诊断，而非空占位"),Details->GetText().IsEmpty());
        Panel->BindPresentationModel(nullptr);
        if(auto* Details=Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("PanelDetailsText"))))
            TestTrue(TEXT("解绑后旧详情必须消失"),Details->GetText().ToString().Contains(TEXT("未激活")));
        TestTrue(TEXT("解绑子页不能结束Hub模型"),Model->IsPageActive());
    }
    Model->EndPage();return true;
}

// 从宿主公开入口清空活动VM，而不是直接解绑子页：重现宿主漏转发导致旧详情/参数/样板残留的问题。
// 值与瞬态样板只作为Editor测试输入，不写资产、不注册目录、不宣称案例已真实执行。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoHubClearedModelTest,
    "DivineBeasts.PluginDemos.HubClearedModel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoHubClearedModelTest::RunTest(const FString&)
{
    UClass* Class=LoadClass<UDivineBeastsDemoHubScreen>(nullptr,TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub.WBP_DBA_PluginDemoHub_C"));
    if(!TestNotNull(TEXT("真实演示中心宿主资产"),Class))return false;
    TStrongObjectPtr<UDivineBeastsDemoHubScreen> Hub(NewObject<UDivineBeastsDemoHubScreen>(GetTransientPackage(),Class));
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    Hub->Initialize();Hub->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"),Model.Get(),TEXT("PluginSearchInput"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    Hub->ActivateWidget();TestTrue(TEXT("测试前页面处于活动状态"),Hub->IsActivated());
    auto* Detail=Cast<UDivineBeastsDemoHubWidget>(Hub->GetWidgetFromName(TEXT("DemoDetailPage")));
    auto* Controls=Cast<UDivineBeastsDemoHubWidget>(Hub->GetWidgetFromName(TEXT("DemoControlsPage")));
    auto* Report=Cast<UDivineBeastsDemoHubWidget>(Hub->GetWidgetFromName(TEXT("DemoReportPage")));
    if(!TestNotNull(TEXT("真实详情子页"),Detail)||!TestNotNull(TEXT("真实控制子页"),Controls)||!TestNotNull(TEXT("真实报告子页"),Report))return false;
    auto* Details=Cast<UTextBlock>(Detail->GetWidgetFromName(TEXT("PanelDetailsText")));
    auto* ReportText=Cast<UTextBlock>(Report->GetWidgetFromName(TEXT("PanelReportText")));
    auto* Parameters=Cast<UListView>(Controls->GetWidgetFromName(TEXT("PanelParameterList")));
    auto* Sample=Cast<UNamedSlot>(Controls->GetWidgetFromName(TEXT("PanelSampleSlot")));
    auto* Execute=Cast<UButton>(Controls->GetWidgetFromName(TEXT("PanelExecuteButton")));
    auto* Plugins=Cast<UListView>(Hub->GetWidgetFromName(TEXT("PluginList")));
    if(!TestNotNull(TEXT("详情文本"),Details)||!TestNotNull(TEXT("报告文本"),ReportText)||!TestNotNull(TEXT("参数列表"),Parameters)||!TestNotNull(TEXT("样板槽"),Sample)||!TestNotNull(TEXT("执行按钮"),Execute)||!TestNotNull(TEXT("插件列表"),Plugins))return false;
    Details->SetText(FText::FromString(TEXT("旧模型详情夹具")));ReportText->SetText(FText::FromString(TEXT("旧模型报告夹具")));
    Parameters->SetListItems(TArray<UObject*>{NewObject<UObject>(Hub.Get())});
    Plugins->SetListItems(TArray<UObject*>{NewObject<UObject>(Hub.Get())});
    Sample->SetContent(NewObject<UDivineBeastsDemoHubWidget>(Hub.Get()));Execute->SetIsEnabled(true);
    Hub->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"),nullptr,TEXT("PluginSearchInput"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    TestFalse(TEXT("清空宿主模型立即撤下旧详情"),Details->GetText().ToString().Contains(TEXT("旧模型")));
    TestFalse(TEXT("清空宿主模型立即撤下旧报告"),ReportText->GetText().ToString().Contains(TEXT("旧模型")));
    TestEqual(TEXT("旧参数不能保留"),Parameters->GetNumItems(),0);
    TestEqual(TEXT("旧插件列表不能保留"),Plugins->GetNumItems(),0);
    TestNull(TEXT("旧样板必须从槽移除"),Sample->GetContent());
    TestFalse(TEXT("失效命令按钮必须禁用"),Execute->GetIsEnabled());
    const FString EmptyDetails=Details->GetText().ToString();
    Model->OnViewStateChanged.Broadcast(Model->GetRevision(),Model->GetPageGeneration());
    TestEqual(TEXT("旧模型广播不能恢复旧内容"),Details->GetText().ToString(),EmptyDetails);
    Hub->DeactivateWidget();return true;
}

// 第十五页的原生合同：十二张Image必须引用真实专用材质，返回按钮在无目录环境必须显示真实失败。
// 仅初始化真实保存蓝图的瞬态副本；不加载地图、不联网、不把静态资源检查冒充最终画面验收。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMaterialReviewContractTest,
    "DivineBeasts.PluginDemos.MaterialReviewContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMaterialReviewContractTest::RunTest(const FString&)
{
    UClass* Class=LoadClass<UDivineBeastsDemoHubWidget>(nullptr,TEXT("/DBAUIPack_Core/Development/MaterialLibrary/UI/WBP_DBA_MaterialLibraryReview.WBP_DBA_MaterialLibraryReview_C"));
    if(!TestNotNull(TEXT("真实材质库审核蓝图"),Class))return false;
    TStrongObjectPtr<UDivineBeastsDemoHubWidget> Page(NewObject<UDivineBeastsDemoHubWidget>(GetTransientPackage(),Class));
    TestTrue(TEXT("真实审核页初始化"),Page->Initialize());
    for(const TCHAR* Name:{TEXT("SDFRoundRect"),TEXT("GradientBackground"),TEXT("BorderShape"),TEXT("OutlineGlow"),TEXT("ButtonShimmer"),TEXT("IconMask"),
        TEXT("RadialCooldown"),TEXT("HealthGradient"),TEXT("EnergyBarFlow"),TEXT("SelectionGlow"),TEXT("UIRipple"),TEXT("UITransitionDissolve")})
    {
        auto* Image=Cast<UImage>(Page->GetWidgetFromName(FName(*(FString(TEXT("Preview_"))+Name))));
        if(!TestNotNull(*FString::Printf(TEXT("%s真实预览控件"),Name),Image))continue;
        auto* Material=Cast<UMaterialInterface>(Image->GetBrush().GetResourceObject());
        if(TestNotNull(*FString::Printf(TEXT("%s不能用纯染色占位"),Name),Material))
        {
            const FString Expected=FString::Printf(TEXT("/Game/Development/MaterialLibrary/UI/Materials/M_DBA_UI_%s_Review.M_DBA_UI_%s_Review"),Name,Name);
            TestEqual(TEXT("预览引用实际审核材质稳定身份"),Material->GetPathName(),Expected);
        }
    }
    auto* Return=Cast<UButton>(Page->GetWidgetFromName(TEXT("ReturnButton")));
    auto* Feedback=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("OverlayFeedbackText")));
    if(TestNotNull(TEXT("返回按钮"),Return)&&TestNotNull(TEXT("返回结果反馈"),Feedback))
    {
        TestTrue(TEXT("返回按钮实际接入原生委托"),Return->OnClicked.IsBound());
        Return->OnClicked.Broadcast();
        TestTrue(TEXT("无目录时返回失败必须显示真实目录诊断"),Feedback->GetText().ToString().Contains(TEXT("演示目录")));
    }
    return true;
}
#endif
