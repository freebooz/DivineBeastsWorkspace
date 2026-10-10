#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

/** 平台内部有界缓冲：由实例遥测子系统独占拥有，锁保护入队/批消费；容量不足按Schema优先级丢弃并计数。 */
class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryBoundedBuffer
{
public:
    /** 构造使用已规范化记录数/字节/批次预算，不负责认证、资产或网络生命周期。 */
    explicit FGamePlatformTelemetryBoundedBuffer(
        FGamePlatformTelemetryLimits InLimits);

    /** 消费事件值；EstimatedBytes为隐私校验器估计的字节数，预算不足返回false并计入丢弃诊断。 */
    bool EnqueueEvent(
        FGamePlatformTelemetryEvent Event,
        int32 EstimatedBytes);

    /** 正式运行时入口：Context单独传入并在Buffer内共享，避免每条Event复制整组FString。 */
    bool EnqueueEvent(
        FGamePlatformTelemetryEvent Event,
        int32 EstimatedBytes,
        const FGamePlatformTelemetryContext& Context);

    /** 消费指标值；结构/标签须已校验，失败不入队，现有记录按优先级保持。 */
    bool EnqueueMetric(
        FGamePlatformTelemetryMetric Metric,
        int32 EstimatedBytes);

    /** 正式运行时入口：Context单独传入并在Buffer内共享，避免每条Metric复制整组FString。 */
    bool EnqueueMetric(
        FGamePlatformTelemetryMetric Metric,
        int32 EstimatedBytes,
        const FGamePlatformTelemetryContext& Context);

    /** 取出预算内的批次，按原不可变上下文分组；无可提交记录返回false，不创建空成功批次。 */
    bool BuildBatch(
        const FGamePlatformTelemetryContext& SourceContext,
        FGamePlatformTelemetryBatch& OutBatch);

    /** 在锁内复制当前累计统计；不暴露队列内玩家属性。 */
    FGamePlatformTelemetryDiagnostics GetDiagnostics() const;

    /** 上层采样主动跳过时累计一条，不入队且不消耗字节预算。 */
    void CountSampledOut();
    /** 上层限速拒绝时累计一条，不作为网络失败重试。 */
    void CountRateLimited();

    /** 子系统关闭/重新初始化时清空记录、上下文和累计统计，不触碰其他实例。 */
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
