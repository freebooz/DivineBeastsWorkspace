#include "Subsystems/GamePlatformTelemetrySubsystem.h"

#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Privacy/GamePlatformTelemetryPrivacyFilter.h"
#include "Sampling/GamePlatformTelemetrySampling.h"
#include "Schema/GamePlatformTelemetrySchemaRegistry.h"
#include "Sinks/GamePlatformTelemetrySink.h"
#include "Trace/GamePlatformTelemetryTrace.h"

void UGamePlatformTelemetrySubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    SchemaRegistry =
        FGamePlatformTelemetrySchemaRegistry::
            CreateFoundationDefaults();

    Buffer =
        MakeUnique<FGamePlatformTelemetryBoundedBuffer>(
            Limits);

    RateLimiter =
        MakeUnique<FGamePlatformTelemetryRateLimiter>();

    {
        FScopeLock Lock(&ContextMutex);
        Context.BuildVersion = FApp::GetBuildVersion();
        Context.Platform = FPlatformProperties::IniPlatformName();

#if UE_BUILD_SHIPPING
        Context.Environment = TEXT("shipping");
#elif UE_BUILD_TEST
        Context.Environment = TEXT("test");
#elif UE_BUILD_DEVELOPMENT
        Context.Environment = TEXT("development");
#else
        Context.Environment = TEXT("unknown");
#endif

        Context.SourceRole =
            IsRunningDedicatedServer()
                ? TEXT("server")
                : TEXT("client");
    }

    Sink =
        MakeShared<
            FGamePlatformTelemetryNullSink,
            ESPMode::ThreadSafe>();
    Sink->Start();

    FlushTickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UGamePlatformTelemetrySubsystem::TickFlush),
            Limits.FlushIntervalSeconds);
}

void UGamePlatformTelemetrySubsystem::Deinitialize()
{
    if (FlushTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
        FlushTickerHandle.Reset();
    }

    FlushBestEffort();

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
        LocalSink;

    {
        FScopeLock Lock(&SinkMutex);
        LocalSink = Sink;
        Sink.Reset();
    }

    if (LocalSink.IsValid())
    {
        LocalSink->Flush();
        LocalSink->Shutdown(
            Limits.ShutdownFlushBudgetSeconds);
    }

    if (Buffer)
    {
        Buffer->Reset();
    }

    if (RateLimiter)
    {
        RateLimiter->Reset();
    }

    SchemaRegistry.Reset();

    Super::Deinitialize();
}

bool UGamePlatformTelemetrySubsystem::ConfigureSink(
    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
        InSink)
{
    if (!InSink.IsValid() || !InSink->Start())
    {
        return false;
    }

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
        OldSink;

    {
        FScopeLock Lock(&SinkMutex);
        OldSink = Sink;
        Sink = MoveTemp(InSink);
    }

    if (OldSink.IsValid())
    {
        OldSink->Flush();
        OldSink->Shutdown(
            Limits.ShutdownFlushBudgetSeconds);
    }

    return true;
}

void UGamePlatformTelemetrySubsystem::SetEnabled(
    bool bInEnabled)
{
    bEnabled = bInEnabled;
}

void UGamePlatformTelemetrySubsystem::SetSamplingSeed(
    FString InSamplingSeed)
{
    SamplingSeed = MoveTemp(InSamplingSeed);
}

void UGamePlatformTelemetrySubsystem::SetTraceBridgeEnabled(
    bool bInEnabled)
{
    bTraceBridgeEnabled = bInEnabled;
}

void UGamePlatformTelemetrySubsystem::SetContentRevision(
    FString InContentRevision)
{
    FScopeLock Lock(&ContextMutex);
    Context.ContentRevision = MoveTemp(InContentRevision);
}

void UGamePlatformTelemetrySubsystem::SetEnvironment(
    FString InEnvironment)
{
    FScopeLock Lock(&ContextMutex);
    Context.Environment = MoveTemp(InEnvironment);
}

void UGamePlatformTelemetrySubsystem::SetServerContext(
    FString InServerRole,
    FString InRegion,
    FString InServerInstanceId)
{
    FScopeLock Lock(&ContextMutex);
    Context.ServerRole = MoveTemp(InServerRole);
    Context.Region = MoveTemp(InRegion);
    Context.ServerInstanceId = MoveTemp(InServerInstanceId);
}

void UGamePlatformTelemetrySubsystem::BeginSession(
    FString InSessionId,
    FString InPseudonymousPlayerId)
{
    EndSession();

    FScopeLock Lock(&ContextMutex);
    ++SessionGeneration;
    Context.SessionId = MoveTemp(InSessionId);
    Context.PseudonymousPlayerId =
        MoveTemp(InPseudonymousPlayerId);
}

