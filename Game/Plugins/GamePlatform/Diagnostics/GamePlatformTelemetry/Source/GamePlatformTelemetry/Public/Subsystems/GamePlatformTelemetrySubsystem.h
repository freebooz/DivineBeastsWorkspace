#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformTelemetryTypes.h"
#include "GamePlatformTelemetrySubsystem.generated.h"

class FGamePlatformTelemetryBoundedBuffer;
class FGamePlatformTelemetryRateLimiter;
class FGamePlatformTelemetrySchemaRegistry;
class IGamePlatformTelemetrySink;

UCLASS()
class GAMEPLATFORMTELEMETRY_API UGamePlatformTelemetrySubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    /**
     * 线程契约：本子系统公开控制、上下文、记录与刷新接口均为 Game Thread Only（仅游戏线程）。
     * HTTP 完成回调会显式投递回游戏线程；需要后台线程生产遥测时，应先进入独立Recorder入口，
     * 不得直接从工作线程访问 UObject 子系统。
     */

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

    bool ConfigureSink(
        TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
            InSink);

    void SetEnabled(bool bInEnabled);

    void SetSamplingSeed(FString InSamplingSeed);

    void SetTraceBridgeEnabled(bool bInEnabled);

    void SetContentRevision(FString InContentRevision);

    void SetEnvironment(FString InEnvironment);

    void SetServerContext(
        FString InServerRole,
        FString InRegion,
        FString InServerInstanceId);

    void BeginSession(
        FString InSessionId,
        FString InPseudonymousPlayerId);

    void EndSession();

    void UpdateWorldContext(
        FString InMapId,
        FString InWorldId,
        FString InExperienceId,
        FString InMatchId,
        FString InArenaModeId);

    void UpdateCorrelationContext(
        FString InCorrelationId,
        FString InTransactionId);

    void BeforeWorldTravel();

    EGamePlatformTelemetryRecordResult RecordEvent(
        FGamePlatformTelemetryEvent Event);

    EGamePlatformTelemetryRecordResult RecordMetric(
        FGamePlatformTelemetryMetric Metric);

    EGamePlatformTelemetryRecordResult IncrementCounter(
        FName MetricName,
        double Delta,
        const TMap<FName, FString>& Labels = {});

    EGamePlatformTelemetryRecordResult RecordGauge(
        FName MetricName,
        double Value,
        const TMap<FName, FString>& Labels = {});

    EGamePlatformTelemetryRecordResult RecordHistogram(
        FName MetricName,
        double Value,
        const TMap<FName, FString>& Labels = {});

    EGamePlatformTelemetryRecordResult RecordDuration(
        FName MetricName,
        double Milliseconds,
        const TMap<FName, FString>& Labels = {});

    bool FlushBestEffort();

    /**
     * 账号切换/隐私边界专用：撤销待刷新任务并丢弃尚未进入Sink的旧上下文记录。
     * 已进入旧NetworkSink的批次必须通过切换/Shutdown旧Sink终止其Retry链。
     */
    int32 DiscardBufferedRecordsForPrivacyBoundary();

    UFUNCTION(BlueprintPure, Category="Telemetry")
    FGamePlatformTelemetryDiagnostics GetDiagnostics() const;

    FGamePlatformTelemetryContext GetContextSnapshot() const;

    TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
        GetSchemaRegistry() const;

private:
    mutable FCriticalSection ContextMutex;
    mutable FCriticalSection SinkMutex;

    FGamePlatformTelemetryContext Context;
    FString SamplingSeed = TEXT("gameplatform");

    FGamePlatformTelemetryLimits Limits;

    TSharedPtr<
        FGamePlatformTelemetrySchemaRegistry,
        ESPMode::ThreadSafe> SchemaRegistry;

    TUniquePtr<FGamePlatformTelemetryBoundedBuffer> Buffer;
    TUniquePtr<FGamePlatformTelemetryRateLimiter> RateLimiter;

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> Sink;

    FThreadSafeCounter64 Sequence;
    FTSTicker::FDelegateHandle FlushTickerHandle;

    bool bEnabled = true;
    bool bTraceBridgeEnabled = false;
    bool bFlushInProgress = false;
    uint64 SessionGeneration = 0;
    FDateTime LastFlushUtc;
    int32 LastFlushRecords = 0;

    bool TickFlush(float DeltaSeconds);
    /** 按需安排一次刷新；Buffer为空时不保留常驻Ticker。 */
    void ScheduleFlush(float DelaySeconds);
    /** 撤销尚未触发的一次性刷新。 */
    void CancelScheduledFlush();
    /** 新记录入队后根据批次阈值决定立即刷新还是安排延迟刷新。 */
    void RequestFlushAfterRecord();
    /** 上下文字段统一去除换行并限制长度，防止异常ID放大每条遥测记录。 */
    FString SanitizeContextValue(FString Value) const;

    EGamePlatformTelemetryRecordResult RecordMetricInternal(
        FName MetricName,
        EGamePlatformTelemetryMetricType Type,
        double Value,
        const TMap<FName, FString>& Labels);

    FString StableSamplingKey(
        const FGamePlatformTelemetryContext& ContextSnapshot) const;
};
