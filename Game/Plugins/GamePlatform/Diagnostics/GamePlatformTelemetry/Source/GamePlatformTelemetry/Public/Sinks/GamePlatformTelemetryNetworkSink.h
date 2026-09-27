#pragma once

#include "CoreMinimal.h"
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

    void SubmitAttempt(
        FGamePlatformTelemetryBatch Batch,
        int32 Attempt,
        double FirstAttemptSeconds,
        FGamePlatformTelemetrySubmitCompletion Completion);

    void FinishPending(
        bool bAccepted,
        const FString& Error,
        FGamePlatformTelemetrySubmitCompletion Completion);

    float ComputeRetryDelay(
        const FGuid& BatchId,
        int32 Attempt,
        float RetryAfterSeconds) const;
};
