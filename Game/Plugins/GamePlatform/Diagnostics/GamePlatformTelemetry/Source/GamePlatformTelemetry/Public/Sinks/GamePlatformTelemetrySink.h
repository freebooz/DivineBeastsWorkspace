#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

UENUM()
enum class EGamePlatformTelemetrySinkHealth : uint8
{
    Stopped,
    Healthy,
    Degraded,
    Unavailable
};

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetrySinkStatus
{
    EGamePlatformTelemetrySinkHealth Health =
        EGamePlatformTelemetrySinkHealth::Stopped;
    FString LastError;
    int64 SubmittedBatches = 0;
    int64 FailedBatches = 0;
    int64 DroppedBatches = 0;
    /** 当前网络输出器已接纳但尚未终态完成的批次数；Null/Log Sink始终为0。 */
    int32 PendingBatches = 0;
    /** 0表示同步/无异步容量概念；NetworkSink填入真实MaxPendingBatches。 */
    int32 PendingCapacity = 0;
    /** 最近一次批次被输出器最终接受的UTC时间；不包含事件内容或身份数据。 */
    FDateTime LastSuccessUtc;
    /** 最近一次批次终态失败的UTC时间；具体错误只保留稳定错误摘要。 */
    FDateTime LastFailureUtc;
};

using FGamePlatformTelemetrySubmitCompletion =
    TFunction<void(bool bAccepted, bool bRetryable)>;

class GAMEPLATFORMTELEMETRY_API IGamePlatformTelemetrySink
{
public:
    virtual ~IGamePlatformTelemetrySink() = default;

    virtual bool Start() = 0;

    virtual void SubmitBatch(
        FGamePlatformTelemetryBatch Batch,
        FGamePlatformTelemetrySubmitCompletion Completion) = 0;

    virtual void Flush() = 0;

    virtual void Shutdown(float BudgetSeconds) = 0;

    virtual FGamePlatformTelemetrySinkStatus GetHealth() const = 0;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryNullSink final
    : public IGamePlatformTelemetrySink
{
public:
    virtual bool Start() override;
    virtual void SubmitBatch(
        FGamePlatformTelemetryBatch Batch,
        FGamePlatformTelemetrySubmitCompletion Completion) override;
    virtual void Flush() override {}
    virtual void Shutdown(float) override;
    virtual FGamePlatformTelemetrySinkStatus GetHealth() const override;

private:
    mutable FCriticalSection Mutex;
    FGamePlatformTelemetrySinkStatus Status;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryLogSink final
    : public IGamePlatformTelemetrySink
{
public:
    virtual bool Start() override;
    virtual void SubmitBatch(
        FGamePlatformTelemetryBatch Batch,
        FGamePlatformTelemetrySubmitCompletion Completion) override;
    virtual void Flush() override {}
    virtual void Shutdown(float) override;
    virtual FGamePlatformTelemetrySinkStatus GetHealth() const override;

private:
    mutable FCriticalSection Mutex;
    FGamePlatformTelemetrySinkStatus Status;
};
