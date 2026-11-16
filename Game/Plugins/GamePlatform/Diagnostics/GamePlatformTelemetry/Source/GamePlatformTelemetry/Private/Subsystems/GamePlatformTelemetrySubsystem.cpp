// 平台GI遥测执行：游戏线程拥有缓冲/采样/Sink与一次性调度；外部Sink可同步重入，关闭永久拒绝新请求。
// 输出器与上下文边界独立：异步提交核原实例/Sink，账号/世界发布核真实后继上下文操作，单纯换Sink不取消边界清理。
#include "Subsystems/GamePlatformTelemetrySubsystem.h"

#include "Buffer/TelemetryBoundedBuffer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "Templates/Atomic.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Privacy/GamePlatformTelemetryPrivacyFilter.h"
#include "Sampling/GamePlatformTelemetrySampling.h"
#include "Schema/TelemetrySchemaRegistry.h"
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


// 显式外置生命周期让内部UniquePtr策略保持Private，UHT构造/热重载入口仍使用原稳定反射身份。
UGamePlatformTelemetrySubsystem::UGamePlatformTelemetrySubsystem() = default;
UGamePlatformTelemetrySubsystem::UGamePlatformTelemetrySubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformTelemetrySubsystem::~UGamePlatformTelemetrySubsystem() = default;

void UGamePlatformTelemetrySubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    check(IsInGameThread());
    if (bClosing || bInitialized) { return; }
    bInitialized = true; ++LifecycleGeneration;
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

    ConfigureSink(MakeShared<FGamePlatformTelemetryNullSink, ESPMode::ThreadSafe>());

}

void UGamePlatformTelemetrySubsystem::Deinitialize()
{
    check(IsInGameThread());
    if (bClosing) { return; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    // 门闩先于任何外部Sink调用；Shutdown/最终Drain重入只能观察关闭，不能恢复配置或记录。
    bClosing = true; bInitialized = false; bEnabled = false; ++LifecycleGeneration;
    CancelScheduledFlush();
    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> LocalSink;
    {
        FScopeLock Lock(&SinkMutex); LocalSink = MoveTemp(Sink); Sink.Reset(); ++SinkGeneration;
    }
    const uint64 ClosingLifecycleGeneration = LifecycleGeneration;
    const uint64 RetiredSinkGeneration = SinkGeneration;
    // 只在没有旧Flush栈时尽力Drain；旧GetHealth/Submit正在关闭时不递归出队，剩余数据随关闭丢弃。
    for (int32 Pass = 0; Pass < 8 && LocalSink.IsValid() && Buffer && Buffer->GetDiagnostics().BufferDepth > 0; ++Pass)
    { if (!FlushToSink(LocalSink, ClosingLifecycleGeneration, RetiredSinkGeneration, true)) { break; } }
    if (LocalSink.IsValid())
    {
        LocalSink->Flush();
        const bool bStillClosingAfterFlush = IsSinkScopeCurrent(ClosingLifecycleGeneration, RetiredSinkGeneration, LocalSink, true);
        // Flush同步再次关闭是幂等的；摘下的Sink仍由此栈负责Shutdown一次，不能遗漏清理。
        LocalSink->Shutdown(Limits.ShutdownFlushBudgetSeconds);
        if (!bStillClosingAfterFlush || !IsSinkScopeCurrent(ClosingLifecycleGeneration, RetiredSinkGeneration, LocalSink, true)) { return; }
    }
    if(DevelopmentSavedSink.IsValid()){auto Saved=MoveTemp(DevelopmentSavedSink);Saved->Shutdown(Limits.ShutdownFlushBudgetSeconds);}
    DevelopmentCaptureScope.Invalidate();DevelopmentSavedBuffer.Reset();DevelopmentSavedRateLimiter.Reset();DevelopmentSavedSchema.Reset();
    Buffer.Reset(); RateLimiter.Reset(); SchemaRegistry.Reset();
    Super::Deinitialize();
}


bool UGamePlatformTelemetrySubsystem::ConfigureSink(TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> InSink)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return false;
    check(IsInGameThread());
    if (bClosing || !bInitialized || bConfiguringSink || !InSink.IsValid()) { return false; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    TGuardValue<bool> ConfigureGuard(bConfiguringSink, true);
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 PreviousSinkGeneration = SinkGeneration;
    bool bAlreadyInstalled = false;
    {
        FScopeLock Lock(&SinkMutex); bAlreadyInstalled = Sink == InSink;
    }
    if (bAlreadyInstalled)
    {
        // 重复配置不Start后再Shutdown自身；也不能把已经Stopped的同一对象报告为成功。
        const auto Status = InSink->GetHealth();
        return IsSinkScopeCurrent(ExpectedLifecycleGeneration, PreviousSinkGeneration, InSink) && Status.Health != EGamePlatformTelemetrySinkHealth::Stopped;
    }
    const bool bStarted = InSink->Start();
    if (!bStarted || bClosing || LifecycleGeneration != ExpectedLifecycleGeneration || SinkGeneration != PreviousSinkGeneration)
    {
        // Start可能已分配局部资源再关闭实例或报告失败；候选未发布，必须自行结束，旧Sink不由此失败栈替换。
        InSink->Shutdown(Limits.ShutdownFlushBudgetSeconds); return false;
    }
    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> OldSink;
    {
        FScopeLock Lock(&SinkMutex); OldSink = MoveTemp(Sink); Sink = InSink; ++SinkGeneration;
    }
    const uint64 PublishedSinkGeneration = SinkGeneration;
    CancelScheduledFlush();
    bool bCurrentAfterOldFlush = true;
    if (OldSink.IsValid())
    {
        OldSink->Flush();
        bCurrentAfterOldFlush = IsSinkScopeCurrent(ExpectedLifecycleGeneration, PublishedSinkGeneration, InSink);
        // 即使旧Flush关闭了新实例，退休Sink清理仍必须完成；接管期间的嵌套Configure明确拒绝。
        OldSink->Shutdown(Limits.ShutdownFlushBudgetSeconds);
    }
    if (!bCurrentAfterOldFlush || !IsSinkScopeCurrent(ExpectedLifecycleGeneration, PublishedSinkGeneration, InSink)) { return false; }
    if (Buffer && Buffer->GetDiagnostics().BufferDepth > 0) { ScheduleFlush(Limits.FlushIntervalSeconds); }
    return true;
}


void UGamePlatformTelemetrySubsystem::SetEnabled(bool bInEnabled)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized || bEnabled == bInEnabled) { return; }
    bEnabled = bInEnabled;
    if (!bInEnabled)
    {
        // 先拒绝新记录/取消调度，再调用可同步重入的Sink；返回栈不再覆写后继开关或关闭状态。
        CancelScheduledFlush(); FlushBestEffort();
    }
    else if (Buffer && Buffer->GetDiagnostics().BufferDepth > 0) { ScheduleFlush(Limits.FlushIntervalSeconds); }
}


