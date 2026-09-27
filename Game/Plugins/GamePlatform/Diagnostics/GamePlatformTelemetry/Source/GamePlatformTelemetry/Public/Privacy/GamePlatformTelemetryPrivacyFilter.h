#pragma once

#include "CoreMinimal.h"
#include "Schema/GamePlatformTelemetrySchemaRegistry.h"

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryPrivacyFilter
{
public:
    static EGamePlatformTelemetryRecordResult ValidateEvent(
        const FGamePlatformTelemetryEvent& Event,
        const FGamePlatformTelemetryEventDefinition& Definition,
        const FGamePlatformTelemetryLimits& Limits);

    static EGamePlatformTelemetryRecordResult ValidateMetric(
        const FGamePlatformTelemetryMetric& Metric,
        const FGamePlatformTelemetryMetricDefinition& Definition);

    static bool IsForbiddenKey(const FString& Key);
    static bool IsValidEventName(FName EventName);
    static int32 EstimateEventBytes(
        const FGamePlatformTelemetryEvent& Event);
    static int32 EstimateMetricBytes(
        const FGamePlatformTelemetryMetric& Metric);
};
