#include "Sinks/GamePlatformTelemetrySink.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogGamePlatformTelemetrySink,
    Log,
    All);

bool FGamePlatformTelemetryNullSink::Start()
{
    FScopeLock Lock(&Mutex);
    Status.Health = EGamePlatformTelemetrySinkHealth::Healthy;
    Status.LastError.Reset();
    return true;
}

void FGamePlatformTelemetryNullSink::SubmitBatch(
    FGamePlatformTelemetryBatch,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    {
        FScopeLock Lock(&Mutex);
        ++Status.SubmittedBatches;
        Status.LastSuccessUtc = FDateTime::UtcNow();
    }

    if (Completion)
    {
        Completion(true, false);
    }
}

void FGamePlatformTelemetryNullSink::Shutdown(float)
{
    FScopeLock Lock(&Mutex);
    Status.Health = EGamePlatformTelemetrySinkHealth::Stopped;
}

FGamePlatformTelemetrySinkStatus
FGamePlatformTelemetryNullSink::GetHealth() const
{
    FScopeLock Lock(&Mutex);
    return Status;
}

bool FGamePlatformTelemetryLogSink::Start()
{
    FScopeLock Lock(&Mutex);
    Status.Health = EGamePlatformTelemetrySinkHealth::Healthy;
    Status.LastError.Reset();
    return true;
}

void FGamePlatformTelemetryLogSink::SubmitBatch(
    FGamePlatformTelemetryBatch Batch,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    UE_LOG(
        LogGamePlatformTelemetrySink,
        Log,
        TEXT("Telemetry batch %s events=%d metrics=%d dropped=%lld"),
        *Batch.BatchId.ToString(EGuidFormats::DigitsWithHyphensLower),
        Batch.Events.Num(),
        Batch.Metrics.Num(),
        Batch.DroppedSinceLastBatch);

    {
        FScopeLock Lock(&Mutex);
        ++Status.SubmittedBatches;
        Status.LastSuccessUtc = FDateTime::UtcNow();
    }

    if (Completion)
    {
        Completion(true, false);
    }
}

void FGamePlatformTelemetryLogSink::Shutdown(float)
{
    FScopeLock Lock(&Mutex);
    Status.Health = EGamePlatformTelemetrySinkHealth::Stopped;
}

FGamePlatformTelemetrySinkStatus
FGamePlatformTelemetryLogSink::GetHealth() const
{
    FScopeLock Lock(&Mutex);
    return Status;
}
