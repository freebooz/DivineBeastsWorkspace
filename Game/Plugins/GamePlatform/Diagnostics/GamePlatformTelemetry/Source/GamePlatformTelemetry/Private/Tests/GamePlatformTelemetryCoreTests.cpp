#if WITH_DEV_AUTOMATION_TESTS

#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"
#include "Misc/AutomationTest.h"
#include "Privacy/GamePlatformTelemetryPrivacyFilter.h"
#include "Sampling/GamePlatformTelemetrySampling.h"
#include "Schema/GamePlatformTelemetrySchemaRegistry.h"
#include "Containers/Ticker.h"
#include "Sinks/GamePlatformTelemetryNetworkSink.h"
#include "Transport/GamePlatformTelemetryTransport.h"

namespace
{
/** 测试传输器：可模拟首次断线后恢复，或持续断线；不执行真实网络请求。 */
class FGamePlatformTelemetryRetryTestTransport final
    : public IGamePlatformTelemetryTransport
{
public:
    explicit FGamePlatformTelemetryRetryTestTransport(bool bInAlwaysRetryableFailure)
        : bAlwaysRetryableFailure(bInAlwaysRetryableFailure)
    {
    }

    virtual bool BeginSubmitBatch(
        const FGamePlatformTelemetryBatch&,
        FGamePlatformTelemetryTransportCompletion Completion) override
    {
        ++Attempts;
        FGamePlatformTelemetryTransportResult Result;
        const bool bRecovered = !bAlwaysRetryableFailure && Attempts >= 2;
        Result.bAccepted = bRecovered;
        Result.bRetryable = !bRecovered;
        Result.Error = bRecovered ? FString() : TEXT("simulated_disconnect");
        Completion(Result);
        return true;
    }

    virtual void CancelAll() override
    {
        ++CancelCalls;
    }

    int32 Attempts = 0;
    int32 CancelCalls = 0;

private:
    bool bAlwaysRetryableFailure = false;
};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryPrivacyAndSchemaTest,
    "GamePlatform.Telemetry.PrivacyAndSchema",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryPrivacyAndSchemaTest::RunTest(
    const FString&)
{
    const TSharedRef<
        FGamePlatformTelemetrySchemaRegistry,
        ESPMode::ThreadSafe> Registry =
        FGamePlatformTelemetrySchemaRegistry::
            CreateFoundationDefaults();

    const FGamePlatformTelemetryEventDefinition* Definition =
        Registry->FindEvent(
            TEXT("Commerce.Payment.VerificationResult"));

    TestNotNull(TEXT("Commerce事件Schema存在"), Definition);

    FGamePlatformTelemetryEvent Event;
    Event.EventName = TEXT("Commerce.Payment.VerificationResult");
    Event.SchemaVersion = 1;

    FGamePlatformTelemetryAttribute Result;
    Result.Key = TEXT("result");
    Result.Type = EGamePlatformTelemetryAttributeType::String;
    Result.PrivacyClass =
        EGamePlatformTelemetryPrivacyClass::Operational;
    Result.StringValue = TEXT("confirmed");
    Event.Attributes.Add(Result);

    FGamePlatformTelemetryLimits Limits;

    TestEqual(
        TEXT("白名单属性通过"),
        FGamePlatformTelemetryPrivacyFilter::ValidateEvent(
            Event,
            *Definition,
            Limits),
        EGamePlatformTelemetryRecordResult::Recorded);

    Result.Type = EGamePlatformTelemetryAttributeType::Double;
    Event.Attributes = {Result};
    TestEqual(
        TEXT("Schema声明String时错误类型必须拒绝"),
        FGamePlatformTelemetryPrivacyFilter::ValidateEvent(Event, *Definition, Limits),
        EGamePlatformTelemetryRecordResult::InvalidAttribute);
    Result.Type = EGamePlatformTelemetryAttributeType::String;
    Event.Attributes = {Result};

    Result.Key = TEXT("payment_receipt");
    Result.StringValue = TEXT("forbidden");
    Event.Attributes = {Result};

    TestEqual(
        TEXT("支付凭据字段拒绝"),
        FGamePlatformTelemetryPrivacyFilter::ValidateEvent(
            Event,
            *Definition,
            Limits),
        EGamePlatformTelemetryRecordResult::ForbiddenAttribute);

    const FGamePlatformTelemetryMetricDefinition* MetricDefinition =
        Registry->FindMetric(TEXT("server.frame_ms"));

    TestNotNull(TEXT("Metric定义存在"), MetricDefinition);

    FGamePlatformTelemetryMetric Metric;
    Metric.Name = TEXT("server.frame_ms");
    Metric.Type = EGamePlatformTelemetryMetricType::Histogram;
    Metric.Value = 10.0;
    Metric.Labels.Add(TEXT("player_id"), TEXT("high-cardinality"));

    TestEqual(
        TEXT("Metric禁止PlayerId标签"),
        FGamePlatformTelemetryPrivacyFilter::ValidateMetric(
            Metric,
            *MetricDefinition),
        EGamePlatformTelemetryRecordResult::MetricLabelNotAllowed);

    FGamePlatformTelemetryEventDefinition LateDefinition;
    LateDefinition.EventName = TEXT("Telemetry.Test.LateRegistration");
    Registry->Freeze();
    TestTrue(TEXT("Schema冻结状态可查询"), Registry->IsFrozen());
    Registry->RegisterEvent(MoveTemp(LateDefinition));
    TestNull(
        TEXT("冻结后不得再注册运行期Schema"),
        Registry->FindEvent(TEXT("Telemetry.Test.LateRegistration")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetrySamplingAndRateTest,
    "GamePlatform.Telemetry.SamplingAndRateLimit",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetrySamplingAndRateTest::RunTest(
    const FString&)
{
    const FGuid RecordId = FGuid::NewGuid();

    const bool First =
        FGamePlatformTelemetrySampler::ShouldSample(
            EGamePlatformTelemetrySamplingPolicy::
                DeterministicSessionSample,
            0.5,
            TEXT("seed"),
            TEXT("session-A"),
            TEXT("World.Load.Completed"),
            RecordId);

    const bool Second =
        FGamePlatformTelemetrySampler::ShouldSample(
            EGamePlatformTelemetrySamplingPolicy::
                DeterministicSessionSample,
            0.5,
            TEXT("seed"),
            TEXT("session-A"),
            TEXT("World.Load.Completed"),
            FGuid::NewGuid());

    TestEqual(
        TEXT("同Session确定性采样稳定"),
        First,
        Second);

    FGamePlatformTelemetryRateLimiter Limiter;
    TestTrue(
        TEXT("Burst第1次允许"),
        Limiter.TryConsume(TEXT("Event.A"), 1, 2, 1.0));
    TestTrue(
        TEXT("Burst第2次允许"),
        Limiter.TryConsume(TEXT("Event.A"), 1, 2, 1.0));
    TestFalse(
        TEXT("超过Burst被限流"),
        Limiter.TryConsume(TEXT("Event.A"), 1, 2, 1.0));
    TestTrue(
        TEXT("时间推进后补充Token"),
        Limiter.TryConsume(TEXT("Event.A"), 1, 2, 2.0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryBoundedBufferTest,
    "GamePlatform.Telemetry.BoundedBuffer",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryBoundedBufferTest::RunTest(
    const FString&)
{
    FGamePlatformTelemetryLimits Limits;
    // 生产Buffer明确把最小事件容量夹到16；测试必须使用真实有效下限，不能假定2条容量。
    Limits.MaxBufferEvents = 16;
    Limits.MaxBufferBytes = 4096;
    Limits.MaxBatchEvents = 1;
    Limits.MaxBatchBytes = 4096;
    Limits.MaxEventBytes = 2048;

    FGamePlatformTelemetryBoundedBuffer Buffer(Limits);

    FGamePlatformTelemetryEvent Critical;
    Critical.EventId = FGuid::NewGuid();
    Critical.EventName = TEXT("Telemetry.Foundation.ServerStarted");
    Critical.Priority =
        EGamePlatformTelemetryPriority::CriticalTelemetry;

    FGamePlatformTelemetryEvent Normal = Critical;
    Normal.EventId = FGuid::NewGuid();
    Normal.Priority = EGamePlatformTelemetryPriority::Normal;

    FGamePlatformTelemetryEvent Verbose = Critical;
    Verbose.EventId = FGuid::NewGuid();
    Verbose.Priority = EGamePlatformTelemetryPriority::Verbose;

    TestTrue(TEXT("Critical入队"), Buffer.EnqueueEvent(Critical, 256));
    for (int32 Index = 0; Index < 15; ++Index)
    {
        FGamePlatformTelemetryEvent Item = Normal;
        Item.EventId = FGuid::NewGuid();
        TestTrue(TEXT("Normal填满真实最小容量"), Buffer.EnqueueEvent(MoveTemp(Item), 256));
    }

    TestFalse(
        TEXT("Buffer满时Verbose不能驱逐更高优先级"),
        Buffer.EnqueueEvent(Verbose, 256));

    FGamePlatformTelemetryEvent NewCritical = Critical;
    NewCritical.EventId = FGuid::NewGuid();
    TestTrue(
        TEXT("Critical可驱逐较低优先级Normal"),
        Buffer.EnqueueEvent(NewCritical, 256));

    const FGamePlatformTelemetryDiagnostics Diagnostics =
        Buffer.GetDiagnostics();

    TestEqual(
        TEXT("Buffer始终有界"),
        Diagnostics.BufferDepth,
        16);

    TestTrue(
        TEXT("至少记录一次Drop"),
        Diagnostics.DroppedVerbose +
            Diagnostics.DroppedNormal +
            Diagnostics.DroppedCritical >= 1);

    FGamePlatformTelemetryBatch Batch;
    TestTrue(
        TEXT("构建Batch"),
        Buffer.BuildBatch({}, Batch));

    TestEqual(
        TEXT("Batch受MaxBatchEvents限制"),
        Batch.Events.Num(),
        1);

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryContextBatchBoundaryTest,
    "GamePlatform.Telemetry.ContextBatchBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryContextBatchBoundaryTest::RunTest(const FString&)
{
    FGamePlatformTelemetryLimits Limits;
    Limits.MaxBufferEvents = 8;
    Limits.MaxBufferBytes = 64 * 1024;
    Limits.MaxBatchEvents = 8;
    Limits.MaxBatchBytes = 64 * 1024;

    FGamePlatformTelemetryBoundedBuffer Buffer(Limits);
    FGamePlatformTelemetryEvent WorldA;
    WorldA.EventId = FGuid::NewGuid();
    WorldA.EventName = TEXT("Telemetry.Foundation.WorldLoaded");
    WorldA.Context.WorldId = TEXT("world-A");

    FGamePlatformTelemetryEvent WorldB = WorldA;
    WorldB.EventId = FGuid::NewGuid();
    WorldB.Context.WorldId = TEXT("world-B");

    TestTrue(TEXT("WorldA入队"), Buffer.EnqueueEvent(WorldA, 256));
    TestTrue(TEXT("WorldB入队"), Buffer.EnqueueEvent(WorldB, 256));

    FGamePlatformTelemetryBatch First;
    TestTrue(TEXT("构建第一上下文批次"), Buffer.BuildBatch({}, First));
    TestEqual(TEXT("上下文变化必须切批"), First.Events.Num(), 1);
    TestEqual(TEXT("第一批使用第一条记录上下文"), First.SourceContext.WorldId, FString(TEXT("world-A")));
    TestTrue(TEXT("批次内Event不再重复携带完整Context"), First.Events[0].Context.WorldId.IsEmpty());

    FGamePlatformTelemetryBatch Second;
    TestTrue(TEXT("构建第二上下文批次"), Buffer.BuildBatch({}, Second));
    TestEqual(TEXT("第二批只包含剩余上下文"), Second.Events.Num(), 1);
    TestEqual(TEXT("第二批上下文正确"), Second.SourceContext.WorldId, FString(TEXT("world-B")));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryMetricCoalescingTest,
    "GamePlatform.Telemetry.MetricCoalescing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryMetricCoalescingTest::RunTest(const FString&)
{
    FGamePlatformTelemetryLimits Limits;
    Limits.MaxBufferEvents = 32;
    Limits.MaxBufferBytes = 64 * 1024;
    Limits.MaxBatchEvents = 32;
    Limits.MaxBatchBytes = 64 * 1024;
    FGamePlatformTelemetryBoundedBuffer Buffer(Limits);

    FGamePlatformTelemetryContext Context;
    Context.WorldId = TEXT("world-coalesce");

    FGamePlatformTelemetryMetric Counter;
    Counter.Name = TEXT("server.telemetry.dropped_total");
    Counter.Type = EGamePlatformTelemetryMetricType::Counter;
    Counter.Value = 1.0;
    Counter.Context = Context;
    TestTrue(TEXT("Counter首次入队"), Buffer.EnqueueMetric(Counter, 128));
    Counter.Value = 2.0;
    TestTrue(TEXT("Counter第二次合并"), Buffer.EnqueueMetric(Counter, 128));
    Counter.Value = 3.0;
    TestTrue(TEXT("Counter第三次合并"), Buffer.EnqueueMetric(Counter, 128));

    FGamePlatformTelemetryMetric Gauge;
    Gauge.Name = TEXT("server.active_players");
    Gauge.Type = EGamePlatformTelemetryMetricType::Gauge;
    Gauge.Value = 10.0;
    Gauge.Context = Context;
    TestTrue(TEXT("Gauge首次入队"), Buffer.EnqueueMetric(Gauge, 128));
    Gauge.Value = 12.0;
    TestTrue(TEXT("Gauge覆盖最新值"), Buffer.EnqueueMetric(Gauge, 128));

    FGamePlatformTelemetryMetric Histogram;
    Histogram.Name = TEXT("server.frame_ms");
    Histogram.Type = EGamePlatformTelemetryMetricType::Histogram;
    Histogram.Value = 16.0;
    Histogram.Context = Context;
    TestTrue(TEXT("Histogram样本1入队"), Buffer.EnqueueMetric(Histogram, 128));
    Histogram.Value = 18.0;
    TestTrue(TEXT("Histogram样本2保持独立"), Buffer.EnqueueMetric(Histogram, 128));

    const FGamePlatformTelemetryDiagnostics Diagnostics = Buffer.GetDiagnostics();
    TestEqual(TEXT("Counter/Gauge共发生3次合并"), Diagnostics.CoalescedMetricTotal, static_cast<int64>(3));
    TestEqual(TEXT("实际Buffer只保留4条记录"), Diagnostics.BufferDepth, 4);

    FGamePlatformTelemetryBatch Batch;
    TestTrue(TEXT("构建合并后的批次"), Buffer.BuildBatch({}, Batch));
    TestEqual(TEXT("批次包含4条Metric"), Batch.Metrics.Num(), 4);
    TestEqual(TEXT("Counter值正确累加"), Batch.Metrics[0].Value, 6.0);
    TestEqual(TEXT("Gauge保留最新值"), Batch.Metrics[1].Value, 12.0);
    TestEqual(TEXT("Histogram样本1保持原值"), Batch.Metrics[2].Value, 16.0);
    TestEqual(TEXT("Histogram样本2保持原值"), Batch.Metrics[3].Value, 18.0);
    TestTrue(TEXT("批次Metric不重复携带完整Context"), Batch.Metrics[0].Context.WorldId.IsEmpty());
    TestEqual(TEXT("公共SourceContext保留世界身份"), Batch.SourceContext.WorldId, Context.WorldId);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryPrivacyBoundaryDiscardTest,
    "GamePlatform.Telemetry.PrivacyBoundaryDiscard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryPrivacyBoundaryDiscardTest::RunTest(const FString&)
{
    FGamePlatformTelemetryLimits Limits;
    Limits.MaxBufferEvents = 16;
    Limits.MaxBufferBytes = 64 * 1024;
    FGamePlatformTelemetryBoundedBuffer Buffer(Limits);

    FGamePlatformTelemetryEvent OldNormal;
    OldNormal.EventId = FGuid::NewGuid();
    OldNormal.EventName = TEXT("Telemetry.Foundation.ClientStarted");
    OldNormal.Priority = EGamePlatformTelemetryPriority::Normal;
    OldNormal.Context.SessionId = TEXT("old-session");
    FGamePlatformTelemetryEvent OldCritical = OldNormal;
    OldCritical.EventId = FGuid::NewGuid();
    OldCritical.Priority = EGamePlatformTelemetryPriority::CriticalTelemetry;

    TestTrue(TEXT("旧会话Normal入队"), Buffer.EnqueueEvent(OldNormal, 256));
    TestTrue(TEXT("旧会话Critical入队"), Buffer.EnqueueEvent(OldCritical, 256));
    TestEqual(TEXT("隐私边界丢弃2条旧记录"), Buffer.DiscardQueuedRecords(), 2);

    const FGamePlatformTelemetryDiagnostics AfterDiscard = Buffer.GetDiagnostics();
    TestEqual(TEXT("隐私边界后Buffer清空"), AfterDiscard.BufferDepth, 0);
    TestEqual(TEXT("Normal丢弃计数增加"), AfterDiscard.DroppedNormal, static_cast<int64>(1));
    TestEqual(TEXT("Critical丢弃计数增加"), AfterDiscard.DroppedCritical, static_cast<int64>(1));

    FGamePlatformTelemetryEvent NewSession;
    NewSession.EventId = FGuid::NewGuid();
    NewSession.EventName = TEXT("Telemetry.Foundation.ClientStarted");
    NewSession.Context.SessionId = TEXT("new-session");
    TestTrue(TEXT("新会话记录正常入队"), Buffer.EnqueueEvent(NewSession, 256));

    FGamePlatformTelemetryBatch Batch;
    TestTrue(TEXT("构建新会话批次"), Buffer.BuildBatch({}, Batch));
    TestEqual(TEXT("新批次只使用新会话Context"), Batch.SourceContext.SessionId, FString(TEXT("new-session")));
    TestEqual(TEXT("旧会话记录不得残留"), Batch.Events.Num(), 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryReconnectRecoveryTest,
    "GamePlatform.Telemetry.Network.ReconnectRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryReconnectRecoveryTest::RunTest(const FString&)
{
    const TSharedRef<FGamePlatformTelemetryRetryTestTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FGamePlatformTelemetryRetryTestTransport, ESPMode::ThreadSafe>(false);
    FGamePlatformTelemetryRetrySettings Retry;
    Retry.RetryMinSeconds = 0.1f;
    Retry.RetryMaxSeconds = 0.1f;
    Retry.MaxRetryAgeSeconds = 5.0f;
    Retry.MaxRetries = 2;
    Retry.MaxPendingBatches = 2;

    const TSharedRef<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe> Sink =
        MakeShared<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe>(Transport, Retry);
    TestTrue(TEXT("NetworkSink启动"), Sink->Start());

    FGamePlatformTelemetryBatch Batch;
    Batch.BatchId = FGuid::NewGuid();
    bool bCompletionAccepted = false;
    Sink->SubmitBatch(Batch, [&bCompletionAccepted](bool bAccepted, bool)
    {
        bCompletionAccepted = bAccepted;
    });

    TestEqual(TEXT("首次发送模拟断线"), Transport->Attempts, 1);
    TestEqual(TEXT("断线期间保持一个有界Pending Batch"), Sink->GetHealth().PendingBatches, 1);

    FTSTicker::GetCoreTicker().Tick(0.2f);

    TestEqual(TEXT("退避后执行第二次提交"), Transport->Attempts, 2);
    TestTrue(TEXT("网络恢复后原Batch成功完成"), bCompletionAccepted);
    TestEqual(TEXT("恢复后Pending归零"), Sink->GetHealth().PendingBatches, 0);
    TestEqual(TEXT("成功批次数计数"), Sink->GetHealth().SubmittedBatches, static_cast<int64>(1));
    Sink->Shutdown(0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryShutdownCancelsRetryTest,
    "GamePlatform.Telemetry.Network.ShutdownCancelsRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryShutdownCancelsRetryTest::RunTest(const FString&)
{
    const TSharedRef<FGamePlatformTelemetryRetryTestTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FGamePlatformTelemetryRetryTestTransport, ESPMode::ThreadSafe>(true);
    FGamePlatformTelemetryRetrySettings Retry;
    Retry.RetryMinSeconds = 0.1f;
    Retry.RetryMaxSeconds = 0.1f;
    Retry.MaxRetryAgeSeconds = 5.0f;
    Retry.MaxRetries = 3;

    const TSharedRef<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe> Sink =
        MakeShared<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe>(Transport, Retry);
    TestTrue(TEXT("持续断线测试Sink启动"), Sink->Start());

    FGamePlatformTelemetryBatch Batch;
    Batch.BatchId = FGuid::NewGuid();
    Sink->SubmitBatch(Batch, {});
    TestEqual(TEXT("已安排断线重试"), Sink->GetHealth().PendingBatches, 1);

    Sink->Shutdown(0.0f);
    const int32 AttemptsAtShutdown = Transport->Attempts;
    FTSTicker::GetCoreTicker().Tick(0.2f);

    TestEqual(TEXT("关停后重试Ticker不得再次发请求"), Transport->Attempts, AttemptsAtShutdown);
    TestEqual(TEXT("关停清空Pending"), Sink->GetHealth().PendingBatches, 0);
    TestTrue(TEXT("关停取消传输层请求"), Transport->CancelCalls >= 1);
    TestEqual(TEXT("未发送批次计入Drop"), Sink->GetHealth().DroppedBatches, static_cast<int64>(1));
    return true;
}

#endif
