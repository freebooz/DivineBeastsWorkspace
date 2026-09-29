#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryBoundedBuffer
{
public:
    explicit FGamePlatformTelemetryBoundedBuffer(
        FGamePlatformTelemetryLimits InLimits);

    bool EnqueueEvent(
        FGamePlatformTelemetryEvent Event,
        int32 EstimatedBytes);

    bool EnqueueMetric(
        FGamePlatformTelemetryMetric Metric,
        int32 EstimatedBytes);

    bool BuildBatch(
        const FGamePlatformTelemetryContext& SourceContext,
        FGamePlatformTelemetryBatch& OutBatch);

    FGamePlatformTelemetryDiagnostics GetDiagnostics() const;

    void CountSampledOut();
    void CountRateLimited();

    void Reset();

private:
    enum class ERecordKind : uint8
    {
        Event,
        Metric
    };

    struct FQueuedRecord
    {
        ERecordKind Kind = ERecordKind::Event;
        EGamePlatformTelemetryPriority Priority =
            EGamePlatformTelemetryPriority::Normal;
        int32 EstimatedBytes = 0;
        FGamePlatformTelemetryEvent Event;
        FGamePlatformTelemetryMetric Metric;
    };

    mutable FCriticalSection Mutex;
    FGamePlatformTelemetryLimits Limits;
    TArray<FQueuedRecord> Records;
    /** 已消费但尚未物理压缩的前缀长度；正常BuildBatch只推进头索引，避免频繁RemoveAt(0,N)搬移。 */
    int32 HeadIndex = 0;
    int64 CurrentBytes = 0;
    int64 DroppedSinceLastBatch = 0;
    FGamePlatformTelemetryDiagnostics Diagnostics;

    int32 ActiveRecordCount() const { return Records.Num() - HeadIndex; }
    /** 达到阈值时一次性压缩已消费前缀，平摊正常批次消费的数组搬移成本。 */
    void CompactConsumedPrefixIfNeeded(bool bForce = false);

    bool EnsureCapacityFor(
        EGamePlatformTelemetryPriority IncomingPriority,
        int32 IncomingBytes);

    int32 FindDropCandidate(
        EGamePlatformTelemetryPriority IncomingPriority) const;

    void CountDrop(
        EGamePlatformTelemetryPriority Priority);
};
