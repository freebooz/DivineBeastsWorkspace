#if WITH_DEV_AUTOMATION_TESTS

#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"
#include "Misc/AutomationTest.h"
#include "Privacy/GamePlatformTelemetryPrivacyFilter.h"
#include "Sampling/GamePlatformTelemetrySampling.h"
#include "Schema/GamePlatformTelemetrySchemaRegistry.h"

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
    Limits.MaxBufferEvents = 2;
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
    TestTrue(TEXT("Normal入队"), Buffer.EnqueueEvent(Normal, 256));
    TestFalse(
        TEXT("Verbose不能驱逐更高优先级"),
        Buffer.EnqueueEvent(Verbose, 256));

    FGamePlatformTelemetryEvent NewCritical = Critical;
    NewCritical.EventId = FGuid::NewGuid();

    TestTrue(
        TEXT("Critical可驱逐较低优先级"),
        Buffer.EnqueueEvent(NewCritical, 256));

    const FGamePlatformTelemetryDiagnostics Diagnostics =
        Buffer.GetDiagnostics();

    TestEqual(
        TEXT("Buffer始终有界"),
        Diagnostics.BufferDepth,
        2);

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

#endif
