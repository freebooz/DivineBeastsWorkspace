#include "Sinks/GamePlatformTelemetryNetworkSink.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "Misc/ScopeLock.h"

FGamePlatformTelemetryNetworkSink::
FGamePlatformTelemetryNetworkSink(
    TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe>
        InTransport,
    FGamePlatformTelemetryRetrySettings InRetrySettings)
    : Transport(MoveTemp(InTransport))
    , RetrySettings(MoveTemp(InRetrySettings))
{
    RetrySettings.RetryMinSeconds =
        FMath::Max(0.1f, RetrySettings.RetryMinSeconds);
    RetrySettings.RetryMaxSeconds =
        FMath::Max(
            RetrySettings.RetryMinSeconds,
            RetrySettings.RetryMaxSeconds);
    RetrySettings.MaxRetryAgeSeconds =
        FMath::Max(
            RetrySettings.RetryMaxSeconds,
            RetrySettings.MaxRetryAgeSeconds);
    RetrySettings.MaxRetries =
        FMath::Max(0, RetrySettings.MaxRetries);
    RetrySettings.MaxPendingBatches =
        FMath::Max(1, RetrySettings.MaxPendingBatches);
}

bool FGamePlatformTelemetryNetworkSink::Start()
{
    FScopeLock Lock(&Mutex);

    if (!Transport.IsValid())
    {
        Status.Health =
            EGamePlatformTelemetrySinkHealth::Unavailable;
        Status.LastError = TEXT("transport_missing");
        return false;
    }

    bStarted = true;
    bShuttingDown = false;
    Status.Health = EGamePlatformTelemetrySinkHealth::Healthy;
    Status.LastError.Reset();
    return true;
}

void FGamePlatformTelemetryNetworkSink::SubmitBatch(
    FGamePlatformTelemetryBatch Batch,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    bool bAcceptedForSubmission = false;

    {
        FScopeLock Lock(&Mutex);

        if (!bStarted ||
            bShuttingDown ||
            !Transport.IsValid())
        {
            ++Status.DroppedBatches;
        }
        else if (
            PendingBatches >= RetrySettings.MaxPendingBatches)
        {
            ++Status.DroppedBatches;
            Status.Health =
                EGamePlatformTelemetrySinkHealth::Degraded;
            Status.LastError = TEXT("pending_batch_capacity");
        }
        else
        {
            ++PendingBatches;
            bAcceptedForSubmission = true;
        }
    }

    if (!bAcceptedForSubmission)
    {
        // 外部回调必须在锁外执行，避免调用方重入Sink形成死锁。
        if (Completion)
        {
            Completion(false, false);
        }
        return;
    }

    SubmitAttempt(
        MoveTemp(Batch),
        0,
        FPlatformTime::Seconds(),
        MoveTemp(Completion));
}

