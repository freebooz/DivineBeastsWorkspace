#include "Schema/GamePlatformTelemetrySchemaRegistry.h"

namespace
{
FGamePlatformTelemetryEventDefinition FoundationEvent(
    const TCHAR* Name,
    EGamePlatformTelemetrySamplingPolicy Sampling =
        EGamePlatformTelemetrySamplingPolicy::Always,
    double Rate = 1.0)
{
    FGamePlatformTelemetryEventDefinition Definition;
    Definition.EventName = FName(Name);
    Definition.SchemaVersion = 1;
    Definition.SamplingPolicy = Sampling;
    Definition.SamplingRate = Rate;
    Definition.SustainedRatePerSecond = 10;
    Definition.Burst = 20;

    Definition.AllowedAttributes.Add(
        TEXT("result"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("error_code"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("duration_ms"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("product_id"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("offer_id"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("provider_type"),
        EGamePlatformTelemetryPrivacyClass::Operational);
    Definition.AllowedAttributes.Add(
        TEXT("correlation_id"),
        EGamePlatformTelemetryPrivacyClass::Pseudonymous);
    return Definition;
}

FGamePlatformTelemetryMetricDefinition Metric(
    const TCHAR* Name,
    EGamePlatformTelemetryMetricType Type,
    const TCHAR* Unit)
{
    FGamePlatformTelemetryMetricDefinition Definition;
    Definition.Name = FName(Name);
    Definition.Type = Type;
    Definition.Unit = Unit;
    Definition.Owner = TEXT("GamePlatform");

    for (const FName Label : {
             FName(TEXT("build_version")),
             FName(TEXT("environment")),
             FName(TEXT("platform")),
             FName(TEXT("server_role")),
             FName(TEXT("region")),
             FName(TEXT("arena_mode")),
             FName(TEXT("result")),
             FName(TEXT("error_code")),
             FName(TEXT("service")),
             FName(TEXT("operation"))})
    {
        Definition.AllowedLabels.Add(Label);
    }

    return Definition;
}
}

void FGamePlatformTelemetrySchemaRegistry::RegisterEvent(
    FGamePlatformTelemetryEventDefinition Definition)
{
    if (!Definition.EventName.IsNone() &&
        Definition.SchemaVersion > 0)
    {
        Events.Add(Definition.EventName, MoveTemp(Definition));
    }
}

void FGamePlatformTelemetrySchemaRegistry::RegisterMetric(
    FGamePlatformTelemetryMetricDefinition Definition)
{
    if (!Definition.Name.IsNone())
    {
        Metrics.Add(Definition.Name, MoveTemp(Definition));
    }
}

const FGamePlatformTelemetryEventDefinition*
FGamePlatformTelemetrySchemaRegistry::FindEvent(
    FName EventName) const
{
    return Events.Find(EventName);
}

const FGamePlatformTelemetryMetricDefinition*
FGamePlatformTelemetrySchemaRegistry::FindMetric(
    FName MetricName) const
{
    return Metrics.Find(MetricName);
}

TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
FGamePlatformTelemetrySchemaRegistry::CreateFoundationDefaults()
{
    TSharedRef<
        FGamePlatformTelemetrySchemaRegistry,
        ESPMode::ThreadSafe> Registry =
        MakeShared<
            FGamePlatformTelemetrySchemaRegistry,
            ESPMode::ThreadSafe>();

    for (const TCHAR* Name : {
             TEXT("Telemetry.Foundation.ClientStarted"),
             TEXT("Telemetry.Foundation.ServerStarted"),
             TEXT("Telemetry.Foundation.WorldLoaded"),
             TEXT("Telemetry.Foundation.BackendRoundTrip"),
             TEXT("Session.Connect.Succeeded"),
             TEXT("World.Load.Completed"),
             TEXT("World.Travel.Completed"),
             TEXT("Combat.Match.Completed"),
             TEXT("Commerce.Intent.Created"),
             TEXT("Commerce.Payment.VerificationResult"),
             TEXT("Commerce.Fulfillment.Result")})
    {
        Registry->RegisterEvent(FoundationEvent(Name));
    }

    Registry->RegisterMetric(
        Metric(
            TEXT("server.frame_ms"),
            EGamePlatformTelemetryMetricType::Histogram,
            TEXT("ms")));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.active_players"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("players")));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.active_ai"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("actors")));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.memory_bytes"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("bytes")));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.telemetry.buffer_depth"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("records")));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.telemetry.dropped_total"),
            EGamePlatformTelemetryMetricType::Counter,
            TEXT("records")));
    Registry->RegisterMetric(
        Metric(
            TEXT("client.frame_ms"),
            EGamePlatformTelemetryMetricType::Histogram,
            TEXT("ms")));
    Registry->RegisterMetric(
        Metric(
            TEXT("client.loading.duration_ms"),
            EGamePlatformTelemetryMetricType::Duration,
            TEXT("ms")));
    Registry->RegisterMetric(
        Metric(
            TEXT("client.telemetry.buffer_depth"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("records")));
    Registry->RegisterMetric(
        Metric(
            TEXT("client.telemetry.dropped_total"),
            EGamePlatformTelemetryMetricType::Counter,
            TEXT("records")));
    Registry->RegisterMetric(
        Metric(
            TEXT("backend.request.duration_ms"),
            EGamePlatformTelemetryMetricType::Duration,
            TEXT("ms")));
    Registry->RegisterMetric(
        Metric(
            TEXT("backend.request.error_total"),
            EGamePlatformTelemetryMetricType::Counter,
            TEXT("requests")));
    Registry->RegisterMetric(
        Metric(
            TEXT("Telemetry.Foundation.TestMetric"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("value")));

    return Registry;
}
