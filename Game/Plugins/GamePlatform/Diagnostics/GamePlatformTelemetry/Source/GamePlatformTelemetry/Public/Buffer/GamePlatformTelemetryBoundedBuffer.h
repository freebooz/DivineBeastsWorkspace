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

    /** 正式运行时入口：Context单独传入并在Buffer内共享，避免每条Event复制整组FString。 */
    bool EnqueueEvent(
        FGamePlatformTelemetryEvent Event,
        int32 EstimatedBytes,
        const FGamePlatformTelemetryContext& Context);

    bool EnqueueMetric(
        FGamePlatformTelemetryMetric Metric,
        int32 EstimatedBytes);

    /** 正式运行时入口：Context单独传入并在Buffer内共享，避免每条Metric复制整组FString。 */
    bool EnqueueMetric(
        FGamePlatformTelemetryMetric Metric,
        int32 EstimatedBytes,
        const FGamePlatformTelemetryContext& Context);

    bool BuildBatch(
        const FGamePlatformTelemetryContext& SourceContext,
        FGamePlatformTelemetryBatch& OutBatch);

    FGamePlatformTelemetryDiagnostics GetDiagnostics() const;

    void CountSampledOut();
    void CountRateLimited();

    void Reset();

    /**
     * 丢弃当前仍在Buffer中的记录但保留累计Diagnostics；用于账号切换等隐私边界，
     * 防止旧会话未发送记录被后续认证上下文继续发送。返回实际丢弃记录数。
     */
    int32 DiscardQueuedRecords();

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
        int32 ContextEstimatedBytes = 0;
        /** 多条连续记录共享同一不可变Context，避免重复持有十余个FString。 */
        TSharedPtr<const FGamePlatformTelemetryContext, ESPMode::ThreadSafe> Context;
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
    /** 最近一次不可变上下文快照；连续相同Context直接复用共享对象。 */
    TSharedPtr<const FGamePlatformTelemetryContext, ESPMode::ThreadSafe> LastSharedContext;

    int32 ActiveRecordCount() const { return Records.Num() - HeadIndex; }
    /** 达到阈值时一次性压缩已消费前缀，平摊正常批次消费的数组搬移成本。 */
    void CompactConsumedPrefixIfNeeded(bool bForce = false);

    /**
     * 对最近有限窗口内同Context/同Labels的Counter/Gauge做原位合并：Counter累加，Gauge保留最新值。
     * Histogram/Duration不合并，避免在没有正式桶协议时破坏统计语义。
     */
    bool TryCoalesceMetric(
        const FGamePlatformTelemetryMetric& Metric,
        const FGamePlatformTelemetryContext& Context);
    TSharedPtr<const FGamePlatformTelemetryContext, ESPMode::ThreadSafe> ResolveSharedContext(
        const FGamePlatformTelemetryContext& Context);

    bool EnsureCapacityFor(
        EGamePlatformTelemetryPriority IncomingPriority,
        int32 IncomingBytes);

    int32 FindDropCandidate(
        EGamePlatformTelemetryPriority IncomingPriority) const;

    void CountDrop(
        EGamePlatformTelemetryPriority Priority);
};