void FGamePlatformTelemetryNetworkSink::SubmitAttempt(
    FGamePlatformTelemetryBatch Batch,
    int32 Attempt,
    double FirstAttemptSeconds,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe>
        LocalTransport;

    bool bCanSubmit = false;
    {
        FScopeLock Lock(&Mutex);
        bCanSubmit =
            !bShuttingDown &&
            Transport.IsValid();

        if (bCanSubmit)
        {
            LocalTransport = Transport;
        }
    }

    // 锁外完成，避免FinishPending再次获取同一Mutex形成递归死锁。
    if (!bCanSubmit)
    {
        FinishPending(
            false,
            TEXT("sink_shutdown"),
            MoveTemp(Completion));
        return;
    }

    TWeakPtr<
        FGamePlatformTelemetryNetworkSink,
        ESPMode::ThreadSafe> WeakThis = AsShared();

    // BeginSubmitBatch返回false时，Transport契约保证不会调用Completion。
    // 使用共享Holder避免把原Completion提前move进lambda后丢失启动失败回调。
    TSharedRef<
        FGamePlatformTelemetrySubmitCompletion,
        ESPMode::ThreadSafe> CompletionHolder =
        MakeShared<
            FGamePlatformTelemetrySubmitCompletion,
            ESPMode::ThreadSafe>(
                MoveTemp(Completion));

    const bool bStartedRequest =
        LocalTransport->BeginSubmitBatch(
            Batch,
            [WeakThis,
             Batch = MoveTemp(Batch),
             Attempt,
             FirstAttemptSeconds,
             CompletionHolder](
                FGamePlatformTelemetryTransportResult Result) mutable
            {
                TSharedPtr<
                    FGamePlatformTelemetryNetworkSink,
                    ESPMode::ThreadSafe> Self = WeakThis.Pin();

                if (!Self.IsValid())
                {
                    return;
                }

                if (Result.bAccepted)
                {
                    Self->FinishPending(
                        true,
                        FString(),
                        MoveTemp(*CompletionHolder));
                    return;
                }

                const double Age =
                    FPlatformTime::Seconds() -
                    FirstAttemptSeconds;

                const bool bCanRetry =
                    Result.bRetryable &&
                    Attempt < Self->RetrySettings.MaxRetries &&
                    Age < Self->RetrySettings.MaxRetryAgeSeconds;

                if (!bCanRetry)
                {
                    Self->FinishPending(
                        false,
                        Result.Error,
                        MoveTemp(*CompletionHolder));
                    return;
                }

                const float Delay =
                    Self->ComputeRetryDelay(
                        Batch.BatchId,
                        Attempt + 1,
                        Result.RetryAfterSeconds);

                TWeakPtr<
                    FGamePlatformTelemetryNetworkSink,
                    ESPMode::ThreadSafe> RetryWeak = Self;

                FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateLambda(
                        [RetryWeak,
                         Batch = MoveTemp(Batch),
                         Attempt,
                         FirstAttemptSeconds,
                         CompletionHolder](
                            float) mutable
                        {
                            if (TSharedPtr<
                                    FGamePlatformTelemetryNetworkSink,
                                    ESPMode::ThreadSafe> RetrySelf =
                                    RetryWeak.Pin())
                            {
                                RetrySelf->SubmitAttempt(
                                    MoveTemp(Batch),
                                    Attempt + 1,
                                    FirstAttemptSeconds,
                                    MoveTemp(*CompletionHolder));
                            }
                            return false;
                        }),
                    Delay);
            });

    if (!bStartedRequest)
    {
        FinishPending(
            false,
            TEXT("transport_start_failed"),
            MoveTemp(*CompletionHolder));
    }
}

void FGamePlatformTelemetryNetworkSink::FinishPending(
    bool bAccepted,
    const FString& Error,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    {
        FScopeLock Lock(&Mutex);
        PendingBatches = FMath::Max(0, PendingBatches - 1);

        if (!bShuttingDown)
        {
            if (bAccepted)
            {
                ++Status.SubmittedBatches;
                Status.Health =
                    EGamePlatformTelemetrySinkHealth::Healthy;
                Status.LastError.Reset();
            }
            else
            {
                ++Status.FailedBatches;
                Status.Health =
                    EGamePlatformTelemetrySinkHealth::Degraded;
                Status.LastError = Error;
            }
        }
    }

    if (Completion)
    {
        Completion(bAccepted, false);
    }
}

float FGamePlatformTelemetryNetworkSink::ComputeRetryDelay(
    const FGuid& BatchId,
    int32 Attempt,
    float RetryAfterSeconds) const
{
    if (RetryAfterSeconds > 0.0f)
    {
        return FMath::Min(
            RetryAfterSeconds,
            RetrySettings.RetryMaxSeconds);
    }

    const float Base =
        FMath::Min(
            RetrySettings.RetryMaxSeconds,
            RetrySettings.RetryMinSeconds *
                FMath::Pow(2.0f, static_cast<float>(Attempt - 1)));

    const uint32 Hash =
        FCrc::StrCrc32(
            *BatchId.ToString(EGuidFormats::Digits));
    const float Jitter =
        0.8f +
        0.4f *
        (static_cast<float>(Hash % 1000) / 999.0f);

    return FMath::Min(
        RetrySettings.RetryMaxSeconds,
        Base * Jitter);
}

void FGamePlatformTelemetryNetworkSink::Shutdown(
    float)
{
    TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe>
        LocalTransport;

    {
        FScopeLock Lock(&Mutex);
        bShuttingDown = true;
        bStarted = false;
        LocalTransport = Transport;
        Status.DroppedBatches += PendingBatches;
        PendingBatches = 0;
        Status.Health =
            EGamePlatformTelemetrySinkHealth::Stopped;
    }

    if (LocalTransport.IsValid())
    {
        LocalTransport->CancelAll();
    }
}

FGamePlatformTelemetrySinkStatus
FGamePlatformTelemetryNetworkSink::GetHealth() const
{
    FScopeLock Lock(&Mutex);
    return Status;
}
