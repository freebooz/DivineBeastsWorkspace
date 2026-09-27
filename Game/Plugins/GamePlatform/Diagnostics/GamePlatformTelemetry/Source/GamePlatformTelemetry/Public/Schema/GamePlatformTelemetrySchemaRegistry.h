#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryEventDefinition
{
    FName EventName = NAME_None;
    int32 SchemaVersion = 1;
    EGamePlatformTelemetrySamplingPolicy SamplingPolicy =
        EGamePlatformTelemetrySamplingPolicy::Always;
    double SamplingRate = 1.0;
    int32 SustainedRatePerSecond = 10;
    int32 Burst = 20;
    TMap<FName, EGamePlatformTelemetryPrivacyClass> AllowedAttributes;
};

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryMetricDefinition
{
    FName Name = NAME_None;
    EGamePlatformTelemetryMetricType Type =
        EGamePlatformTelemetryMetricType::Counter;
    FString Unit;
    TSet<FName> AllowedLabels;
    EGamePlatformTelemetrySamplingPolicy SamplingPolicy =
        EGamePlatformTelemetrySamplingPolicy::Always;
    double SamplingRate = 1.0;
    FString Owner;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetrySchemaRegistry
{
public:
    void RegisterEvent(
        FGamePlatformTelemetryEventDefinition Definition);

    void RegisterMetric(
        FGamePlatformTelemetryMetricDefinition Definition);

    const FGamePlatformTelemetryEventDefinition* FindEvent(
        FName EventName) const;

    const FGamePlatformTelemetryMetricDefinition* FindMetric(
        FName MetricName) const;

    static TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
        CreateFoundationDefaults();

private:
    TMap<FName, FGamePlatformTelemetryEventDefinition> Events;
    TMap<FName, FGamePlatformTelemetryMetricDefinition> Metrics;
};
