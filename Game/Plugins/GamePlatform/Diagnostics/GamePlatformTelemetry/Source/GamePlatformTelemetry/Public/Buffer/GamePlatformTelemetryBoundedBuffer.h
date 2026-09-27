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
    int64 CurrentBytes = 0;
    int64 DroppedSinceLastBatch = 0;
    FGamePlatformTelemetryDiagnostics Diagnostics;

    bool EnsureCapacityFor(
        EGamePlatformTelemetryPriority IncomingPriority,
        int32 IncomingBytes);

    int32 FindDropCandidate(
        EGamePlatformTelemetryPriority IncomingPriority) const;

    void CountDrop(
        EGamePlatformTelemetryPriority Priority);
};
