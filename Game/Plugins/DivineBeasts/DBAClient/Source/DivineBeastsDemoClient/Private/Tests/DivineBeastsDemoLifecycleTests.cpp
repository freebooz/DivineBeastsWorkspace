// SSLM演示生命周期回归；纯可控时钟和实例注册表，无真实睡眠、地图、生产写入。
// 验证重复/迟到完成、停止超时、世界销毁和跨实例注销；原生运行待统一构建。
#if WITH_DEV_AUTOMATION_TESTS
#include "Demo/DivineBeastsDemoRunState.h"
#include "Scenarios/DivineBeastsDemoAdapterRegistry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoLifecycleTest, "DivineBeasts.PluginDemos.Lifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoLifecycleTest::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    FDivineBeastsDemoRunToken Token;
    Token.RunId = FGuid::NewGuid(); Token.WorldId = FGuid::NewGuid(); Token.Generation = 1;
    FRunState State;
    TestTrue(TEXT("开始准备"), State.Begin(Token, TEXT("Test.Normal"), TEXT("Test.Feature"), 100.0, 10.0));
    TestFalse(TEXT("同世界第二可写案例拒绝"), State.Begin(Token, TEXT("Other"), TEXT("Test.Feature"), 100.0, 10.0));
    TestTrue(TEXT("同步准备回调可接受"), State.CompletePreparation(Token, FGamePlatformResult::Success(), 100.0));
    TestFalse(TEXT("两次准备完成拒绝"), State.CompletePreparation(Token, FGamePlatformResult::Success(), 100.0));
    uint64 Serial = 0, Duplicate = 0;
    TestTrue(TEXT("首个命令登记"), State.BeginOperation(Token, Serial));
    TestFalse(TEXT("忙碌写入拒绝"), State.BeginOperation(Token, Duplicate));
    TestTrue(TEXT("命令完成一次"), State.CompleteOperation(Token, Serial, FGamePlatformResult::Success()));
    TestFalse(TEXT("重复命令完成不覆盖"), State.CompleteOperation(Token, Serial, FGamePlatformResult::Success()));
    TestTrue(TEXT("取消进入停止"), State.BeginStop(Token, FGamePlatformResult::Cancelled(TEXT("用户取消")), 101.0, 5.0));
    TestFalse(TEXT("重复停止不重复清理"), State.BeginStop(Token, FGamePlatformResult::Success(), 101.0, 5.0));
    TestFalse(TEXT("停止中旧操作拒绝"), State.CompleteOperation(Token, Serial, FGamePlatformResult::Success()));
    TestTrue(TEXT("清理截止失败"), State.Expire(106.0));
    TestEqual(TEXT("超时不是正常完成"), State.GetSnapshot().State, EDivineBeastsDemoRunState::Failed);
    TestFalse(TEXT("超时后迟到停止不能成功"), State.CompleteStop(Token, FGamePlatformResult::Success(), 106.0));
    ++Token.Generation; Token.RunId = FGuid::NewGuid();
    TestTrue(TEXT("新运行可开始"), State.Begin(Token, TEXT("Test.Timeout"), TEXT("Test.Feature"), 200.0, 2.0));
    TestTrue(TEXT("准备超时明确失败"), State.Expire(202.0));
    TestFalse(TEXT("准备截止后的迟到成功拒绝"), State.CompletePreparation(Token, FGamePlatformResult::Success(), 202.0));
    State.AbandonWorld();
    TestEqual(TEXT("世界退出保存取消"), State.GetSnapshot().State, EDivineBeastsDemoRunState::Cancelled);
    TestFalse(TEXT("退出后回调不复活"), State.CompletePreparation(Token, FGamePlatformResult::Success(), 203.0));
    ++Token.Generation; Token.RunId = FGuid::NewGuid();
    TestTrue(TEXT("开始完成与计时器竞态夹具"), State.Begin(Token, TEXT("Test.LateCompletion"), TEXT("Test.Feature"), 300.0, 2.0));
    TestFalse(TEXT("计时器尚未投递也拒绝准备逾期成功"), State.CompletePreparation(Token, FGamePlatformResult::Success(), 302.0));
    TestEqual(TEXT("保存真实准备超时"), State.GetSnapshot().OperationResult.Code, FName(TEXT("Demo.PrepareTimeout")));
    TestTrue(TEXT("逾期准备仍清理"), State.BeginStop(Token, State.GetSnapshot().OperationResult, 302.0, 5.0));
    TestFalse(TEXT("计时器尚未投递也拒绝清理逾期成功"), State.CompleteStop(Token, FGamePlatformResult::Success(), 307.0));
    TestEqual(TEXT("保存真实清理超时"), State.GetSnapshot().CleanupResult.Code, FName(TEXT("Demo.StopTimeout")));

    FAdapterRegistry A, B;
    auto Factory = [](UWorld&) -> TUniquePtr<IDivineBeastsDemoScenarioAdapter> { return nullptr; };
    const auto Handle = A.Register(TEXT("TestAdapter"), Factory);
    TestTrue(TEXT("实例注册接受"), Handle.IsValid());
    TestFalse(TEXT("同身份注册拒绝"), A.Register(TEXT("TestAdapter"), Factory).IsValid());
    TestFalse(TEXT("跨实例注销拒绝"), B.Unregister(Handle).IsSuccess());
    TestTrue(TEXT("完整句柄注销成功"), A.Unregister(Handle).IsSuccess());
    TestFalse(TEXT("旧注销不能操作下一注册"), A.Unregister(Handle).IsSuccess());
    return true;
}
#endif
