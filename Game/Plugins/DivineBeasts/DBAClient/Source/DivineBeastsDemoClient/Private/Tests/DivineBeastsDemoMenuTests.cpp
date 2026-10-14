// SSLM菜单生命周期回归；无世界/HTTP测试拒绝和空投影，完整列表身份由Catalog回归覆盖。
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "UI/DivineBeastsDemoParameterRow.h"
#include "Catalog/DivineBeastsDemoCatalogValidation.h"
#include "Scenarios/DivineBeastsDemoCoreIdentityScenario.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoMenuTest, "DivineBeasts.PluginDemos.Menu",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoMenuTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    TestFalse(TEXT("无目录拒绝绑定"), Model->BindCatalog(nullptr));
    Model->BeginPage();
    FDivineBeastsDemoFilter Filter; Filter.SearchText = TEXT("不存在的项目"); Model->SetFilter(Filter);
    TestEqual(TEXT("空筛选无假插件"), Model->GetMenuSnapshot().Plugins.Num(), 0);
    TestFalse(TEXT("未审计不显示已知分母"), Model->GetMenuSnapshot().bHasKnownDenominator);
    TestFalse(TEXT("中文名不能充当功能ID"), Model->SelectFeature(TEXT("资源条")));
    TestFalse(TEXT("无服务不受理案例"), Model->RequestScenario(TEXT("Missing.Normal")));
    Model->EndPage();
    TestFalse(TEXT("失活页面拒绝新操作"), Model->RequestOperation(TEXT("Execute"), {}));
    TestFalse(TEXT("失活页面不请求返回"), Model->RequestReturn());
    Model->UnbindCatalog();
    // 引擎Lex转换可接受数值前缀；此回归要求编辑器完整语法消费，而不是50abc被当50。
    TStrongObjectPtr<UDivineBeastsDemoParameterItem> Parameter(NewObject<UDivineBeastsDemoParameterItem>());
    Parameter->Definition.bWritable = true; Parameter->Definition.Minimum = -100; Parameter->Definition.Maximum = 100;
    Parameter->Definition.DefaultValue.Type = EDivineBeastsDemoParameterType::Integer; Parameter->Value = Parameter->Definition.DefaultValue;
    TestTrue(TEXT("完整整数接受"), Parameter->SetTextValue(TEXT("50")));
    for (const TCHAR* Text : {TEXT("50abc"), TEXT("1.5"), TEXT(" 50"), TEXT("50 "), TEXT("9007199254740992"), TEXT("1e2"), TEXT("+")})
        TestFalse(TEXT("整数尾垃圾/溢出/小数/科学计数拒绝"), Parameter->SetTextValue(Text));
    Parameter->Definition.DefaultValue.Type = EDivineBeastsDemoParameterType::Number; Parameter->Value = Parameter->Definition.DefaultValue;
    TestTrue(TEXT("实数显式允许指数语法"), Parameter->SetTextValue(TEXT("5e1")));
    for (const TCHAR* Text : {TEXT("50abc"), TEXT("nan"), TEXT("inf"), TEXT("1e999"), TEXT("1e"), TEXT("."), TEXT("1 2")})
        TestFalse(TEXT("实数异常输入拒绝"), Parameter->SetTextValue(Text));
    Parameter->Definition.DefaultValue.Type = EDivineBeastsDemoParameterType::Identifier; Parameter->Value = Parameter->Definition.DefaultValue;
    TestFalse(TEXT("长身份在构造FName前拒绝"), Parameter->SetTextValue(FString::ChrN(NAME_SIZE, TEXT('A'))));
    TestFalse(TEXT("保留空名拒绝"), Parameter->SetTextValue(TEXT("NONE")));
    TestTrue(TEXT("直接命令和控件共用稳定身份规则"), DivineBeasts::Demo::IsStableIdentityText(TEXT("Sample.Choice-A_1")));
    for (const TCHAR* Text : {TEXT(""), TEXT("None"), TEXT("中文"), TEXT("/Game/Choice"), TEXT("1Choice"), TEXT("Choice Value")})
        TestFalse(TEXT("无UI直接身份入口同样拒绝非法文本"), DivineBeasts::Demo::IsStableIdentityText(Text));
    // 演示值调用实际Core公开API；这些输入不是账号、资产路径或网络权威身份。
    TArray<FDivineBeastsDemoParameterValue> IdentityValues; IdentityValues.SetNum(3);
    IdentityValues[0].Type = EDivineBeastsDemoParameterType::Identifier; IdentityValues[0].IdentifierValue = TEXT("Sample.Catalog");
    IdentityValues[1].Type = EDivineBeastsDemoParameterType::Identifier; IdentityValues[1].IdentifierValue = TEXT("Item");
    IdentityValues[2].Type = EDivineBeastsDemoParameterType::Integer; IdentityValues[2].IntegerValue = 1;
    FString Canonical;
    TestTrue(TEXT("真实Core身份回环"), DivineBeasts::Demo::EvaluateCoreIdentity(IdentityValues, Canonical).IsSuccess());
    TestEqual(TEXT("规范文本为平台小写格式"), Canonical, FString(TEXT("sample.catalog.item@1")));
    IdentityValues[1].IdentifierValue = TEXT("Invalid.Name");
    TestFalse(TEXT("单段名称不允许点"), DivineBeasts::Demo::EvaluateCoreIdentity(IdentityValues, Canonical).IsSuccess());
    TestTrue(TEXT("失败清空旧回环文本"), Canonical.IsEmpty());
    IdentityValues[1].IdentifierValue = TEXT("Item"); IdentityValues[2].IntegerValue = 0;
    TestFalse(TEXT("非法版本不夹紧"), DivineBeasts::Demo::EvaluateCoreIdentity(IdentityValues, Canonical).IsSuccess());
    IdentityValues[2].IntegerValue = 1; IdentityValues[2].Type = EDivineBeastsDemoParameterType::Number;
    TestFalse(TEXT("版本类型错不解释其他载荷"), DivineBeasts::Demo::EvaluateCoreIdentity(IdentityValues, Canonical).IsSuccess());
    IdentityValues.Pop();
    TestFalse(TEXT("缺参数拒绝"), DivineBeasts::Demo::EvaluateCoreIdentity(IdentityValues, Canonical).IsSuccess());
    return true;
}
#endif