void UGamePlatformTelemetrySubsystem::EndSession()
{
    bool bHadSession = false;
    {
        FScopeLock Lock(&ContextMutex);
        bHadSession = !Context.SessionId.IsEmpty();
    }

    if (bHadSession)
    {
        FlushBestEffort();
    }

    {
        FScopeLock Lock(&ContextMutex);
        ++SessionGeneration;
        Context.SessionId.Reset();
        Context.PseudonymousPlayerId.Reset();
        Context.CorrelationId.Reset();
        Context.TransactionId.Reset();
        Context.MatchId.Reset();
        Context.ArenaModeId.Reset();
    }

    if (RateLimiter)
    {
        RateLimiter->Reset();
    }
}

void UGamePlatformTelemetrySubsystem::UpdateWorldContext(
    FString InMapId,
    FString InWorldId,
    FString InExperienceId,
    FString InMatchId,
    FString InArenaModeId)
{
    FScopeLock Lock(&ContextMutex);
    Context.MapId = MoveTemp(InMapId);
    Context.WorldId = MoveTemp(InWorldId);
    Context.ExperienceId = MoveTemp(InExperienceId);
    Context.MatchId = MoveTemp(InMatchId);
    Context.ArenaModeId = MoveTemp(InArenaModeId);
}

void UGamePlatformTelemetrySubsystem::UpdateCorrelationContext(
    FString InCorrelationId,
    FString InTransactionId)
{
    FScopeLock Lock(&ContextMutex);
    Context.CorrelationId = MoveTemp(InCorrelationId);
    Context.TransactionId = MoveTemp(InTransactionId);
}

