// SSLM三份演示子页回归；真实Monolith类与真实VM，不连接后端、不创建生产视觉树或假人工报告。
// 断言模型生命周期不被子页改变、解绑清除旧详情、实际三个资产可以初始化；失败意味着共享模型桥失效。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsDemoHubWidget.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
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
#endif
