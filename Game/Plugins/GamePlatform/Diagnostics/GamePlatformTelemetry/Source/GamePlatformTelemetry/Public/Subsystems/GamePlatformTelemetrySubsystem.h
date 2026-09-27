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
    uint64 SessionGeneration = 0;

    bool TickFlush(float DeltaSeconds);

    EGamePlatformTelemetryRecordResult RecordMetricInternal(
        FName MetricName,
        EGamePlatformTelemetryMetricType Type,
        double Value,
        const TMap<FName, FString>& Labels);

    FString StableSamplingKey(
        const FGamePlatformTelemetryContext& ContextSnapshot) const;
};
