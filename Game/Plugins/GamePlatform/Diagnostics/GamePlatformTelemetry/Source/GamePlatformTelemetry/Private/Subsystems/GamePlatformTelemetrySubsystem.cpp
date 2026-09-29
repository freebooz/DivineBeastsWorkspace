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

namespace
{
FName SinkHealthName(EGamePlatformTelemetrySinkHealth Health)
{
    switch (Health)
    {
    case EGamePlatformTelemetrySinkHealth::Stopped: return TEXT("Stopped");
    case EGamePlatformTelemetrySinkHealth::Healthy: return TEXT("Healthy");
    case EGamePlatformTelemetrySinkHealth::Degraded: return TEXT("Degraded");
    case EGamePlatformTelemetrySinkHealth::Unavailable: return TEXT("Unavailable");
    default: return TEXT("Unknown");
    }
}
}


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

}

void UGamePlatformTelemetrySubsystem::Deinitialize()
{
    CancelScheduledFlush();

    // 关停前只做有界、非等待式Drain：尽量把Buffer填入Sink尚有的Pending容量；真正HTTP等待由Sink的Shutdown Budget处理。
    for (int32 Pass = 0; Pass < 8 && Buffer && Buffer->GetDiagnostics().BufferDepth > 0; ++Pass)
    {
        if (!FlushBestEffort())
        {
            break;
        }
    }

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
    check(IsInGameThread());
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
    check(IsInGameThread());
    if (bEnabled == bInEnabled)
    {
        return;
    }

    if (!bInEnabled)
    {
        FlushBestEffort();
        CancelScheduledFlush();
    }
    bEnabled = bInEnabled;
}

void UGamePlatformTelemetrySubsystem::SetSamplingSeed(
    FString InSamplingSeed)
{
    check(IsInGameThread());
    SamplingSeed = MoveTemp(InSamplingSeed);
}

void UGamePlatformTelemetrySubsystem::SetTraceBridgeEnabled(
    bool bInEnabled)
{
    check(IsInGameThread());
    bTraceBridgeEnabled = bInEnabled;
}

void UGamePlatformTelemetrySubsystem::SetContentRevision(
    FString InContentRevision)
{
    check(IsInGameThread());
    FScopeLock Lock(&ContextMutex);
    Context.ContentRevision = SanitizeContextValue(MoveTemp(InContentRevision));
}

void UGamePlatformTelemetrySubsystem::SetEnvironment(
    FString InEnvironment)
{
    check(IsInGameThread());
    FScopeLock Lock(&ContextMutex);
    Context.Environment = SanitizeContextValue(MoveTemp(InEnvironment));
}

void UGamePlatformTelemetrySubsystem::SetServerContext(
    FString InServerRole,
    FString InRegion,
    FString InServerInstanceId)
{
    check(IsInGameThread());
    FScopeLock Lock(&ContextMutex);
    Context.ServerRole = SanitizeContextValue(MoveTemp(InServerRole));
    Context.Region = SanitizeContextValue(MoveTemp(InRegion));
    Context.ServerInstanceId = SanitizeContextValue(MoveTemp(InServerInstanceId));
}

void UGamePlatformTelemetrySubsystem::BeginSession(
    FString InSessionId,
    FString InPseudonymousPlayerId)
{
    check(IsInGameThread());
    EndSession();

    FScopeLock Lock(&ContextMutex);
    ++SessionGeneration;
    Context.SessionId = SanitizeContextValue(MoveTemp(InSessionId));
    Context.PseudonymousPlayerId = SanitizeContextValue(MoveTemp(InPseudonymousPlayerId));
}

void UGamePlatformTelemetrySubsystem::EndSession()
{
    check(IsInGameThread());
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
    check(IsInGameThread());
    FScopeLock Lock(&ContextMutex);
    Context.MapId = SanitizeContextValue(MoveTemp(InMapId));
    Context.WorldId = SanitizeContextValue(MoveTemp(InWorldId));
    Context.ExperienceId = SanitizeContextValue(MoveTemp(InExperienceId));
    // Match/Arena字段暂保留二进制兼容；新项目不得继续向平台Context增加MOBA专有字段。
    Context.MatchId = SanitizeContextValue(MoveTemp(InMatchId));
    Context.ArenaModeId = SanitizeContextValue(MoveTemp(InArenaModeId));
}