void UGamePlatformTelemetrySubsystem::SetSamplingSeed(
    FString InSamplingSeed)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    SamplingSeed = MoveTemp(InSamplingSeed);
}

void UGamePlatformTelemetrySubsystem::SetTraceBridgeEnabled(
    bool bInEnabled)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    bTraceBridgeEnabled = bInEnabled;
}

void UGamePlatformTelemetrySubsystem::SetContentRevision(
    FString InContentRevision)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    FScopeLock Lock(&ContextMutex);
    Context.ContentRevision = SanitizeContextValue(MoveTemp(InContentRevision));
}

void UGamePlatformTelemetrySubsystem::SetEnvironment(
    FString InEnvironment)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    FScopeLock Lock(&ContextMutex);
    Context.Environment = SanitizeContextValue(MoveTemp(InEnvironment));
}

void UGamePlatformTelemetrySubsystem::SetServerContext(
    FString InServerRole,
    FString InRegion,
    FString InServerInstanceId)
{
    if(DevelopmentCaptureScope.IsValid()||bBeginningDevelopmentCapture)return;
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    FScopeLock Lock(&ContextMutex);
    Context.ServerRole = SanitizeContextValue(MoveTemp(InServerRole));
    Context.Region = SanitizeContextValue(MoveTemp(InRegion));
    Context.ServerInstanceId = SanitizeContextValue(MoveTemp(InServerInstanceId));
}

