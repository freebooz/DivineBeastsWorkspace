#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"

namespace
{
int32 EstimateContextBytes(const FGamePlatformTelemetryContext& Context)
{
    const FString* Values[] = {
        &Context.BuildVersion, &Context.ContentRevision, &Context.Platform,
        &Context.Environment, &Context.SourceRole, &Context.ServerRole,
        &Context.Region, &Context.MapId, &Context.WorldId, &Context.ExperienceId,
        &Context.ServerInstanceId, &Context.MatchId, &Context.ArenaModeId,
        &Context.SessionId, &Context.PseudonymousPlayerId, &Context.CorrelationId,
        &Context.TransactionId
    };

    int32 Bytes = 256;
    for (const FString* Value : Values)
    {
        Bytes += Value ? Value->Len() * static_cast<int32>(sizeof(TCHAR)) : 0;
    }
    return Bytes;
}

bool SameContext(const FGamePlatformTelemetryContext& A, const FGamePlatformTelemetryContext& B)
{
    return A.BuildVersion == B.BuildVersion && A.ContentRevision == B.ContentRevision &&
        A.Platform == B.Platform && A.Environment == B.Environment &&
        A.SourceRole == B.SourceRole && A.ServerRole == B.ServerRole &&
        A.Region == B.Region && A.MapId == B.MapId && A.WorldId == B.WorldId &&
        A.ExperienceId == B.ExperienceId && A.ServerInstanceId == B.ServerInstanceId &&
        A.MatchId == B.MatchId && A.ArenaModeId == B.ArenaModeId &&
        A.SessionId == B.SessionId && A.PseudonymousPlayerId == B.PseudonymousPlayerId &&
        A.CorrelationId == B.CorrelationId && A.TransactionId == B.TransactionId;
}

bool SameLabels(const TMap<FName, FString>& A, const TMap<FName, FString>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const TPair<FName, FString>& Pair : A)
    {
        const FString* Other = B.Find(Pair.Key);
        if (!Other || *Other != Pair.Value)
        {
            return false;
        }
    }
    return true;
}

constexpr int32 MaxMetricCoalesceScan = 64;
}


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
    const FGamePlatformTelemetryContext Context = Event.Context;
    return EnqueueEvent(MoveTemp(Event), EstimatedBytes, Context);
}