void UGamePlatformTelemetrySubsystem::UpdateCorrelationContext(
    FString InCorrelationId,
    FString InTransactionId)
{
    check(IsInGameThread());
    FScopeLock Lock(&ContextMutex);
    Context.CorrelationId = SanitizeContextValue(MoveTemp(InCorrelationId));
    Context.TransactionId = SanitizeContextValue(MoveTemp(InTransactionId));
}

void UGamePlatformTelemetrySubsystem::BeforeWorldTravel()
{
    check(IsInGameThread());
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
    check(IsInGameThread());
    if (!bEnabled || !SchemaRegistry.IsValid() || !Buffer)
    {
        return EGamePlatformTelemetryRecordResult::Disabled;
    }

    // 首条运行时记录出现时冻结Schema；项目/领域组合根必须在真正开始记录前完成贡献注册。
    if (!SchemaRegistry->IsFrozen())
    {
        SchemaRegistry->Freeze();
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
    // Priority和Privacy均以冻结Schema为准，调用方不能自行提升优先级或降低隐私等级。
    Event.Priority = Definition->Priority;
    for (FGamePlatformTelemetryAttribute& Attribute : Event.Attributes)
    {
        if (const FGamePlatformTelemetryAttributeDefinition* AttributeDefinition =
                Definition->Attributes.Find(Attribute.Key))
        {
            Attribute.PrivacyClass = AttributeDefinition->PrivacyClass;
        }
    }

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

    RequestFlushAfterRecord();

    return EGamePlatformTelemetryRecordResult::Recorded;
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordMetric(
    FGamePlatformTelemetryMetric Metric)
{
    check(IsInGameThread());
    if (!bEnabled || !SchemaRegistry.IsValid() || !Buffer)
    {
        return EGamePlatformTelemetryRecordResult::Disabled;
    }

    if (!SchemaRegistry->IsFrozen())
    {
        SchemaRegistry->Freeze();
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

    if (!RateLimiter->TryConsume(
            Metric.Name,
            Definition->SustainedRatePerSecond,
            Definition->Burst,
            FPlatformTime::Seconds()))
    {
        Buffer->CountRateLimited();
        return EGamePlatformTelemetryRecordResult::RateLimited;
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

    RequestFlushAfterRecord();

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
    check(IsInGameThread());
    if (!Buffer || bFlushInProgress)
    {
        return false;
    }

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> LocalSink;
    {
        FScopeLock Lock(&SinkMutex);
        LocalSink = Sink;
    }
    if (!LocalSink.IsValid())
    {
        return false;
    }

    CancelScheduledFlush();
    TGuardValue<bool> FlushGuard(bFlushInProgress, true);
    bool bSubmittedAny = false;
    int32 SubmittedRecords = 0;

    const FGamePlatformTelemetrySinkStatus InitialSinkStatus = LocalSink->GetHealth();
    int32 MaxBatches = FMath::Clamp(Limits.MaxFlushBatchesPerPass, 1, 16);
    if (InitialSinkStatus.PendingCapacity > 0)
    {
        const int32 AvailableSlots = FMath::Max(
            0,
            InitialSinkStatus.PendingCapacity - InitialSinkStatus.PendingBatches);
        if (AvailableSlots <= 0)
        {
            // 断网/重试期间保留Buffer数据，不先BuildBatch再让Sink因容量不足丢弃。
            ScheduleFlush(Limits.FlushIntervalSeconds);
            return false;
        }
        MaxBatches = FMath::Min(MaxBatches, AvailableSlots);
    }

    TWeakObjectPtr<UGamePlatformTelemetrySubsystem> WeakThis(this);
    for (int32 BatchIndex = 0; BatchIndex < MaxBatches; ++BatchIndex)
    {
        FGamePlatformTelemetryBatch Batch;
        if (!Buffer->BuildBatch(GetContextSnapshot(), Batch))
        {
            break;
        }

        bSubmittedAny = true;
        SubmittedRecords += Batch.Events.Num() + Batch.Metrics.Num();
        LocalSink->SubmitBatch(
            MoveTemp(Batch),
            [WeakThis](bool, bool)
            {
                // NetworkSink终态完成意味着释放一个Pending槽位；下一游戏线程尽快继续Drain剩余Buffer。
                if (UGamePlatformTelemetrySubsystem* Self = WeakThis.Get())
                {
                    if (Self->Buffer && Self->Buffer->GetDiagnostics().BufferDepth > 0)
                    {
                        Self->ScheduleFlush(0.01f);
                    }
                }
            });
    }

    if (bSubmittedAny)
    {
        LastFlushUtc = FDateTime::UtcNow();
        LastFlushRecords = SubmittedRecords;
    }

    if (Buffer->GetDiagnostics().BufferDepth > 0)
    {
        // 这是兜底Deadline；异步Sink一旦完成会通过Completion更快唤醒下一轮。
        ScheduleFlush(Limits.FlushIntervalSeconds);
    }
    else
    {
        CancelScheduledFlush();
    }
    return bSubmittedAny;
}

FGamePlatformTelemetryDiagnostics
UGamePlatformTelemetrySubsystem::GetDiagnostics() const
{
    check(IsInGameThread());
    FGamePlatformTelemetryDiagnostics Diagnostics = Buffer
        ? Buffer->GetDiagnostics()
        : FGamePlatformTelemetryDiagnostics{};
    Diagnostics.bEnabled = bEnabled;
    Diagnostics.bFlushScheduled = FlushTickerHandle.IsValid();

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> LocalSink;
    {
        FScopeLock Lock(&SinkMutex);
        LocalSink = Sink;
    }
    if (LocalSink.IsValid())
    {
        const FGamePlatformTelemetrySinkStatus Status = LocalSink->GetHealth();
        Diagnostics.SinkHealth = SinkHealthName(Status.Health);
        Diagnostics.SinkLastError = Status.LastError;
        Diagnostics.PendingNetworkBatches = Status.PendingBatches;
        Diagnostics.SubmittedBatches = Status.SubmittedBatches;
        Diagnostics.FailedBatches = Status.FailedBatches;
        Diagnostics.DroppedBatches = Status.DroppedBatches;
        Diagnostics.SinkLastSuccessUtc = Status.LastSuccessUtc;
        Diagnostics.SinkLastFailureUtc = Status.LastFailureUtc;
    }
    Diagnostics.LastFlushUtc = LastFlushUtc;
    Diagnostics.LastFlushRecords = LastFlushRecords;
    return Diagnostics;
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
    // 一次性调度：触发前先清句柄，Flush若仍有数据会自行安排下一次Deadline。
    FlushTickerHandle.Reset();
    FlushBestEffort();
    return false;
}

void UGamePlatformTelemetrySubsystem::ScheduleFlush(float DelaySeconds)
{
    check(IsInGameThread());
    if (!bEnabled || !Buffer || FlushTickerHandle.IsValid() ||
        Buffer->GetDiagnostics().BufferDepth <= 0)
    {
        return;
    }

    FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UGamePlatformTelemetrySubsystem::TickFlush),
        FMath::Max(0.01f, DelaySeconds));
}

void UGamePlatformTelemetrySubsystem::CancelScheduledFlush()
{
    check(IsInGameThread());
    if (FlushTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
        FlushTickerHandle.Reset();
    }
}

void UGamePlatformTelemetrySubsystem::RequestFlushAfterRecord()
{
    check(IsInGameThread());
    if (!Buffer)
    {
        return;
    }

    const FGamePlatformTelemetryDiagnostics Diagnostics = Buffer->GetDiagnostics();
    if (Diagnostics.BufferDepth >= Limits.MaxBatchEvents ||
        Diagnostics.BufferBytes >= Limits.MaxBatchBytes)
    {
        FlushBestEffort();
    }
    else
    {
        ScheduleFlush(Limits.FlushIntervalSeconds);
    }
}

FString UGamePlatformTelemetrySubsystem::SanitizeContextValue(FString Value) const
{
    Value.TrimStartAndEndInline();
    Value.ReplaceInline(TEXT("\r"), TEXT(""));
    Value.ReplaceInline(TEXT("\n"), TEXT(""));
    if (Value.Len() > Limits.MaxContextStringLength)
    {
        Value.LeftInline(Limits.MaxContextStringLength, EAllowShrinking::No);
    }
    return Value;
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