void UGamePlatformTelemetrySubsystem::BeginSessionOwned(const FGuid& Scope,FString InSessionId,FString InPseudonymousPlayerId)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return;}
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 ExpectedContextOperationGeneration = ++ContextOperationGeneration;
    // 清旧与发布新属于同一账号边界；Sink替换只结束旧输出栈，真实后继上下文才接管本次发布权。
    if (!EndSessionInternal(ExpectedLifecycleGeneration, ExpectedContextOperationGeneration)) { return; }

    FScopeLock Lock(&ContextMutex);
    ++SessionGeneration;
    Context.SessionId = SanitizeContextValue(MoveTemp(InSessionId));
    Context.PseudonymousPlayerId = SanitizeContextValue(MoveTemp(InPseudonymousPlayerId));
}

void UGamePlatformTelemetrySubsystem::EndSessionOwned(const FGuid& Scope)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return;}
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 ExpectedContextOperationGeneration = ++ContextOperationGeneration;
    EndSessionInternal(ExpectedLifecycleGeneration, ExpectedContextOperationGeneration);
}

bool UGamePlatformTelemetrySubsystem::IsContextOperationCurrent(
    uint64 ExpectedLifecycleGeneration, uint64 ExpectedContextOperationGeneration) const
{
    return !bClosing && bInitialized && LifecycleGeneration == ExpectedLifecycleGeneration
        && ContextOperationGeneration == ExpectedContextOperationGeneration;
}

bool UGamePlatformTelemetrySubsystem::EndSessionInternal(
    uint64 ExpectedLifecycleGeneration, uint64 ExpectedContextOperationGeneration)
{
    if (!IsContextOperationCurrent(ExpectedLifecycleGeneration, ExpectedContextOperationGeneration)) { return false; }
    bool bHadSession = false;
    {
        FScopeLock Lock(&ContextMutex);
        bHadSession = !Context.SessionId.IsEmpty();
    }

    if (bHadSession)
    {
        FlushBestEffort();
    }

    // GetHealth/Submit可换Sink或发起新账号/世界命令；只后者接管上下文，避免留下旧玩家或覆盖新玩家。
    if (!IsContextOperationCurrent(ExpectedLifecycleGeneration, ExpectedContextOperationGeneration)) { return false; }
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
    return true;
}

void UGamePlatformTelemetrySubsystem::UpdateWorldContextOwned(const FGuid& Scope,FString InMapId,FString InWorldId,FString InExperienceId,FString InMatchId,FString InArenaModeId)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return;}
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    ++ContextOperationGeneration;
    FScopeLock Lock(&ContextMutex);
    Context.MapId = SanitizeContextValue(MoveTemp(InMapId));
    Context.WorldId = SanitizeContextValue(MoveTemp(InWorldId));
    Context.ExperienceId = SanitizeContextValue(MoveTemp(InExperienceId));
    // Match/Arena字段暂保留二进制兼容；新项目不得继续向平台Context增加MOBA专有字段。
    Context.MatchId = SanitizeContextValue(MoveTemp(InMatchId));
    Context.ArenaModeId = SanitizeContextValue(MoveTemp(InArenaModeId));
}

void UGamePlatformTelemetrySubsystem::UpdateCorrelationContextOwned(const FGuid& Scope,FString InCorrelationId,FString InTransactionId)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return;}
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    FScopeLock Lock(&ContextMutex);
    Context.CorrelationId = SanitizeContextValue(MoveTemp(InCorrelationId));
    Context.TransactionId = SanitizeContextValue(MoveTemp(InTransactionId));
}