void UGamePlatformTelemetrySubsystem::BeforeWorldTravel()
{
    if (bTraceBridgeEnabled)
    {
        const FGamePlatformTelemetryContext Snapshot =
            GetContextSnapshot();

        FGamePlatformTelemetryTrace::EmitBookmark(
            TEXT("World.Travel.Started"),
            Snapshot.CorrelationId);
    }

    FlushBestEffort();

    FScopeLock Lock(&ContextMutex);
    Context.MapId.Reset();
    Context.WorldId.Reset();
    Context.ExperienceId.Reset();
    Context.MatchId.Reset();
    Context.ArenaModeId.Reset();
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordEvent(
    FGamePlatformTelemetryEvent Event)
{
    if (!bEnabled || !SchemaRegistry.IsValid() || !Buffer)
    {
        return EGamePlatformTelemetryRecordResult::Disabled;
    }

    const FGamePlatformTelemetryEventDefinition* Definition =
        SchemaRegistry->FindEvent(Event.EventName);

    if (!Definition)
    {
        return EGamePlatformTelemetryRecordResult::InvalidEventName;
    }

    Event.EventId = FGuid::NewGuid();
    Event.TimestampUtc = FDateTime::UtcNow();
    Event.MonotonicTimestampSeconds =
        FPlatformTime::Seconds();
    Event.Sequence = Sequence.Increment();
    Event.Context = GetContextSnapshot();

    const EGamePlatformTelemetryRecordResult Validation =
        FGamePlatformTelemetryPrivacyFilter::ValidateEvent(
            Event,
            *Definition,
            Limits);

    if (Validation !=
        EGamePlatformTelemetryRecordResult::Recorded)
    {
        return Validation;
    }

    if (!FGamePlatformTelemetrySampler::ShouldSample(
            Definition->SamplingPolicy,
            Definition->SamplingRate,
            SamplingSeed,
            StableSamplingKey(Event.Context),
            Event.EventName,
            Event.EventId))
    {
        Buffer->CountSampledOut();
        return EGamePlatformTelemetryRecordResult::SampledOut;
    }

    if (!RateLimiter->TryConsume(
            Event.EventName,
            Definition->SustainedRatePerSecond,
            Definition->Burst,
            Event.MonotonicTimestampSeconds))
    {
        Buffer->CountRateLimited();
        return EGamePlatformTelemetryRecordResult::RateLimited;
    }

    const int32 EstimatedBytes =
        FGamePlatformTelemetryPrivacyFilter::
            EstimateEventBytes(Event);

    if (!Buffer->EnqueueEvent(Event, EstimatedBytes))
    {
        return EGamePlatformTelemetryRecordResult::BufferFull;
    }

    if (bTraceBridgeEnabled)
    {
        FGamePlatformTelemetryTrace::EmitEvent(
            Event.EventName,
            static_cast<uint64>(Event.Sequence),
            Event.Context.CorrelationId);
    }

    const FGamePlatformTelemetryDiagnostics Diagnostics =
        Buffer->GetDiagnostics();

    if (Diagnostics.BufferDepth >= Limits.MaxBatchEvents ||
        Diagnostics.BufferBytes >= Limits.MaxBatchBytes)
    {
        FlushBestEffort();
    }

    return EGamePlatformTelemetryRecordResult::Recorded;
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordMetric(
    FGamePlatformTelemetryMetric Metric)
{
    if (!bEnabled || !SchemaRegistry.IsValid() || !Buffer)
    {
        return EGamePlatformTelemetryRecordResult::Disabled;
    }

    const FGamePlatformTelemetryMetricDefinition* Definition =
        SchemaRegistry->FindMetric(Metric.Name);

    if (!Definition)
    {
        return EGamePlatformTelemetryRecordResult::
            MetricDefinitionNotFound;
    }

    Metric.Type = Definition->Type;
    Metric.Unit = Definition->Unit;
    Metric.TimestampUtc = FDateTime::UtcNow();
    Metric.Context = GetContextSnapshot();

    const EGamePlatformTelemetryRecordResult Validation =
        FGamePlatformTelemetryPrivacyFilter::ValidateMetric(
            Metric,
            *Definition);

    if (Validation !=
        EGamePlatformTelemetryRecordResult::Recorded)
    {
        return Validation;
    }

    const FGuid SamplingId = FGuid::NewGuid();

    if (!FGamePlatformTelemetrySampler::ShouldSample(
            Definition->SamplingPolicy,
            Definition->SamplingRate,
            SamplingSeed,
            StableSamplingKey(Metric.Context),
            Metric.Name,
            SamplingId))
    {
        Buffer->CountSampledOut();
        return EGamePlatformTelemetryRecordResult::SampledOut;
    }

    const int32 EstimatedBytes =
        FGamePlatformTelemetryPrivacyFilter::
            EstimateMetricBytes(Metric);

    if (!Buffer->EnqueueMetric(
            MoveTemp(Metric),
            EstimatedBytes))
    {
        return EGamePlatformTelemetryRecordResult::BufferFull;
    }

    return EGamePlatformTelemetryRecordResult::Recorded;
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::IncrementCounter(
    FName MetricName,
    double Delta,
    const TMap<FName, FString>& Labels)
{
    return RecordMetricInternal(
        MetricName,
        EGamePlatformTelemetryMetricType::Counter,
        Delta,
        Labels);
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordGauge(
    FName MetricName,
    double Value,
    const TMap<FName, FString>& Labels)
{
    return RecordMetricInternal(
        MetricName,
        EGamePlatformTelemetryMetricType::Gauge,
        Value,
        Labels);
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordHistogram(
    FName MetricName,
    double Value,
    const TMap<FName, FString>& Labels)
{
    return RecordMetricInternal(
        MetricName,
        EGamePlatformTelemetryMetricType::Histogram,
        Value,
        Labels);
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordDuration(
    FName MetricName,
    double Milliseconds,
    const TMap<FName, FString>& Labels)
{
    return RecordMetricInternal(
        MetricName,
        EGamePlatformTelemetryMetricType::Duration,
        Milliseconds,
        Labels);
}

bool UGamePlatformTelemetrySubsystem::FlushBestEffort()
{
    if (!Buffer)
    {
        return false;
    }

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
        LocalSink;

    {
        FScopeLock Lock(&SinkMutex);
        LocalSink = Sink;
    }

    if (!LocalSink.IsValid())
    {
        return false;
    }

    FGamePlatformTelemetryBatch Batch;
    if (!Buffer->BuildBatch(
            GetContextSnapshot(),
            Batch))
    {
        return false;
    }

    LocalSink->SubmitBatch(
        MoveTemp(Batch),
        [](bool, bool)
        {
            // Telemetry结果只进入Sink health/diagnostics；
            // 不能改变Gameplay或业务状态。
        });

    return true;
}

FGamePlatformTelemetryDiagnostics
UGamePlatformTelemetrySubsystem::GetDiagnostics() const
{
    return Buffer
        ? Buffer->GetDiagnostics()
        : FGamePlatformTelemetryDiagnostics{};
}

FGamePlatformTelemetryContext
UGamePlatformTelemetrySubsystem::GetContextSnapshot() const
{
    FScopeLock Lock(&ContextMutex);
    return Context;
}

TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
UGamePlatformTelemetrySubsystem::GetSchemaRegistry() const
{
    check(SchemaRegistry.IsValid());
    return SchemaRegistry.ToSharedRef();
}

bool UGamePlatformTelemetrySubsystem::TickFlush(float)
{
    FlushBestEffort();
    return true;
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordMetricInternal(
    FName MetricName,
    EGamePlatformTelemetryMetricType Type,
    double Value,
    const TMap<FName, FString>& Labels)
{
    FGamePlatformTelemetryMetric Metric;
    Metric.Name = MetricName;
    Metric.Type = Type;
    Metric.Value = Value;
    Metric.Labels = Labels;
    return RecordMetric(MoveTemp(Metric));
}

FString UGamePlatformTelemetrySubsystem::StableSamplingKey(
    const FGamePlatformTelemetryContext& ContextSnapshot) const
{
    if (!ContextSnapshot.SessionId.IsEmpty())
    {
        return ContextSnapshot.SessionId;
    }

    uint64 Generation = 0;
    {
        FScopeLock Lock(&ContextMutex);
        Generation = SessionGeneration;
    }

    return ContextSnapshot.SourceRole +
           TEXT("|") +
           ContextSnapshot.WorldId +
           TEXT("|") +
           LexToString(Generation);
}
