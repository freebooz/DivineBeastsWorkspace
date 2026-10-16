// SSLM开发快捷登录回归：只用明确夹具口令，不读取开发环境、连接后端或伪造认证成功。
// 验证完整配置拒绝、索引与名称重验证、真实WBP十按钮及激活/失活订阅；真实认证另由双客户端人工确认。
#if WITH_DEV_AUTOMATION_TESTS
#include "Development/DivineBeastsDevelopmentLoginAccounts.h"
#include "Screens/Login/DivineBeastsLoginScreen.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
    // 夹具永不写出文件或日志，名称/口令完全不来自用户账号。
    FString MakeDevelopmentAccountFixture(int32 Count = 10)
    {
        FString Json = TEXT("[");
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (Index) { Json += TEXT(","); }
            Json += FString::Printf(TEXT("{\"accountName\":\"fixture%02d\",\"password\":\"fixture-only-secret\"}"), Index + 1);
        }
        return Json + TEXT("]");
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentAccountParsingTest,
    "DivineBeasts.UI.Login.DevelopmentAccountConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentAccountParsingTest::RunTest(const FString&)
{
    using namespace DivineBeasts::UI::DevelopmentLogin;
    TArray<FString> Names;
    const FString Valid = MakeDevelopmentAccountFixture();
    TestTrue(TEXT("十个唯一账号可形成只读名称快照"), ReadAccountNames(Valid, Names));
    TestEqual(TEXT("名称数量"), Names.Num(), 10);
    FString Name, Password;
    TestTrue(TEXT("点击第十个按钮使用对应账号"), ReadCredential(Valid, 9, TEXT("fixture10"), Name, Password));
    TestEqual(TEXT("精确账号"), Name, FString(TEXT("fixture10")));
    TestTrue(TEXT("口令仅作为本次调用输出"), !Password.IsEmpty());
    TestFalse(TEXT("负数槽位拒绝"), ReadCredential(Valid, -1, TEXT("fixture01"), Name, Password));
    TestTrue(TEXT("失败清空所有凭据输出"), Name.IsEmpty() && Password.IsEmpty());
    TestFalse(TEXT("过期快照名称不能登录另一账号"), ReadCredential(Valid, 0, TEXT("fixture02"), Name, Password));
    TestTrue(TEXT("过期快照不泄露另一个口令"), Name.IsEmpty() && Password.IsEmpty());
    for (const FString& Invalid : {FString(TEXT("not-json")), MakeDevelopmentAccountFixture(9),
        MakeDevelopmentAccountFixture(11), Valid.Replace(TEXT("fixture10"), TEXT("FIXTURE01")),
        Valid.Replace(TEXT("fixture-only-secret"), TEXT(""))})
    {
        Names.Add(TEXT("stale"));
        TestFalse(TEXT("非法、缺项、重复或空口令完整拒绝"), ReadAccountNames(Invalid, Names));
        TestEqual(TEXT("失败不能保留半套或旧名称"), Names.Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentAccountBindingsTest,
    "DivineBeasts.UI.Login.DevelopmentAccountWidgetLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentAccountBindingsTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsLoginScreen>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login.WBP_DBA_UI_Login_C"));
    if (!TestNotNull(TEXT("真实已保存登录蓝图"), Class)) { return false; }
    TStrongObjectPtr<UDivineBeastsLoginScreen> Screen(NewObject<UDivineBeastsLoginScreen>(GetTransientPackage(), Class));
    Screen->Initialize(); Screen->ActivateWidget();
    ON_SCOPE_EXIT { Screen->DeactivateWidget(); };
    auto* Panel = Screen->GetWidgetFromName(TEXT("DevelopmentTestAccountsPanel"));
    if (!TestNotNull(TEXT("真实十账号容器"), Panel)) { return false; }
    // 测试显式注入无秘密名称，不修改进程环境，避免污染其他原生回归。
    DivineBeasts::UI::DevelopmentLogin::ReadAccountNames(MakeDevelopmentAccountFixture(), Screen->DevelopmentAccountNames);
    Screen->RefreshDevelopmentAccounts();
    TestEqual(TEXT("完整名称快照显示容器"), Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
    for (int32 Index = 0; Index < 10; ++Index)
    {
        auto* Button = Cast<UButton>(Screen->GetWidgetFromName(FName(*FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index + 1))));
        auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(FName(*FString::Printf(TEXT("DevelopmentAccount%02dLabel"), Index + 1))));
        if (!TestNotNull(TEXT("按钮"), Button) || !TestNotNull(TEXT("账号名称"), Label)) { return false; }
        TestEqual(TEXT("按钮标签来自名称快照"), Label->GetText().ToString(), Screen->DevelopmentAccountNames[Index]);
        TestTrue(TEXT("激活后真实点击事件已绑定"), Button->OnClicked.IsBound());
        TestFalse(TEXT("没有权威ViewModel时禁止快捷登录"), Button->GetIsEnabled());
    }
    Screen->BindLoginEvents();
    Screen->DeactivateWidget();
    TestEqual(TEXT("离页清理非秘密账号快照"), Screen->DevelopmentAccountNames.Num(), 0);
    for (int32 Index = 0; Index < 10; ++Index)
    {
        auto* Button = CastChecked<UButton>(Screen->GetWidgetFromName(FName(*FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index + 1))));
        TestFalse(TEXT("离页完整解绑"), Button->OnClicked.IsBound());
    }
    Screen->ActivateWidget();
    auto* First = CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("DevelopmentAccount01Button")));
    TestTrue(TEXT("再次激活重新配对绑定"), First->OnClicked.IsBound());
    Screen->DeactivateWidget();
    TestFalse(TEXT("再次失活没有残留"), First->OnClicked.IsBound());
    return true;
}
#endif