void UGamePlatformTelemetrySubsystem::BeforeWorldTravelOwned(const FGuid& Scope)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return;}
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 ExpectedContextOperationGeneration = ++ContextOperationGeneration;
    if (bTraceBridgeEnabled)
    {
        const FGamePlatformTelemetryContext Snapshot =
            GetContextSnapshot();

        FGamePlatformTelemetryTrace::EmitBookmark(
            TEXT("World.Travel.Started"),
            Snapshot.CorrelationId);
    }

    FlushBestEffort();
    // 换输出器必须仍完成旧世界清理；新世界发布或真实后继边界已接管时才停止旧清理栈。
    if (!IsContextOperationCurrent(ExpectedLifecycleGeneration, ExpectedContextOperationGeneration)) { return; }

    FScopeLock Lock(&ContextMutex);
    Context.MapId.Reset();
    Context.WorldId.Reset();
    Context.ExperienceId.Reset();
    Context.MatchId.Reset();
    Context.ArenaModeId.Reset();
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordEventOwned(const FGuid& Scope,FGamePlatformTelemetryEvent Event)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return EGamePlatformTelemetryRecordResult::Disabled;}
    check(IsInGameThread());
    if (bClosing || !bInitialized || !bEnabled || !SchemaRegistry.IsValid() || !Buffer)
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
    const FGamePlatformTelemetryContext ContextSnapshot = GetContextSnapshot();
    // 调用方传入的Context一律忽略；Buffer单独共享平台当前快照，避免每条Event复制十余个FString。
    Event.Context = {};
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
            StableSamplingKey(ContextSnapshot),
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

    const FName RecordedEventName = Event.EventName;
    const int64 RecordedSequence = Event.Sequence;

    if (!Buffer->EnqueueEvent(MoveTemp(Event), EstimatedBytes, ContextSnapshot))
    {
        return EGamePlatformTelemetryRecordResult::BufferFull;
    }

    if (bTraceBridgeEnabled)
    {
        FGamePlatformTelemetryTrace::EmitEvent(
            RecordedEventName,
            static_cast<uint64>(RecordedSequence),
            ContextSnapshot.CorrelationId);
    }

    RequestFlushAfterRecord();

    return EGamePlatformTelemetryRecordResult::Recorded;
}

EGamePlatformTelemetryRecordResult
UGamePlatformTelemetrySubsystem::RecordMetricOwned(const FGuid& Scope,FGamePlatformTelemetryMetric Metric)
{
    if(Scope!=DevelopmentCaptureScope||bEndingDevelopmentCapture||bBeginningDevelopmentCapture){return EGamePlatformTelemetryRecordResult::Disabled;}
    check(IsInGameThread());
    if (bClosing || !bInitialized || !bEnabled || !SchemaRegistry.IsValid() || !Buffer)
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
    const FGamePlatformTelemetryContext ContextSnapshot = GetContextSnapshot();
    // Metric上下文同样由Buffer共享快照持有，调用方不能伪造或覆盖平台Context。
    Metric.Context = {};

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
            StableSamplingKey(ContextSnapshot),
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
            EstimatedBytes,
            ContextSnapshot))
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
    if (bClosing || !bInitialized || !Buffer || bFlushInProgress) { return false; }
    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> LocalSink;
    { FScopeLock Lock(&SinkMutex); LocalSink = Sink; }
    return FlushToSink(LocalSink, LifecycleGeneration, SinkGeneration, false);
}

bool UGamePlatformTelemetrySubsystem::IsSinkScopeCurrent(uint64 ExpectedLifecycleGeneration, uint64 ExpectedSinkGeneration,
    const TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>& ExpectedSink, bool bFinalDrain) const
{
    if (!ExpectedSink.IsValid() || LifecycleGeneration != ExpectedLifecycleGeneration || SinkGeneration != ExpectedSinkGeneration) { return false; }
    FScopeLock Lock(&SinkMutex);
    return bFinalDrain ? bClosing && !Sink.IsValid() : !bClosing && bInitialized && Sink == ExpectedSink;
}

