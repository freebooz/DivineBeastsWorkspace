// 清理失败继续拥有本次运行，重试有独立序号，旧Stop回调不得完成新一轮清理。
#include "Demo/DivineBeastsDemoRunState.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoCleanupRetryTest,
    "DivineBeasts.PluginDemos.CleanupRetry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoCleanupRetryTest::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    FDivineBeastsDemoRunToken Token;Token.RunId=FGuid::NewGuid();Token.WorldId=FGuid::NewGuid();Token.Generation=1;
    FRunState State;State.Begin(Token,TEXT("Test.IO"),TEXT("Test.Feature"),100,10);
    State.CompletePreparation(Token,FGamePlatformResult::Success(),101);
    uint64 Operation=0;State.BeginOperation(Token,Operation);
    const auto Original=FGamePlatformResult::Failure(TEXT("Test.Original"),TEXT("原命令失败"));
    State.CompleteOperation(Token,Operation,Original);
    TestTrue(TEXT("首次停止受理"),State.BeginStop(Token,FGamePlatformResult::Cancelled(TEXT("用户取消")),102,5));
    const auto First=State.GetStopAttemptSerial();
    TestTrue(TEXT("真实清理失败被保留"),State.CompleteStop(Token,First,FGamePlatformResult::Failure(TEXT("Test.IOBusy"),TEXT("IO未完成")),103));
    TestTrue(TEXT("失败仍待清理"),State.GetSnapshot().bCleanupPending);
    TestEqual(TEXT("清理失败不覆盖原命令码"),State.GetSnapshot().OperationResult.Code,Original.Code);
    auto Other=Token;Other.RunId=FGuid::NewGuid();++Other.Generation;
    TestFalse(TEXT("待清理不可覆盖为新运行"),State.Begin(Other,TEXT("Other"),TEXT("Other.Feature"),104,10));
    TestTrue(TEXT("同一Run可显式重试停止"),State.BeginStop(Token,FGamePlatformResult::Success(),104,5));
    const auto Second=State.GetStopAttemptSerial();
    TestTrue(TEXT("重试序号前进"),Second>First);
    TestFalse(TEXT("迟到旧Stop不能确认本次清理"),State.CompleteStop(Token,First,FGamePlatformResult::Success(),105));
    TestTrue(TEXT("本次真实清理成功"),State.CompleteStop(Token,Second,FGamePlatformResult::Success(),105));
    TestFalse(TEXT("成功后不再持待清理所有权"),State.GetSnapshot().bCleanupPending);
    TestEqual(TEXT("重试不把原取消改成成功"),State.GetSnapshot().State,EDivineBeastsDemoRunState::Cancelled);
    TestTrue(TEXT("收敛后新运行允许"),State.Begin(Other,TEXT("Other"),TEXT("Other.Feature"),106,10));
    State.CompletePreparation(Other,FGamePlatformResult::Success(),107);
    State.BeginStop(Other,FGamePlatformResult::Success(),108,2);
    const auto TimedOut=State.GetStopAttemptSerial();State.Expire(110);
    TestTrue(TEXT("超时仍待清理"),State.GetSnapshot().bCleanupPending);
    TestFalse(TEXT("超时旧Stop结果拒绝"),State.CompleteStop(Other,TimedOut,FGamePlatformResult::Success(),111));
    TestTrue(TEXT("超时可重试"),State.BeginStop(Other,FGamePlatformResult::Success(),112,3));
    State.AbandonWorld();
    TestFalse(TEXT("World退出撤销重试资格"),State.GetSnapshot().bCleanupPending);
    TestFalse(TEXT("退出后迟到清理不能复活"),State.CompleteStop(Other,State.GetStopAttemptSerial(),FGamePlatformResult::Success(),113));
    return true;
}
#endif
