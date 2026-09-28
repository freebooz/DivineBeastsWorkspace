#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Sinks/GamePlatformTelemetrySink.h"
#include "Transport/GamePlatformTelemetryTransport.h"

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryRetrySettings
{
    float RetryMinSeconds = 0.5f;
    float RetryMaxSeconds = 10.0f;
    float MaxRetryAgeSeconds = 30.0f;
    int32 MaxRetries = 4;
    int32 MaxPendingBatches = 8;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryNetworkSink final
    : public IGamePlatformTelemetrySink
    , public TSharedFromThis<
        FGamePlatformTelemetryNetworkSink,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformTelemetryNetworkSink(
        TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe>
            InTransport,
        FGamePlatformTelemetryRetrySettings InRetrySettings);

    virtual bool Start() override;

    virtual void SubmitBatch(
        FGamePlatformTelemetryBatch Batch,
        FGamePlatformTelemetrySubmitCompletion Completion) override;

    virtual void Flush() override {}

    virtual void Shutdown(float BudgetSeconds) override;

    virtual FGamePlatformTelemetrySinkStatus GetHealth() const override;

private:
    mutable FCriticalSection Mutex;

    TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe>
        Transport;

    FGamePlatformTelemetryRetrySettings RetrySettings;
    FGamePlatformTelemetrySinkStatus Status;

    int32 PendingBatches = 0;
    bool bStarted = false;
    bool bShuttingDown = false;

    /** 已安排的一次性重试任务；数量受 MaxPendingBatches * MaxRetries 的硬上限约束。 */
    TArray<FTSTicker::FDelegateHandle> RetryTickerHandles;
    /** 非阻塞关停预算到期任务；在预算内允许已发HTTP自然完成。 */
    FTSTicker::FDelegateHandle ShutdownTickerHandle;

    void SubmitAttempt(
        FGamePlatformTelemetryBatch Batch,
        int32 Attempt,
        double FirstAttemptSeconds,
        FGamePlatformTelemetrySubmitCompletion Completion);

    void FinishPending(
        bool bAccepted,
        const FString& Error,
        FGamePlatformTelemetrySubmitCompletion Completion);

    /** 在游戏线程安排有界指数退避；断网恢复后由同一Batch继续提交。 */
    void ScheduleRetry(
        FGamePlatformTelemetryBatch Batch,
        int32 NextAttempt,
        double FirstAttemptSeconds,
        float RetryAfterSeconds,
        FGamePlatformTelemetrySubmitCompletion Completion);

    /** 关停时撤销尚未触发的Retry Ticker，避免延迟闭包继续持有Batch。 */
    void CancelRetryTickers();
    /** 关停预算到期后取消仍在飞行的HTTP，并把剩余批次计入Dropped。 */
    void FinalizeShutdownAfterBudget();

    float ComputeRetryDelay(
        const FGuid& BatchId,
        int32 Attempt,
        float RetryAfterSeconds) const;
};