bool UGamePlatformTelemetrySubsystem::FlushToSink(const TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>& LocalSink,
    uint64 ExpectedLifecycleGeneration, uint64 ExpectedSinkGeneration, bool bFinalDrain)
{
    check(IsInGameThread());
    if (!Buffer || bFlushInProgress || !IsSinkScopeCurrent(ExpectedLifecycleGeneration, ExpectedSinkGeneration, LocalSink, bFinalDrain)) { return false; }
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(this);
    CancelScheduledFlush(); TGuardValue<bool> FlushGuard(bFlushInProgress, true);
    const auto IsCurrent = [&]() { return Buffer && IsSinkScopeCurrent(ExpectedLifecycleGeneration, ExpectedSinkGeneration, LocalSink, bFinalDrain); };
    // GetHealth可同步替换/关闭Sink，必须在出队前复核实际身份，不能向已关闭的旧Sink提交。
    const auto InitialSinkStatus = LocalSink->GetHealth();
    if (!IsCurrent()) { return false; }
    int32 MaxBatches = FMath::Clamp(Limits.MaxFlushBatchesPerPass, 1, 16);
    if (InitialSinkStatus.PendingCapacity > 0)
    {
        const int32 AvailableSlots = FMath::Max(0, InitialSinkStatus.PendingCapacity - InitialSinkStatus.PendingBatches);
        if (AvailableSlots <= 0)
        { if (!bFinalDrain) { ScheduleFlush(Limits.FlushIntervalSeconds); } return false; }
        MaxBatches = FMath::Min(MaxBatches, AvailableSlots);
    }
    bool bSubmittedAny = false; int32 SubmittedRecords = 0;
    const TWeakObjectPtr<UGamePlatformTelemetrySubsystem> WeakThis(this);
    for (int32 BatchIndex = 0; BatchIndex < MaxBatches; ++BatchIndex)
    {
        if (!IsCurrent()) { return bSubmittedAny; }
        FGamePlatformTelemetryBatch Batch;
        if (!Buffer->BuildBatch(GetContextSnapshot(), Batch)) { break; }
        bSubmittedAny = true; SubmittedRecords += Batch.Events.Num() + Batch.Metrics.Num();
        auto CompletionConsumed = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
        LocalSink->SubmitBatch(MoveTemp(Batch),
            [WeakThis, ExpectedLifecycleGeneration, ExpectedSinkGeneration, CompletionConsumed](bool, bool)
            {
                if (CompletionConsumed->Exchange(true)) { return; }
                auto Complete = [WeakThis, ExpectedLifecycleGeneration, ExpectedSinkGeneration]()
                {
                    if (auto* Self = WeakThis.Get())
                    {
                        // 终态只唤醒同一活跃Sink代次，关闭Drain/旧Sink/重复完成均不能创建Ticker。
                        if (!Self->bClosing && Self->bInitialized && Self->LifecycleGeneration == ExpectedLifecycleGeneration && Self->SinkGeneration == ExpectedSinkGeneration &&
                            Self->Buffer && Self->Buffer->GetDiagnostics().BufferDepth > 0) { Self->ScheduleFlush(0.01f); }
                    }
                };
                if (IsInGameThread()) { Complete(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Complete)); }
            });
        // Submit可同步关闭/换代；转交已发生，但禁止旧栈继续出队、写后继诊断或恢复调度。
        if (!IsCurrent()) { return bSubmittedAny; }
    }
    if (bSubmittedAny) { LastFlushUtc = FDateTime::UtcNow(); LastFlushRecords = SubmittedRecords; }
    if (!bFinalDrain)
    {
        if (Buffer->GetDiagnostics().BufferDepth > 0) { ScheduleFlush(Limits.FlushIntervalSeconds); }
        else { CancelScheduledFlush(); }
    }
    return bSubmittedAny;
}


int32 UGamePlatformTelemetrySubsystem::DiscardBufferedRecordsForPrivacyBoundary()
{
    check(IsInGameThread());
    if (bClosing || !bInitialized) { return 0; }
    CancelScheduledFlush();
    return Buffer ? Buffer->DiscardQueuedRecords() : 0;
}

FGamePlatformTelemetryDiagnostics UGamePlatformTelemetrySubsystem::GetDiagnostics() const
{
    check(IsInGameThread());
    TStrongObjectPtr<UGamePlatformTelemetrySubsystem> KeepSelf(const_cast<UGamePlatformTelemetrySubsystem*>(this));
    const auto ReadBase = [this]()
    {
        auto Result = Buffer ? Buffer->GetDiagnostics() : FGamePlatformTelemetryDiagnostics{};
        Result.bEnabled = !bClosing && bInitialized && bEnabled;
        Result.bFlushScheduled = !bClosing && FlushTickerHandle.IsValid();
        Result.LastFlushUtc = LastFlushUtc; Result.LastFlushRecords = LastFlushRecords; return Result;
    };
    auto Diagnostics = ReadBase();
    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> LocalSink;
    { FScopeLock Lock(&SinkMutex); LocalSink = Sink; }
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 ExpectedSinkGeneration = SinkGeneration;
    if (LocalSink.IsValid() && !bClosing)
    {
        const auto Status = LocalSink->GetHealth();
        // 自定义健康查询可换代；只返回最新基础诊断，不递归查询新Sink或附上过期A的健康信息。
        if (!IsSinkScopeCurrent(ExpectedLifecycleGeneration, ExpectedSinkGeneration, LocalSink)) { return ReadBase(); }
        Diagnostics.SinkHealth = SinkHealthName(Status.Health); Diagnostics.SinkLastError = Status.LastError;
        Diagnostics.PendingNetworkBatches = Status.PendingBatches; Diagnostics.SubmittedBatches = Status.SubmittedBatches;
        Diagnostics.FailedBatches = Status.FailedBatches; Diagnostics.DroppedBatches = Status.DroppedBatches;
        Diagnostics.SinkLastSuccessUtc = Status.LastSuccessUtc; Diagnostics.SinkLastFailureUtc = Status.LastFailureUtc;
    }
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
    check(IsInGameThread());
    // ScheduleFlush捕获实例/Sink/调度资格，过期Ticker不会进入此方法或清掉新句柄。
    FlushTickerHandle.Reset(); ++FlushScheduleGeneration;
    if (!bClosing && bInitialized) { FlushBestEffort(); }
    return false;
}


