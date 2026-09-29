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
    Status.PendingCapacity = RetrySettings.MaxPendingBatches;
}

bool FGamePlatformTelemetryNetworkSink::Start()
{
    check(IsInGameThread());
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
    check(IsInGameThread());
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
            Status.PendingBatches = PendingBatches;
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
    check(IsInGameThread());
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

    // Retry副本与提交参数分离，避免同一Batch同时作为函数实参又被Lambda Move捕获的求值顺序隐患。
    FGamePlatformTelemetryBatch RetryBatch = Batch;
    TSharedRef<FGamePlatformTelemetrySubmitCompletion, ESPMode::ThreadSafe> CompletionHolder =
        MakeShared<FGamePlatformTelemetrySubmitCompletion, ESPMode::ThreadSafe>(MoveTemp(Completion));

    const bool bStartedRequest =
        LocalTransport->BeginSubmitBatch(
            Batch,
            [WeakThis,
             RetryBatch = MoveTemp(RetryBatch),
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

                Self->ScheduleRetry(
                    MoveTemp(RetryBatch),
                    Attempt + 1,
                    FirstAttemptSeconds,
                    Result.RetryAfterSeconds,
                    MoveTemp(*CompletionHolder));
            });

    if (!bStartedRequest)
    {
        const double Age = FPlatformTime::Seconds() - FirstAttemptSeconds;
        if (Attempt < RetrySettings.MaxRetries && Age < RetrySettings.MaxRetryAgeSeconds)
        {
            // ProcessRequest启动失败通常是瞬时网络/HTTP层不可用，按同一有限退避策略重试而不是立即永久丢批。
            ScheduleRetry(
                MoveTemp(Batch),
                Attempt + 1,
                FirstAttemptSeconds,
                0.0f,
                MoveTemp(*CompletionHolder));
        }
        else
        {
            FinishPending(false, TEXT("transport_start_failed"), MoveTemp(*CompletionHolder));
        }
    }
}

void FGamePlatformTelemetryNetworkSink::ScheduleRetry(
    FGamePlatformTelemetryBatch Batch,
    int32 NextAttempt,
    double FirstAttemptSeconds,
    float RetryAfterSeconds,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    check(IsInGameThread());
    bool bIsShuttingDown = false;
    {
        FScopeLock Lock(&Mutex);
        bIsShuttingDown = bShuttingDown;
    }
    if (bIsShuttingDown)
    {
        FinishPending(false, TEXT("sink_shutdown"), MoveTemp(Completion));
        return;
    }

    const float Delay = ComputeRetryDelay(Batch.BatchId, NextAttempt, RetryAfterSeconds);
    TWeakPtr<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe> WeakThis = AsShared();
    const FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [WeakThis,
             Batch = MoveTemp(Batch),
             NextAttempt,
             FirstAttemptSeconds,
             Completion = MoveTemp(Completion)](float) mutable
            {
                if (TSharedPtr<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe> Self = WeakThis.Pin())
                {
                    Self->SubmitAttempt(
                        MoveTemp(Batch),
                        NextAttempt,
                        FirstAttemptSeconds,
                        MoveTemp(Completion));
                }
                return false;
            }),
        Delay);

    FScopeLock Lock(&Mutex);
    if (!bShuttingDown)
    {
        RetryTickerHandles.Add(Handle);
    }
    else
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Handle);
    }
}

void FGamePlatformTelemetryNetworkSink::CancelRetryTickers()
{
    check(IsInGameThread());
    TArray<FTSTicker::FDelegateHandle> Handles;
    {
        FScopeLock Lock(&Mutex);
        Handles = MoveTemp(RetryTickerHandles);
        RetryTickerHandles.Reset();
    }

    for (const FTSTicker::FDelegateHandle& Handle : Handles)
    {
        if (Handle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(Handle);
        }
    }
}

void FGamePlatformTelemetryNetworkSink::FinishPending(
    bool bAccepted,
    const FString& Error,
    FGamePlatformTelemetrySubmitCompletion Completion)
{
    check(IsInGameThread());
    {
        FScopeLock Lock(&Mutex);
        PendingBatches = FMath::Max(0, PendingBatches - 1);
        Status.PendingBatches = PendingBatches;
        if (PendingBatches == 0)
        {
            // 所有重试链都已终止；已触发的一次性句柄无需继续保留。
            RetryTickerHandles.Reset();
        }

        if (!bShuttingDown)
        {
            if (bAccepted)
            {
                ++Status.SubmittedBatches;
                Status.Health =
                    EGamePlatformTelemetrySinkHealth::Healthy;
                Status.LastError.Reset();
                Status.LastSuccessUtc = FDateTime::UtcNow();
            }
            else
            {
                ++Status.FailedBatches;
                Status.Health =
                    EGamePlatformTelemetrySinkHealth::Degraded;
                Status.LastError = Error;
                Status.LastFailureUtc = FDateTime::UtcNow();
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
    float BudgetSeconds)
{
    check(IsInGameThread());
    CancelRetryTickers();

    bool bNeedsBudget = false;
    {
        FScopeLock Lock(&Mutex);
        bShuttingDown = true;
        bStarted = false;
        bNeedsBudget = PendingBatches > 0 && BudgetSeconds > 0.0f;
        Status.Health = bNeedsBudget
            ? EGamePlatformTelemetrySinkHealth::Degraded
            : EGamePlatformTelemetrySinkHealth::Stopped;
    }

    if (!bNeedsBudget)
    {
        FinalizeShutdownAfterBudget();
        return;
    }

    // 不阻塞游戏线程等待HTTP回调：保留Self到预算到期，期间已在飞请求仍可自然完成。
    TSharedRef<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe> KeepAlive = AsShared();
    ShutdownTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([KeepAlive](float)
        {
            KeepAlive->FinalizeShutdownAfterBudget();
            return false;
        }),
        FMath::Max(0.01f, BudgetSeconds));
}

void FGamePlatformTelemetryNetworkSink::FinalizeShutdownAfterBudget()
{
    check(IsInGameThread());
    TSharedPtr<IGamePlatformTelemetryTransport, ESPMode::ThreadSafe> LocalTransport;
    {
        FScopeLock Lock(&Mutex);
        LocalTransport = Transport;
        if (PendingBatches > 0)
        {
            Status.DroppedBatches += PendingBatches;
            PendingBatches = 0;
            Status.PendingBatches = 0;
        }
        Status.Health = EGamePlatformTelemetrySinkHealth::Stopped;
        ShutdownTickerHandle.Reset();
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
