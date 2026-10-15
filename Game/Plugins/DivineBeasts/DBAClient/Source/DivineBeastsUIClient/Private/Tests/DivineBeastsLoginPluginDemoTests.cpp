// SSLM开发入口失败关闭回归：无有效世界时不得创建进程，默认状态不得虚报启动成功。
// 该测试不打开窗口、不接后端；真实按钮/资源/进程启动需要独立端到端验证。
#include "Development/DivineBeastsPluginDemoLauncher.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
// 真实启动状态对象的生命周期回归，不创建外部进程、不伪造门户Ready或认证成功。
// 同步重复点击、其他请求/旧代次完成及GameInstance失活必须保持当前请求的所有权。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBALoginPluginDemoLaunchLifetimeTest,
    "DivineBeasts.PluginDev.LoginEntry.LaunchLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDBALoginPluginDemoLaunchLifetimeTest::RunTest(const FString&)
{
    FDivineBeastsPluginDemoLaunchState State;
    TestFalse(TEXT("初始没有活动启动"), State.IsBusy());
    TestTrue(TEXT("首次点击占有启动"), State.TryBegin());
    const FGuid FirstId = State.GetLaunchId();
    const uint64 FirstGeneration = State.GetGeneration();
    TestTrue(TEXT("开始后立即忙碌"), State.IsBusy());
    TestFalse(TEXT("重复点击不得分配第二请求"), State.TryBegin());
    TestEqual(TEXT("重复点击保留原身份"), State.GetLaunchId(), FirstId);
    TestFalse(TEXT("其他进程完成不得结束当前请求"), State.TryComplete(FGuid::NewGuid(), FirstGeneration));
    TestFalse(TEXT("过期代次不得结束当前请求"), State.TryComplete(FirstId, FirstGeneration + 1));
    TestTrue(TEXT("无关完成后仍忙碌"), State.IsBusy());
    TestTrue(TEXT("真实当前完成释放忙碌"), State.TryComplete(FirstId, FirstGeneration));
    TestFalse(TEXT("完成只接受一次"), State.TryComplete(FirstId, FirstGeneration));
    TestTrue(TEXT("结束后允许重试"), State.TryBegin());
    const FGuid CancelledId = State.GetLaunchId();
    const uint64 CancelledGeneration = State.GetGeneration();
    State.Invalidate();
    TestFalse(TEXT("失活立即清除本地忙碌"), State.IsBusy());
    TestFalse(TEXT("失活后迟到完成不能发布状态"), State.TryComplete(CancelledId, CancelledGeneration));
    TestTrue(TEXT("新生命周期允许新请求"), State.TryBegin());
    TestFalse(TEXT("旧生命周期不能完成新请求"), State.TryComplete(CancelledId, CancelledGeneration));
    TestTrue(TEXT("当前生命周期仍可正常完成"), State.TryComplete(State.GetLaunchId(), State.GetGeneration()));
    return true;
}

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
