// 平台GI遥测生命周期回归：手控实际IGamePlatformTelemetrySink，不联网、不持有凭据，也不替代生产Sink。
// 验证Start/旧Flush/Shutdown/GetHealth/Submit同步关闭与换代、上下文边界不被单纯Sink换代取消，以及旧完成不能恢复新Sink调度；UE运行由统一验证执行。
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Sinks/GamePlatformTelemetrySink.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FTelemetryReentrySink final : public IGamePlatformTelemetrySink
{
public:
    TFunction<void()> OnStart, OnSubmit, OnFlush, OnShutdown;
    TFunction<void()> OnHealth;
    TArray<FGamePlatformTelemetrySubmitCompletion> Completions;
    bool bStartResult = true;
    bool bStopped = false;
    int32 Starts = 0, Submits = 0, Flushes = 0, Shutdowns = 0, SubmitsAfterShutdown = 0;
    mutable int32 HealthReads = 0;
    bool Start() override { ++Starts; bStopped = false; if (OnStart) { OnStart(); } return bStartResult; }
    void SubmitBatch(FGamePlatformTelemetryBatch, FGamePlatformTelemetrySubmitCompletion Completion) override
    { ++Submits; if (bStopped) { ++SubmitsAfterShutdown; } Completions.Add(MoveTemp(Completion)); if (OnSubmit) { OnSubmit(); } }
    void Flush() override { ++Flushes; if (OnFlush) { OnFlush(); } }
    void Shutdown(float) override { ++Shutdowns; bStopped = true; if (OnShutdown) { OnShutdown(); } }
    FGamePlatformTelemetrySinkStatus GetHealth() const override
    { ++HealthReads; if (OnHealth) { OnHealth(); } FGamePlatformTelemetrySinkStatus Status; Status.Health = bStopped ? EGamePlatformTelemetrySinkHealth::Stopped : EGamePlatformTelemetrySinkHealth::Healthy; return Status; }
};
struct FTelemetryServiceFixture
{
    TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> Service{NewObject<UGamePlatformTelemetrySubsystem>(Instance.Get())};
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    FTelemetryServiceFixture() { Service->Initialize(Collection); }
    ~FTelemetryServiceFixture() { Service->Deinitialize(); }
    EGamePlatformTelemetryRecordResult Record()
    { FGamePlatformTelemetryEvent Event; Event.EventName = TEXT("Telemetry.Foundation.ClientStarted"); return Service->RecordEvent(MoveTemp(Event)); }
    // 三个边界均从真实旧会话/世界及合法已接纳队列出发；不直接写子系统私有状态。
    void SetOldContext()
    {
        Service->BeginSession(TEXT("old-session"), TEXT("old-player"));
        Service->UpdateWorldContext(TEXT("old-map"), TEXT("old-world"), TEXT("old-experience"), TEXT("old-match"), TEXT("old-mode"));
        Service->UpdateCorrelationContext(TEXT("old-correlation"), TEXT("old-transaction"));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryStartCloseTest, "GamePlatform.Telemetry.Lifecycle.StartClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryStartCloseTest::RunTest(const FString&)
{
    FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    Sink->OnStart = [&Fixture]() { Fixture.Service->Deinitialize(); };
    TestFalse(TEXT("Start同步关闭后不得安装候选"), Fixture.Service->ConfigureSink(Sink));
    TestEqual(TEXT("已Start候选必须清理一次"), Sink->Shutdowns, 1);
    TestEqual(TEXT("关闭后的记录明确Disabled"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Disabled);
    Fixture.Service->SetEnabled(true);
    Fixture.Service->Initialize(Fixture.Collection);
    TestEqual(TEXT("关闭实例不能通过重复Initialize重开"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Disabled);
    auto Other = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    TestFalse(TEXT("关闭后拒绝重新配置"), Fixture.Service->ConfigureSink(Other));
    TestEqual(TEXT("被拒候选不得Start"), Other->Starts, 0);
    const auto Diagnostics = Fixture.Service->GetDiagnostics();
    TestFalse(TEXT("关闭不可重新启用"), Diagnostics.bEnabled); TestFalse(TEXT("关闭无刷新Ticker"), Diagnostics.bFlushScheduled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryShutdownReentryTest, "GamePlatform.Telemetry.Lifecycle.ShutdownReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryShutdownReentryTest::RunTest(const FString&)
{
    FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto Other = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    TestTrue(TEXT("旧Sink正常配置"), Fixture.Service->ConfigureSink(Sink));
    TestEqual(TEXT("先接纳合法事件"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Recorded);
    bool bReconfigured = true; bool bScheduledDuringShutdown = true; EGamePlatformTelemetryRecordResult RecordResult = EGamePlatformTelemetryRecordResult::Recorded;
    Sink->OnShutdown = [&]() { Fixture.Service->Deinitialize(); bReconfigured = Fixture.Service->ConfigureSink(Other); Fixture.Service->SetEnabled(true); RecordResult = Fixture.Record(); bScheduledDuringShutdown = Fixture.Service->GetDiagnostics().bFlushScheduled; };
    Fixture.Service->Deinitialize();
    TestFalse(TEXT("Shutdown内重配不能复活服务"), bReconfigured); TestEqual(TEXT("关闭期事件Disabled"), RecordResult, EGamePlatformTelemetryRecordResult::Disabled);
    TestFalse(TEXT("Shutdown回调内部无Ticker"), bScheduledDuringShutdown);
    TestEqual(TEXT("关闭期新Sink不Start"), Other->Starts, 0); TestEqual(TEXT("Deinitialize重入幂等"), Sink->Shutdowns, 1);
    TestEqual(TEXT("受控最终Drain可转交旧缓冲"), Sink->Submits, 1);
    const auto Diagnostics = Fixture.Service->GetDiagnostics(); TestFalse(TEXT("关闭期无Ticker"), Diagnostics.bFlushScheduled); TestFalse(TEXT("关闭期禁启用"), Diagnostics.bEnabled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryHealthReplacementTest, "GamePlatform.Telemetry.Lifecycle.HealthReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryHealthReplacementTest::RunTest(const FString&)
{
    FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    Fixture.Service->ConfigureSink(SinkA); Fixture.Record(); const int32 Depth = Fixture.Service->GetDiagnostics().BufferDepth;
    bool bReplaced = false;
    SinkA->OnHealth = [&]() { if (!bReplaced) { bReplaced = true; Fixture.Service->ConfigureSink(SinkB); } };
    TestFalse(TEXT("GetHealth换代使旧Flush停止"), Fixture.Service->FlushBestEffort());
    TestEqual(TEXT("已关闭A不得继续Submit"), SinkA->Submits, 0); TestEqual(TEXT("换代前队列不能被旧栈出队"), Fixture.Service->GetDiagnostics().BufferDepth, Depth);
    TestEqual(TEXT("A完整关闭一次"), SinkA->Shutdowns, 1); TestTrue(TEXT("B可以转交仍在队列的数据"), Fixture.Service->FlushBestEffort()); TestEqual(TEXT("B收到真实批次"), SinkB->Submits, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryConfigureNestedTest, "GamePlatform.Telemetry.Lifecycle.ConfigureNested", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryConfigureNestedTest::RunTest(const FString&)
{
    FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkC = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    Fixture.Service->ConfigureSink(SinkA); bool bNested = true;
    SinkB->OnStart = [&]() { bNested = Fixture.Service->ConfigureSink(SinkC); };
    TestTrue(TEXT("外层配置仍可安装B"), Fixture.Service->ConfigureSink(SinkB)); TestFalse(TEXT("正在配置时嵌套重配明确拒绝"), bNested); TestEqual(TEXT("嵌套被拒不Start"), SinkC->Starts, 0);
    TestTrue(TEXT("相同活跃Sink可确认且不重复Start"), Fixture.Service->ConfigureSink(SinkB)); TestEqual(TEXT("同对象不重复Start"), SinkB->Starts, 1);
    SinkB->bStopped = true; TestFalse(TEXT("同一Stopped Sink不得伪成功"), Fixture.Service->ConfigureSink(SinkB));
    auto Failed = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Failed->bStartResult = false;
    TestFalse(TEXT("启动失败不得伪成功"), Fixture.Service->ConfigureSink(Failed)); TestEqual(TEXT("启动失败保留B"), SinkB->Shutdowns, 0); TestEqual(TEXT("失败候选清理一次"), Failed->Shutdowns, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryExternalCloseTest, "GamePlatform.Telemetry.Lifecycle.ExternalClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryExternalCloseTest::RunTest(const FString&)
{
    {
        FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(Sink); Fixture.Record();
        Sink->OnSubmit = [&]() { Fixture.Service->Deinitialize(); };
        Fixture.Service->FlushBestEffort(); TestEqual(TEXT("Submit同步关闭只转交一个批次"), Sink->Submits, 1); TestFalse(TEXT("Submit关闭后不得安排Ticker"), Fixture.Service->GetDiagnostics().bFlushScheduled);
        Sink->Completions[0](true, false); TestFalse(TEXT("关闭后迟到完成不能调度"), Fixture.Service->GetDiagnostics().bFlushScheduled);
    }
    {
        FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(SinkA);
        SinkA->OnFlush = [&]() { Fixture.Service->Deinitialize(); };
        TestFalse(TEXT("旧Flush关闭后外层配置返回失败"), Fixture.Service->ConfigureSink(SinkB)); TestEqual(TEXT("退休A仍完整清理"), SinkA->Shutdowns, 1); TestEqual(TEXT("已发布B也随关闭清理"), SinkB->Shutdowns, 1);
    }
    return true;
}

// 仅测试访问撤销调度入口，制造有数据但无Ticker前置；不复制队列或生产调度实现。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryCompletionGenerationTest, "GamePlatform.Telemetry.Lifecycle.CompletionGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryCompletionGenerationTest::RunTest(const FString&)
{
    FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
    Fixture.Service->ConfigureSink(SinkA); Fixture.Record(); Fixture.Service->FlushBestEffort(); auto OldCompletion = SinkA->Completions[0];
    Fixture.Record(); Fixture.Service->ConfigureSink(SinkB); Fixture.Service->CancelScheduledFlush();
    OldCompletion(true, false); TestFalse(TEXT("A旧完成不得恢复B调度"), Fixture.Service->GetDiagnostics().bFlushScheduled);
    Fixture.Service->FlushBestEffort(); auto CurrentCompletion = SinkB->Completions[0]; Fixture.Record(); Fixture.Service->CancelScheduledFlush();
    CurrentCompletion(true, false); TestTrue(TEXT("当前Sink终态可唤醒剩余队列"), Fixture.Service->GetDiagnostics().bFlushScheduled);
    Fixture.Service->CancelScheduledFlush(); CurrentCompletion(true, false); TestFalse(TEXT("重复终态不得再次调度"), Fixture.Service->GetDiagnostics().bFlushScheduled);
    return true;
}

// GetHealth仅换输出器时，旧Flush必须停止，而账号/世界边界必须继续完成；否则残留旧玩家或旧世界身份。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryContextSinkReplacementTest, "GamePlatform.Telemetry.Lifecycle.ContextSinkReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryContextSinkReplacementTest::RunTest(const FString&)
{
    {
        FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
        Fixture.Service->ConfigureSink(SinkA); Fixture.SetOldContext(); TestEqual(TEXT("结束会话前有合法队列"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Recorded);
        bool bReplaced = false;
        SinkA->OnHealth = [&]() { if (!bReplaced) { bReplaced = true; TestTrue(TEXT("结束会话中换Sink成功"), Fixture.Service->ConfigureSink(SinkB)); } };
        Fixture.Service->EndSession(); const auto Context = Fixture.Service->GetContextSnapshot();
        TestTrue(TEXT("换Sink仍清旧会话"), Context.SessionId.IsEmpty()); TestTrue(TEXT("换Sink仍清旧匿名玩家"), Context.PseudonymousPlayerId.IsEmpty());
        TestTrue(TEXT("换Sink仍清旧链路及事务"), Context.CorrelationId.IsEmpty() && Context.TransactionId.IsEmpty());
        TestTrue(TEXT("结束会话清旧比赛"), Context.MatchId.IsEmpty() && Context.ArenaModeId.IsEmpty()); TestEqual(TEXT("结束会话保留当前地图"), Context.MapId, FString(TEXT("old-map")));
        TestEqual(TEXT("结束边界旧A不Submit"), SinkA->Submits, 0);
    }
    {
        FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
        Fixture.Service->ConfigureSink(SinkA); Fixture.SetOldContext(); TestEqual(TEXT("新会话前有合法队列"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Recorded);
        bool bReplaced = false;
        SinkA->OnHealth = [&]() { if (!bReplaced) { bReplaced = true; TestTrue(TEXT("新会话中换Sink成功"), Fixture.Service->ConfigureSink(SinkB)); } };
        Fixture.Service->BeginSession(TEXT("new-session"), TEXT("new-player")); const auto Context = Fixture.Service->GetContextSnapshot();
        TestEqual(TEXT("换Sink仍发布新会话"), Context.SessionId, FString(TEXT("new-session"))); TestEqual(TEXT("换Sink仍发布新匿名玩家"), Context.PseudonymousPlayerId, FString(TEXT("new-player")));
        TestTrue(TEXT("新会话清旧关联"), Context.CorrelationId.IsEmpty() && Context.TransactionId.IsEmpty()); TestEqual(TEXT("新会话边界旧A不Submit"), SinkA->Submits, 0);
    }
    {
        FTelemetryServiceFixture Fixture; auto SinkA = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); auto SinkB = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>();
        Fixture.Service->ConfigureSink(SinkA); Fixture.SetOldContext(); TestEqual(TEXT("旅行前有合法队列"), Fixture.Record(), EGamePlatformTelemetryRecordResult::Recorded);
        bool bReplaced = false;
        SinkA->OnHealth = [&]() { if (!bReplaced) { bReplaced = true; TestTrue(TEXT("旅行中换Sink成功"), Fixture.Service->ConfigureSink(SinkB)); } };
        Fixture.Service->BeforeWorldTravel(); const auto Context = Fixture.Service->GetContextSnapshot();
        TestTrue(TEXT("换Sink仍清旧地图世界体验"), Context.MapId.IsEmpty() && Context.WorldId.IsEmpty() && Context.ExperienceId.IsEmpty());
        TestTrue(TEXT("换Sink仍清旧比赛模式"), Context.MatchId.IsEmpty() && Context.ArenaModeId.IsEmpty()); TestEqual(TEXT("旅行保留会话"), Context.SessionId, FString(TEXT("old-session")));
        TestEqual(TEXT("旅行边界旧A不Submit"), SinkA->Submits, 0);
    }
    return true;
}

// 外部健康查询可同步发布真实后继上下文；旧边界返回后不能清新会话、复活已结束会话或删除新世界。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformTelemetryContextSuccessorTest, "GamePlatform.Telemetry.Lifecycle.ContextSuccessor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformTelemetryContextSuccessorTest::RunTest(const FString&)
{
    {
        FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(Sink); Fixture.SetOldContext(); Fixture.Record(); bool bReentered = false;
        Sink->OnHealth = [&]() { if (!bReentered) { bReentered = true; Fixture.Service->BeginSession(TEXT("inner-session"), TEXT("inner-player")); } };
        Fixture.Service->EndSession(); const auto Context = Fixture.Service->GetContextSnapshot();
        TestEqual(TEXT("旧End不得清后继Begin会话"), Context.SessionId, FString(TEXT("inner-session"))); TestEqual(TEXT("旧End不得清后继玩家"), Context.PseudonymousPlayerId, FString(TEXT("inner-player")));
        Sink->OnHealth = nullptr;
    }
    {
        FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(Sink); Fixture.SetOldContext(); Fixture.Record(); bool bReentered = false;
        Sink->OnHealth = [&]() { if (!bReentered) { bReentered = true; Fixture.Service->EndSession(); } };
        Fixture.Service->BeginSession(TEXT("outer-session"), TEXT("outer-player")); const auto Context = Fixture.Service->GetContextSnapshot();
        TestTrue(TEXT("后继End终态不能被旧Begin复活"), Context.SessionId.IsEmpty() && Context.PseudonymousPlayerId.IsEmpty());
        Sink->OnHealth = nullptr;
    }
    {
        FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(Sink); Fixture.SetOldContext(); Fixture.Record(); bool bReentered = false;
        Sink->OnHealth = [&]() { if (!bReentered) { bReentered = true; Fixture.Service->UpdateWorldContext(TEXT("inner-map"), TEXT("inner-world"), TEXT("inner-experience"), TEXT("inner-match"), TEXT("inner-mode")); } };
        Fixture.Service->BeforeWorldTravel(); const auto Context = Fixture.Service->GetContextSnapshot();
        TestEqual(TEXT("旧Travel不清后继地图"), Context.MapId, FString(TEXT("inner-map"))); TestEqual(TEXT("旧Travel不清后继世界"), Context.WorldId, FString(TEXT("inner-world")));
        TestEqual(TEXT("旧Travel不清后继体验"), Context.ExperienceId, FString(TEXT("inner-experience"))); TestEqual(TEXT("旧Travel不清后继比赛"), Context.MatchId, FString(TEXT("inner-match")));
        Sink->OnHealth = nullptr;
    }
    {
        FTelemetryServiceFixture Fixture; auto Sink = MakeShared<FTelemetryReentrySink, ESPMode::ThreadSafe>(); Fixture.Service->ConfigureSink(Sink); Fixture.SetOldContext(); Fixture.Record(); bool bReentered = false;
        Sink->OnHealth = [&]() { if (!bReentered) { bReentered = true; Fixture.Service->BeforeWorldTravel(); } };
        Fixture.Service->BeginSession(TEXT("outer-session"), TEXT("outer-player")); const auto Context = Fixture.Service->GetContextSnapshot();
        TestEqual(TEXT("后继Travel接管后旧Begin不得换玩家"), Context.SessionId, FString(TEXT("old-session"))); TestTrue(TEXT("后继Travel清世界终态保留"), Context.MapId.IsEmpty() && Context.WorldId.IsEmpty());
        Sink->OnHealth = nullptr;
    }
    return true;
}
#endif
