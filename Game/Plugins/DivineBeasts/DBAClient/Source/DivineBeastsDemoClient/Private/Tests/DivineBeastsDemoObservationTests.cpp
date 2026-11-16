// 服务事件观察不能改写命令结果，也不能跨运行/退出后恢复旧界面。
#include "Demo/DivineBeastsDemoRunState.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoObservationTest,
    "DivineBeasts.PluginDemos.Observation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoObservationTest::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    FDivineBeastsDemoRunToken Token;
    Token.RunId=FGuid::NewGuid(); Token.WorldId=FGuid::NewGuid(); Token.Generation=1;
    FRunState State;
    TestTrue(TEXT("开始本运行"),State.Begin(Token,TEXT("Test.Service"),TEXT("Test.Feature"),100,10));
    TestTrue(TEXT("准备首次服务快照可通知"),State.RecordObservation(Token,TEXT("Loading"),101));
    TestEqual(TEXT("首次观察序号"),State.GetSnapshot().ObservationRevision,1);
    TestFalse(TEXT("重复相同观察不重复刷新"),State.RecordObservation(Token,TEXT("Loading"),102));
    TestEqual(TEXT("观察不补执行次数"),State.GetSnapshot().CompletedOperations,int64(0));
    TestEqual(TEXT("观察不能伪造准备完成"),State.GetSnapshot().OperationResult.Status,EGamePlatformResultStatus::NotExecuted);
    auto Wrong=Token; Wrong.WorldId=FGuid::NewGuid();
    TestFalse(TEXT("其他World通知拒绝"),State.RecordObservation(Wrong,TEXT("Ready"),103));
    FRunState TimedOut; auto OtherToken=Token; OtherToken.RunId=FGuid::NewGuid(); OtherToken.WorldId=FGuid::NewGuid();
    TimedOut.Begin(OtherToken,TEXT("Test.Timeout"),TEXT("Test.Feature"),100,10);
    TestFalse(TEXT("超时后的准备观察拒绝"),TimedOut.RecordObservation(OtherToken,TEXT("Ready"),110));
    TestTrue(TEXT("真正准备回调开放操作"),State.CompletePreparation(Token,FGamePlatformResult::Success(),105));
    uint64 Serial=0; State.BeginOperation(Token,Serial);
    const auto Failure=FGamePlatformResult::Failure(TEXT("Test.CommandFailed"),TEXT("真实命令失败"));
    TestTrue(TEXT("真实命令结果写入"),State.CompleteOperation(Token,Serial,Failure));
    TestTrue(TEXT("无在途命令时服务事件仍能通知"),State.RecordObservation(Token,TEXT("Disconnected"),106));
    TestEqual(TEXT("事件不覆盖原失败码"),State.GetSnapshot().OperationResult.Code,Failure.Code);
    TestEqual(TEXT("事件不补操作次数"),State.GetSnapshot().CompletedOperations,int64(1));
    TestFalse(TEXT("超长观察拒绝"),State.RecordObservation(Token,FString::ChrN(4097,TEXT('x')),106));
    State.BeginStop(Token,FGamePlatformResult::Cancelled(TEXT("Stop")),107,5);
    TestFalse(TEXT("停止中不接受旧服务通知"),State.RecordObservation(Token,TEXT("Ready"),108));
    State.CompleteStop(Token,State.GetStopAttemptSerial(),FGamePlatformResult::Success(),108);
    TestFalse(TEXT("终态不接受旧服务通知"),State.RecordObservation(Token,TEXT("Ready"),109));
    ++Token.Generation; Token.RunId=FGuid::NewGuid();
    State.Begin(Token,TEXT("Test.Service"),TEXT("Test.Feature"),110,10);
    TestEqual(TEXT("新运行清空旧服务观察"),State.GetSnapshot().ObservationRevision,0);
    TestTrue(TEXT("新运行首观察"),State.RecordObservation(Token,TEXT("New"),111));
    State.AbandonWorld();
    TestFalse(TEXT("World退出后通知拒绝"),State.RecordObservation(Token,TEXT("Ready"),112));
    return true;
}
#endif
