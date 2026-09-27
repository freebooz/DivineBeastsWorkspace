#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"

FGamePlatformTelemetryBoundedBuffer::
FGamePlatformTelemetryBoundedBuffer(
    FGamePlatformTelemetryLimits InLimits)
    : Limits(MoveTemp(InLimits))
{
    Limits.MaxBufferEvents =
        FMath::Max(16, Limits.MaxBufferEvents);
    Limits.MaxBufferBytes =
        FMath::Max(1024, Limits.MaxBufferBytes);
    Limits.MaxBatchEvents =
        FMath::Clamp(
            Limits.MaxBatchEvents,
            1,
            Limits.MaxBufferEvents);
    Limits.MaxBatchBytes =
        FMath::Clamp(
            Limits.MaxBatchBytes,
            1024,
            Limits.MaxBufferBytes);
}

bool FGamePlatformTelemetryBoundedBuffer::EnqueueEvent(
    FGamePlatformTelemetryEvent Event,
    int32 EstimatedBytes)
{
    if (EstimatedBytes <= 0 ||
        EstimatedBytes > Limits.MaxEventBytes)
    {
        return false;
    }

    FScopeLock Lock(&Mutex);

    if (!EnsureCapacityFor(
            Event.Priority,
            EstimatedBytes))
    {
        CountDrop(Event.Priority);
        return false;
    }

    FQueuedRecord Record;
    Record.Kind = ERecordKind::Event;
    Record.Priority = Event.Priority;
    Record.EstimatedBytes = EstimatedBytes;
    Record.Event = MoveTemp(Event);

    CurrentBytes += EstimatedBytes;
    Records.Add(MoveTemp(Record));
    ++Diagnostics.RecordedTotal;
    Diagnostics.BufferDepth = Records.Num();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

bool FGamePlatformTelemetryBoundedBuffer::EnqueueMetric(
    FGamePlatformTelemetryMetric Metric,
    int32 EstimatedBytes)
{
    if (EstimatedBytes <= 0)
    {
        return false;
    }

    FScopeLock Lock(&Mutex);

    if (!EnsureCapacityFor(
            EGamePlatformTelemetryPriority::Normal,
            EstimatedBytes))
    {
        CountDrop(EGamePlatformTelemetryPriority::Normal);
        return false;
    }

    FQueuedRecord Record;
    Record.Kind = ERecordKind::Metric;
    Record.Priority = EGamePlatformTelemetryPriority::Normal;
    Record.EstimatedBytes = EstimatedBytes;
    Record.Metric = MoveTemp(Metric);

    CurrentBytes += EstimatedBytes;
    Records.Add(MoveTemp(Record));
    ++Diagnostics.RecordedTotal;
    Diagnostics.BufferDepth = Records.Num();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

bool FGamePlatformTelemetryBoundedBuffer::BuildBatch(
    const FGamePlatformTelemetryContext& SourceContext,
    FGamePlatformTelemetryBatch& OutBatch)
{
    FScopeLock Lock(&Mutex);

    if (Records.IsEmpty())
    {
        return false;
    }

    OutBatch = {};
    OutBatch.BatchId = FGuid::NewGuid();
    OutBatch.SchemaVersion = 1;
    OutBatch.SourceContext = SourceContext;
    OutBatch.CreatedAtUtc = FDateTime::UtcNow();
    OutBatch.DroppedSinceLastBatch = DroppedSinceLastBatch;

    int32 Count = 0;
    int32 Bytes = 0;
    int32 ConsumeCount = 0;

    for (const FQueuedRecord& Record : Records)
    {
        if (Count >= Limits.MaxBatchEvents ||
            Bytes + Record.EstimatedBytes >
                Limits.MaxBatchBytes)
        {
            break;
        }

        if (Record.Kind == ERecordKind::Event)
        {
            OutBatch.Events.Add(Record.Event);
        }
        else
        {
            OutBatch.Metrics.Add(Record.Metric);
        }

        ++Count;
        ++ConsumeCount;
        Bytes += Record.EstimatedBytes;
    }

    if (ConsumeCount <= 0)
    {
        return false;
    }

    for (int32 Index = 0; Index < ConsumeCount; ++Index)
    {
        CurrentBytes -= Records[Index].EstimatedBytes;
    }

    Records.RemoveAt(
        0,
        ConsumeCount,
        EAllowShrinking::No);

    DroppedSinceLastBatch = 0;
    Diagnostics.BufferDepth = Records.Num();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

FGamePlatformTelemetryDiagnostics
FGamePlatformTelemetryBoundedBuffer::GetDiagnostics() const
{
    FScopeLock Lock(&Mutex);
    FGamePlatformTelemetryDiagnostics Result = Diagnostics;
    Result.BufferDepth = Records.Num();
    Result.BufferBytes = CurrentBytes;
    return Result;
}

void FGamePlatformTelemetryBoundedBuffer::CountSampledOut()
{
    FScopeLock Lock(&Mutex);
    ++Diagnostics.SampledOutTotal;
}

void FGamePlatformTelemetryBoundedBuffer::CountRateLimited()
{
    FScopeLock Lock(&Mutex);
    ++Diagnostics.RateLimitedTotal;
}

void FGamePlatformTelemetryBoundedBuffer::Reset()
{
    FScopeLock Lock(&Mutex);
    Records.Reset();
    CurrentBytes = 0;
    DroppedSinceLastBatch = 0;
    Diagnostics = {};
}

bool FGamePlatformTelemetryBoundedBuffer::EnsureCapacityFor(
    EGamePlatformTelemetryPriority IncomingPriority,
    int32 IncomingBytes)
{
    if (IncomingBytes > Limits.MaxBufferBytes)
    {
        return false;
    }

    while (Records.Num() >= Limits.MaxBufferEvents ||
           CurrentBytes + IncomingBytes >
               Limits.MaxBufferBytes)
    {
        const int32 Candidate =
            FindDropCandidate(IncomingPriority);

        if (Candidate == INDEX_NONE)
        {
            return false;
        }

        CountDrop(Records[Candidate].Priority);
        CurrentBytes -= Records[Candidate].EstimatedBytes;
        Records.RemoveAt(
            Candidate,
            1,
            EAllowShrinking::No);
    }

    return true;
}

int32 FGamePlatformTelemetryBoundedBuffer::FindDropCandidate(
    EGamePlatformTelemetryPriority IncomingPriority) const
{
    for (const EGamePlatformTelemetryPriority Priority : {
             EGamePlatformTelemetryPriority::Verbose,
             EGamePlatformTelemetryPriority::Normal,
             EGamePlatformTelemetryPriority::CriticalTelemetry})
    {
        if (static_cast<uint8>(Priority) >
            static_cast<uint8>(IncomingPriority))
        {
            continue;
        }

        for (int32 Index = 0; Index < Records.Num(); ++Index)
        {
            if (Records[Index].Priority == Priority)
            {
                return Index;
            }
        }
    }

    return INDEX_NONE;
}

void FGamePlatformTelemetryBoundedBuffer::CountDrop(
    EGamePlatformTelemetryPriority Priority)
{
    ++DroppedSinceLastBatch;

    switch (Priority)
    {
    case EGamePlatformTelemetryPriority::Verbose:
        ++Diagnostics.DroppedVerbose;
        break;
    case EGamePlatformTelemetryPriority::Normal:
        ++Diagnostics.DroppedNormal;
        break;
    case EGamePlatformTelemetryPriority::CriticalTelemetry:
        ++Diagnostics.DroppedCritical;
        break;
    default:
        break;
    }
}