bool FGamePlatformTelemetryBoundedBuffer::EnqueueEvent(
    FGamePlatformTelemetryEvent Event,
    int32 EstimatedBytes,
    const FGamePlatformTelemetryContext& Context)
{
    if (EstimatedBytes <= 0 || EstimatedBytes > Limits.MaxEventBytes)
    {
        return false;
    }

    FScopeLock Lock(&Mutex);
    const int32 ContextBytes = EstimateContextBytes(Context);
    const int32 BufferBytes = EstimatedBytes + ContextBytes;
    if (!EnsureCapacityFor(Event.Priority, BufferBytes))
    {
        CountDrop(Event.Priority);
        return false;
    }

    FQueuedRecord Record;
    Record.Kind = ERecordKind::Event;
    Record.Priority = Event.Priority;
    Record.EstimatedBytes = EstimatedBytes;
    Record.ContextEstimatedBytes = ContextBytes;
    Record.Context = ResolveSharedContext(Context);
    Event.Context = {};
    Record.Event = MoveTemp(Event);

    CurrentBytes += BufferBytes;
    Records.Add(MoveTemp(Record));
    ++Diagnostics.RecordedTotal;
    Diagnostics.BufferDepth = ActiveRecordCount();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

bool FGamePlatformTelemetryBoundedBuffer::EnqueueMetric(
    FGamePlatformTelemetryMetric Metric,
    int32 EstimatedBytes)
{
    const FGamePlatformTelemetryContext Context = Metric.Context;
    return EnqueueMetric(MoveTemp(Metric), EstimatedBytes, Context);
}

bool FGamePlatformTelemetryBoundedBuffer::EnqueueMetric(
    FGamePlatformTelemetryMetric Metric,
    int32 EstimatedBytes,
    const FGamePlatformTelemetryContext& Context)
{
    if (EstimatedBytes <= 0)
    {
        return false;
    }

    FScopeLock Lock(&Mutex);
    if (TryCoalesceMetric(Metric, Context))
    {
        ++Diagnostics.RecordedTotal;
        ++Diagnostics.CoalescedMetricTotal;
        Diagnostics.BufferDepth = ActiveRecordCount();
        Diagnostics.BufferBytes = CurrentBytes;
        return true;
    }

    const int32 ContextBytes = EstimateContextBytes(Context);
    const int32 BufferBytes = EstimatedBytes + ContextBytes;
    if (!EnsureCapacityFor(EGamePlatformTelemetryPriority::Normal, BufferBytes))
    {
        CountDrop(EGamePlatformTelemetryPriority::Normal);
        return false;
    }

    FQueuedRecord Record;
    Record.Kind = ERecordKind::Metric;
    Record.Priority = EGamePlatformTelemetryPriority::Normal;
    Record.EstimatedBytes = EstimatedBytes;
    Record.ContextEstimatedBytes = ContextBytes;
    Record.Context = ResolveSharedContext(Context);
    Metric.Context = {};
    Record.Metric = MoveTemp(Metric);

    CurrentBytes += BufferBytes;
    Records.Add(MoveTemp(Record));
    ++Diagnostics.RecordedTotal;
    Diagnostics.BufferDepth = ActiveRecordCount();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

bool FGamePlatformTelemetryBoundedBuffer::BuildBatch(
    const FGamePlatformTelemetryContext&,
    FGamePlatformTelemetryBatch& OutBatch)
{
    FScopeLock Lock(&Mutex);

    if (ActiveRecordCount() <= 0)
    {
        return false;
    }

    const FQueuedRecord& First = Records[HeadIndex];
    if (!First.Context.IsValid())
    {
        return false;
    }
    const FGamePlatformTelemetryContext& BatchContext = *First.Context;

    OutBatch = {};
    OutBatch.BatchId = FGuid::NewGuid();
    OutBatch.SchemaVersion = 1;
    OutBatch.SourceContext = BatchContext;
    OutBatch.CreatedAtUtc = FDateTime::UtcNow();
    OutBatch.DroppedSinceLastBatch = DroppedSinceLastBatch;

    int32 Count = 0;
    // Batch只发送一次公共Context，因此这里把Context成本计入真实批次预算，而不是对每条记录重复估算。
    int32 Bytes = EstimateContextBytes(BatchContext) + 256;
    int32 ConsumeCount = 0;

    for (int32 Index = HeadIndex; Index < Records.Num(); ++Index)
    {
        const FQueuedRecord& Record = Records[Index];
        if (!Record.Context.IsValid() || !SameContext(BatchContext, *Record.Context))
        {
            // Context切换必须自然切批，避免世界/会话切换前后的记录被错误归到同一SourceContext。
            break;
        }

        if (Count >= Limits.MaxBatchEvents ||
            Bytes + Record.EstimatedBytes > Limits.MaxBatchBytes)
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

    for (int32 Offset = 0; Offset < ConsumeCount; ++Offset)
    {
        FQueuedRecord& Consumed = Records[HeadIndex + Offset];
        CurrentBytes -= Consumed.EstimatedBytes + Consumed.ContextEstimatedBytes;
        // 只保留空槽位供摊销压缩，不保留已消费Event/Metric里的FString/TArray堆内存，确保真实内存仍受有界策略约束。
        Consumed = FQueuedRecord{};
    }

    HeadIndex += ConsumeCount;
    CompactConsumedPrefixIfNeeded();

    DroppedSinceLastBatch = 0;
    Diagnostics.BufferDepth = ActiveRecordCount();
    Diagnostics.BufferBytes = CurrentBytes;
    return true;
}

FGamePlatformTelemetryDiagnostics
FGamePlatformTelemetryBoundedBuffer::GetDiagnostics() const
{
    FScopeLock Lock(&Mutex);
    FGamePlatformTelemetryDiagnostics Result = Diagnostics;
    Result.BufferDepth = ActiveRecordCount();
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
    HeadIndex = 0;
    CurrentBytes = 0;
    DroppedSinceLastBatch = 0;
    Diagnostics = {};
    LastSharedContext.Reset();
}

int32 FGamePlatformTelemetryBoundedBuffer::DiscardQueuedRecords()
{
    FScopeLock Lock(&Mutex);
    const int32 Discarded = ActiveRecordCount();
    for (int32 Index = HeadIndex; Index < Records.Num(); ++Index)
    {
        ++DroppedSinceLastBatch;
        switch (Records[Index].Priority)
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

    Records.Reset();
    HeadIndex = 0;
    CurrentBytes = 0;
    LastSharedContext.Reset();
    Diagnostics.BufferDepth = 0;
    Diagnostics.BufferBytes = 0;
    return Discarded;
}

void FGamePlatformTelemetryBoundedBuffer::CompactConsumedPrefixIfNeeded(bool bForce)
{
    if (HeadIndex <= 0)
    {
        return;
    }

    const bool bShouldCompact = bForce || HeadIndex >= 256 || HeadIndex * 2 >= Records.Num();
    if (!bShouldCompact)
    {
        return;
    }

    Records.RemoveAt(0, HeadIndex, EAllowShrinking::No);
    HeadIndex = 0;
}

bool FGamePlatformTelemetryBoundedBuffer::TryCoalesceMetric(
    const FGamePlatformTelemetryMetric& Metric,
    const FGamePlatformTelemetryContext& Context)
{
    if (Metric.Type != EGamePlatformTelemetryMetricType::Counter &&
        Metric.Type != EGamePlatformTelemetryMetricType::Gauge)
    {
        return false;
    }

    const int32 BeginIndex = FMath::Max(HeadIndex, Records.Num() - MaxMetricCoalesceScan);
    for (int32 Index = Records.Num() - 1; Index >= BeginIndex; --Index)
    {
        FQueuedRecord& Record = Records[Index];
        if (Record.Kind != ERecordKind::Metric ||
            Record.Metric.Name != Metric.Name ||
            Record.Metric.Type != Metric.Type ||
            !Record.Context.IsValid() || !SameContext(*Record.Context, Context) ||
            !SameLabels(Record.Metric.Labels, Metric.Labels))
        {
            continue;
        }

        if (Metric.Type == EGamePlatformTelemetryMetricType::Counter)
        {
            Record.Metric.Value += Metric.Value;
        }
        else
        {
            Record.Metric.Value = Metric.Value;
        }
        Record.Metric.TimestampUtc = Metric.TimestampUtc;
        return true;
    }
    return false;
}

TSharedPtr<const FGamePlatformTelemetryContext, ESPMode::ThreadSafe>
FGamePlatformTelemetryBoundedBuffer::ResolveSharedContext(
    const FGamePlatformTelemetryContext& Context)
{
    if (LastSharedContext.IsValid() && SameContext(*LastSharedContext, Context))
    {
        return LastSharedContext;
    }

    LastSharedContext = MakeShared<FGamePlatformTelemetryContext, ESPMode::ThreadSafe>(Context);
    return LastSharedContext;
}

bool FGamePlatformTelemetryBoundedBuffer::EnsureCapacityFor(
    EGamePlatformTelemetryPriority IncomingPriority,
    int32 IncomingBytes)
{
    if (IncomingBytes > Limits.MaxBufferBytes)
    {
        return false;
    }

    CompactConsumedPrefixIfNeeded();

    while (ActiveRecordCount() >= Limits.MaxBufferEvents ||
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
        CurrentBytes -= Records[Candidate].EstimatedBytes + Records[Candidate].ContextEstimatedBytes;
        if (Candidate == HeadIndex)
        {
            // 头部被驱逐时同样立即释放记录内部堆内存，再推进逻辑头。
            Records[Candidate] = FQueuedRecord{};
            ++HeadIndex;
            CompactConsumedPrefixIfNeeded();
        }
        else
        {
            Records.RemoveAt(Candidate, 1, EAllowShrinking::No);
        }
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

        for (int32 Index = HeadIndex; Index < Records.Num(); ++Index)
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