void UGamePlatformTelemetrySubsystem::ScheduleFlush(float DelaySeconds)
{
    check(IsInGameThread());
    if (bClosing || !bInitialized || !bEnabled || !Buffer || FlushTickerHandle.IsValid() || Buffer->GetDiagnostics().BufferDepth <= 0) { return; }
    const uint64 ExpectedLifecycleGeneration = LifecycleGeneration;
    const uint64 ExpectedSinkGeneration = SinkGeneration;
    const uint64 ExpectedScheduleGeneration = ++FlushScheduleGeneration;
    const TWeakObjectPtr<UGamePlatformTelemetrySubsystem> WeakThis(this);
    FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [WeakThis, ExpectedLifecycleGeneration, ExpectedSinkGeneration, ExpectedScheduleGeneration](float DeltaSeconds)
        {
            if (auto* Self = WeakThis.Get())
            {
                if (!Self->bClosing && Self->bInitialized && Self->LifecycleGeneration == ExpectedLifecycleGeneration && Self->SinkGeneration == ExpectedSinkGeneration && Self->FlushScheduleGeneration == ExpectedScheduleGeneration)
                { return Self->TickFlush(DeltaSeconds); }
            }
            return false;
        }), FMath::Max(0.01f, DelaySeconds));
}


void UGamePlatformTelemetrySubsystem::CancelScheduledFlush()
{
    check(IsInGameThread());
    ++FlushScheduleGeneration;
    if (FlushTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
        FlushTickerHandle.Reset();
    }
}

