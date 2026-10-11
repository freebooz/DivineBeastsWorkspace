// 纯路由回归覆盖前端/传输/错误/注销不能开世界菜单，以及系统页输入和不暂停合同；不替代真实UMG运行。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSystemMenuRoutingTest, "DivineBeasts.UI.SystemMenu.WorldRouting", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSystemMenuRoutingTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    TestFalse(TEXT("缺省启动不得打开世界菜单"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bAuthenticated = true;
    for (const TCHAR* Step : {TEXT("DBA.Flow.Authentication"), TEXT("DBA.Flow.CharacterEntry"), TEXT("DBA.Flow.TransferWorld"), TEXT("DBA.Flow.WorldReady")})
    {
        State.CurrentStep = Step;
        TestFalse(TEXT("前端与加载步骤保留原页取消，不开世界菜单"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    }
    State.CurrentStep = TEXT("DBA.Flow.InWorld");
    TestTrue(TEXT("已认证空闲InWorld可以打开"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bBusy = true;
    TestFalse(TEXT("忙碌拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bBusy = false; State.Loading.bIsLoading = true;
    TestFalse(TEXT("旅行拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.Loading.bIsLoading = false; State.PageState = EDivineBeastsUIPageState::Error;
    TestFalse(TEXT("错误拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.PageState = EDivineBeastsUIPageState::Ready; State.bAuthenticated = false;
    TestFalse(TEXT("注销拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    for (const TCHAR* Id : {TEXT("UI.Screen.SystemMenu"), TEXT("UI.Screen.Settings")})
    {
        const auto* Entry = FDivineBeastsUIScreenCatalog::Find(Id);
        TestNotNull(TEXT("真实系统功能有稳定目录"), Entry);
        if (!Entry) continue;
        TestEqual(TEXT("只消费UI输入"), Entry->InputMode, EGamePlatformUIInputMode::UIOnly);
        TestEqual(TEXT("联网世界不Pause"), Entry->PausePolicy, EGamePlatformUIPausePolicy::Never);
        TestFalse(TEXT("旅行不保留捕获页面"), Entry->bSurvivesTravel);
        TestEqual(TEXT("系统层栈"), Entry->Layer, EGamePlatformUILayer::System);
    }
    return true;
}
#endif
