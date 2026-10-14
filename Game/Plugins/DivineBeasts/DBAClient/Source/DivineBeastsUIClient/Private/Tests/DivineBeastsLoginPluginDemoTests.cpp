// SSLM开发入口失败关闭回归：无有效世界时不得创建进程，默认状态不得虚报启动成功。
// 该测试不打开窗口、不接后端；真实按钮/资源/进程启动需要独立端到端验证。
#include "Development/DivineBeastsPluginDemoLauncher.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBALoginPluginDemoNoWorldTest,
    "DivineBeasts.PluginDev.LoginEntry.NoWorldFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDBALoginPluginDemoNoWorldTest::RunTest(const FString&)
{
    UGameInstance* Instance = NewObject<UGameInstance>();
    auto* Launcher = NewObject<UDivineBeastsPluginDemoLauncher>(Instance);
    TestFalse(TEXT("无世界不能开放登录开发入口"), Launcher->IsEntryAvailable());
    Launcher->RequestLaunch();
    TestFalse(TEXT("拒绝时没有进程句柄"), Launcher->IsBusy());
    TestTrue(TEXT("拒绝不发布虚假启动状态"), Launcher->GetStatusText().IsEmpty());
    return true;
}
#endif