void UGamePlatformTelemetrySubsystem::RequestFlushAfterRecord()
{
    check(IsInGameThread());
    if (bClosing || !bInitialized || !bEnabled) { return; }
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

EGamePlatformTelemetryRecordResult UGamePlatformTelemetrySubsystem::RecordEvent(FGamePlatformTelemetryEvent Event){return RecordEventOwned({},MoveTemp(Event));}
EGamePlatformTelemetryRecordResult UGamePlatformTelemetrySubsystem::RecordMetric(FGamePlatformTelemetryMetric Metric){return RecordMetricOwned({},MoveTemp(Metric));}
void UGamePlatformTelemetrySubsystem::BeginSession(FString InSessionId,
    FString InPseudonymousPlayerId){BeginSessionOwned({},MoveTemp(InSessionId),MoveTemp(InPseudonymousPlayerId));}
void UGamePlatformTelemetrySubsystem::EndSession(){EndSessionOwned({});}
void UGamePlatformTelemetrySubsystem::UpdateWorldContext(FString InMapId,
    FString InWorldId,
    FString InExperienceId,
    FString InMatchId,
    FString InArenaModeId){UpdateWorldContextOwned({},MoveTemp(InMapId),MoveTemp(InWorldId),MoveTemp(InExperienceId),MoveTemp(InMatchId),MoveTemp(InArenaModeId));}
void UGamePlatformTelemetrySubsystem::UpdateCorrelationContext(FString InCorrelationId,
    FString InTransactionId){UpdateCorrelationContextOwned({},MoveTemp(InCorrelationId),MoveTemp(InTransactionId));}
void UGamePlatformTelemetrySubsystem::BeforeWorldTravel(){BeforeWorldTravelOwned({});}

namespace {
/** 同步有界计数sink只接纳平台执行器传来的合成批次；不存事件、身份、属性或网络队列。 */
class FGamePlatformDevelopmentCountSink final:public IGamePlatformTelemetrySink {
 FGamePlatformTelemetrySinkStatus Status;
public:
 bool Start()override{Status.Health=EGamePlatformTelemetrySinkHealth::Healthy;return true;}
 void SubmitBatch(FGamePlatformTelemetryBatch,FGamePlatformTelemetrySubmitCompletion Done)override{++Status.SubmittedBatches;Status.LastSuccessUtc=FDateTime::UtcNow();if(Done)Done(true,false);}
 void Flush()override{}
 void Shutdown(float)override{Status.Health=EGamePlatformTelemetrySinkHealth::Stopped;}
 FGamePlatformTelemetrySinkStatus GetHealth()const override{return Status;}
};
}
bool UGamePlatformTelemetrySubsystem::BeginDevelopmentCapture(UWorld& World,FGuid Owner,FGuid& OutScope){
 check(IsInGameThread());OutScope.Invalidate();
#if UE_BUILD_SHIPPING
 return false;
#else
 if(!Owner.IsValid()||World.GetGameInstance()!=GetGameInstance()||World.GetNetMode()!=NM_Standalone||World.bIsTearingDown||World.WorldType!=EWorldType::Game||
    !FParse::Param(FCommandLine::Get(),TEXT("DBAPluginDemo"))||!FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture"))||
    bClosing||!bInitialized||bConfiguringSink||bFlushInProgress||FlushTickerHandle.IsValid()||DevelopmentCaptureScope.IsValid()||!Buffer||Buffer->GetDiagnostics().BufferDepth!=0)return false;
 const auto C=GetContextSnapshot();if(!C.SessionId.IsEmpty()||!C.PseudonymousPlayerId.IsEmpty()||!C.MapId.IsEmpty()||!C.WorldId.IsEmpty()||!C.ExperienceId.IsEmpty()||!C.MatchId.IsEmpty()||!C.ArenaModeId.IsEmpty()||!C.CorrelationId.IsEmpty()||!C.TransactionId.IsEmpty()||!C.ServerInstanceId.IsEmpty())return false;
 TGuardValue<bool> Beginning(bBeginningDevelopmentCapture,true);TGuardValue<bool> Configuring(bConfiguringSink,true);const auto L=LifecycleGeneration;const auto G=SinkGeneration;const auto ContextGeneration=ContextOperationGeneration;auto Previous=Sink;
 const auto Health=Previous.IsValid()?Previous->GetHealth():FGamePlatformTelemetrySinkStatus{};
 if(bClosing||LifecycleGeneration!=L||SinkGeneration!=G||ContextOperationGeneration!=ContextGeneration||Sink!=Previous||Health.PendingBatches!=0||Buffer->GetDiagnostics().BufferDepth!=0||bFlushInProgress)return false;
 auto Candidate=MakeShared<FGamePlatformDevelopmentCountSink,ESPMode::ThreadSafe>();if(!Candidate->Start())return false;
 CancelScheduledFlush();DevelopmentSavedSink=MoveTemp(Sink);DevelopmentSavedBuffer=MoveTemp(Buffer);DevelopmentSavedRateLimiter=MoveTemp(RateLimiter);DevelopmentSavedSchema=MoveTemp(SchemaRegistry);
 DevelopmentSavedContext=C;DevelopmentSavedSamplingSeed=SamplingSeed;bDevelopmentSavedEnabled=bEnabled;bDevelopmentSavedTrace=bTraceBridgeEnabled;DevelopmentSavedLastFlushUtc=LastFlushUtc;DevelopmentSavedLastFlushRecords=LastFlushRecords;
 Sink=Candidate;++SinkGeneration;Buffer=MakeUnique<FGamePlatformTelemetryBoundedBuffer>(Limits);RateLimiter=MakeUnique<FGamePlatformTelemetryRateLimiter>();SchemaRegistry=FGamePlatformTelemetrySchemaRegistry::CreateFoundationDefaults();
 SamplingSeed=TEXT("manual.synthetic");bEnabled=true;bTraceBridgeEnabled=false;DevelopmentCaptureScope=FGuid::NewGuid();OutScope=DevelopmentCaptureScope;return true;
#endif
}
bool UGamePlatformTelemetrySubsystem::ExecuteDevelopmentCapture(const FGuid& Scope,FName Op,double Value,FString Identifier,EGamePlatformTelemetryRecordResult& Out){
 Out=EGamePlatformTelemetryRecordResult::Disabled;if(!Scope.IsValid()||Scope!=DevelopmentCaptureScope||bClosing||bEndingDevelopmentCapture||!FMath::IsFinite(Value)||Identifier.Len()>128)return false;
 for(TCHAR C:Identifier)if(!((C>='A'&&C<='Z')||(C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='.'||C=='_'||C=='-'))return false;
 const FString Id=TEXT("manual.")+Identifier;
 if(Op==TEXT("BeginSession"))BeginSessionOwned(Scope,Id,TEXT("manual.anonymous"));
 else if(Op==TEXT("EndSession"))EndSessionOwned(Scope);
 else if(Op==TEXT("PublishWorld")){UpdateWorldContextOwned(Scope,TEXT("manual.map"),Id,TEXT(""),TEXT(""),TEXT(""));UpdateCorrelationContextOwned(Scope,TEXT("manual.trace"),TEXT(""));}
 else if(Op==TEXT("PublishCorrelation"))UpdateCorrelationContextOwned(Scope,Id,TEXT(""));
 else if(Op==TEXT("BeforeTravel"))BeforeWorldTravelOwned(Scope);
 else if(Op==TEXT("Discard"))DiscardBufferedRecordsForPrivacyBoundary();
 else if(Op==TEXT("Flush"))FlushBestEffort();
 else if(Op==TEXT("RecordEvent")||Op==TEXT("RejectSchema")){
  FGamePlatformTelemetryEvent V;V.EventName=Op==TEXT("RecordEvent")?FName(TEXT("Telemetry.Foundation.WorldLoaded")):FName(TEXT("Manual.UnknownSchema"));V.SchemaVersion=1;
  FGamePlatformTelemetryAttribute A;A.Key=TEXT("result");A.StringValue=Id;V.Attributes.Add(A);Out=RecordEventOwned(Scope,MoveTemp(V));
 }else{
  FGamePlatformTelemetryMetric V;V.Value=Value;
  if(Op==TEXT("Counter")){V.Name=TEXT("client.telemetry.dropped_total");V.Type=EGamePlatformTelemetryMetricType::Counter;}
  else if(Op==TEXT("Gauge")){V.Name=TEXT("client.telemetry.buffer_depth");V.Type=EGamePlatformTelemetryMetricType::Gauge;}
  else if(Op==TEXT("Histogram")){V.Name=TEXT("client.frame_ms");V.Type=EGamePlatformTelemetryMetricType::Histogram;}
  else if(Op==TEXT("Duration")){V.Name=TEXT("client.loading.duration_ms");V.Type=EGamePlatformTelemetryMetricType::Duration;}
  else return false;Out=RecordMetricOwned(Scope,MoveTemp(V));
 }
 return !bClosing&&DevelopmentCaptureScope==Scope;
}
bool UGamePlatformTelemetrySubsystem::EndDevelopmentCapture(const FGuid& Scope){
 check(IsInGameThread());if(!Scope.IsValid()||Scope!=DevelopmentCaptureScope||bClosing||bConfiguringSink||bFlushInProgress||bEndingDevelopmentCapture)return false;
 TGuardValue<bool> Ending(bEndingDevelopmentCapture,true);CancelScheduledFlush();
 auto Retired=MoveTemp(Sink);++SinkGeneration;if(Retired.IsValid())Retired->Shutdown(Limits.ShutdownFlushBudgetSeconds);
 // The dedicated sink has no callbacks or pending IO. Restore exact suspended objects, including an actually empty pointer.
 Sink=MoveTemp(DevelopmentSavedSink);Buffer=MoveTemp(DevelopmentSavedBuffer);RateLimiter=MoveTemp(DevelopmentSavedRateLimiter);SchemaRegistry=MoveTemp(DevelopmentSavedSchema);
 {FScopeLock Lock(&ContextMutex);Context=DevelopmentSavedContext;}SamplingSeed=MoveTemp(DevelopmentSavedSamplingSeed);bEnabled=bDevelopmentSavedEnabled;bTraceBridgeEnabled=bDevelopmentSavedTrace;LastFlushUtc=DevelopmentSavedLastFlushUtc;LastFlushRecords=DevelopmentSavedLastFlushRecords;++ContextOperationGeneration;++SessionGeneration;
 DevelopmentCaptureScope.Invalidate();return true;
}
