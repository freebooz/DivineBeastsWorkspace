// 平台层遥测Schema私有注册与验证实现，GI子系统拥有；只验证事件结构/隐私边界，不拥有业务权威状态。
// 游戏线程按既有服务合同注册/查询；失败返回诊断，注册表生命周期随GI释放，稳定值类型仍可公开扩展。
// 同名旧Public头保留不透明兼容；首先包含它，再引入Private定义，避免违反UE第一包含规则。
#include "Schema/GamePlatformTelemetrySchemaRegistry.h"
#include "Schema/TelemetrySchemaRegistry.h"

namespace
{
FGamePlatformTelemetryAttributeDefinition Attribute(
    EGamePlatformTelemetryAttributeType Type,
    EGamePlatformTelemetryPrivacyClass Privacy = EGamePlatformTelemetryPrivacyClass::Operational,
    bool bRequired = false,
    int32 MaxStringLength = 512)
{
    FGamePlatformTelemetryAttributeDefinition Definition;
    Definition.Type = Type;
    Definition.PrivacyClass = Privacy;
    Definition.bRequired = bRequired;
    Definition.MaxStringLength = MaxStringLength;
    return Definition;
}

FGamePlatformTelemetryEventDefinition FoundationEvent(
    const TCHAR* Name,
    EGamePlatformTelemetryPriority Priority = EGamePlatformTelemetryPriority::Normal,
    EGamePlatformTelemetrySamplingPolicy Sampling = EGamePlatformTelemetrySamplingPolicy::Always,
    double Rate = 1.0)
{
    FGamePlatformTelemetryEventDefinition Definition;
    Definition.EventName = FName(Name);
    Definition.SchemaVersion = 1;
    Definition.Priority = Priority;
    Definition.SamplingPolicy = Sampling;
    Definition.SamplingRate = Rate;
    Definition.SustainedRatePerSecond = 10;
    Definition.Burst = 20;

    // 基础事件只允许稳定结果/错误/时延三类通用字段；商业字段不得泄漏到Session/World等无关事件。
    Definition.Attributes.Add(TEXT("result"), Attribute(EGamePlatformTelemetryAttributeType::String));
    Definition.Attributes.Add(TEXT("error_code"), Attribute(EGamePlatformTelemetryAttributeType::String));
    Definition.Attributes.Add(TEXT("duration_ms"), Attribute(EGamePlatformTelemetryAttributeType::Double));
    return Definition;
}

FGamePlatformTelemetryEventDefinition CommerceEvent(
    const TCHAR* Name,
    EGamePlatformTelemetryPriority Priority = EGamePlatformTelemetryPriority::Normal)
{
    FGamePlatformTelemetryEventDefinition Definition = FoundationEvent(Name, Priority);
    Definition.Attributes.Add(TEXT("product_id"), Attribute(EGamePlatformTelemetryAttributeType::String));
    Definition.Attributes.Add(TEXT("offer_id"), Attribute(EGamePlatformTelemetryAttributeType::String));
    Definition.Attributes.Add(TEXT("provider_type"), Attribute(EGamePlatformTelemetryAttributeType::String));
    return Definition;
}

FGamePlatformTelemetryMetricDefinition Metric(
    const TCHAR* Name,
    EGamePlatformTelemetryMetricType Type,
    const TCHAR* Unit,
    int32 SustainedRatePerSecond = 10,
    int32 Burst = 20)
{
    FGamePlatformTelemetryMetricDefinition Definition;
    Definition.Name = FName(Name);
    Definition.Type = Type;
    Definition.Unit = Unit;
    Definition.Owner = TEXT("GamePlatform");
    Definition.SustainedRatePerSecond = FMath::Max(1, SustainedRatePerSecond);
    Definition.Burst = FMath::Max(1, Burst);

    for (const FName Label : {
             FName(TEXT("build_version")),
             FName(TEXT("environment")),
             FName(TEXT("platform")),
             FName(TEXT("server_role")),
             FName(TEXT("region")),
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
    if (bFrozen)
    {
        return;
    }
    if (!Definition.EventName.IsNone() &&
        Definition.SchemaVersion > 0)
    {
        Events.Add(Definition.EventName, MoveTemp(Definition));
    }
}

void FGamePlatformTelemetrySchemaRegistry::RegisterMetric(
    FGamePlatformTelemetryMetricDefinition Definition)
{
    if (bFrozen)
    {
        return;
    }
    if (!Definition.Name.IsNone())
    {
        Metrics.Add(Definition.Name, MoveTemp(Definition));
    }
}

void FGamePlatformTelemetrySchemaRegistry::Freeze()
{
    bFrozen = true;
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

    // Telemetry自身只拥有运行时基础事件；其它领域事件当前保留兼容注册，后续由各领域Schema Contributor接管。
    Registry->RegisterEvent(FoundationEvent(TEXT("Telemetry.Foundation.ClientStarted"), EGamePlatformTelemetryPriority::CriticalTelemetry));
    Registry->RegisterEvent(FoundationEvent(TEXT("Telemetry.Foundation.ServerStarted"), EGamePlatformTelemetryPriority::CriticalTelemetry));
    Registry->RegisterEvent(FoundationEvent(TEXT("Telemetry.Foundation.WorldLoaded")));
    Registry->RegisterEvent(FoundationEvent(TEXT("Telemetry.Foundation.BackendRoundTrip")));
    Registry->RegisterEvent(FoundationEvent(TEXT("Session.Connect.Succeeded")));
    Registry->RegisterEvent(FoundationEvent(TEXT("World.Load.Completed")));
    Registry->RegisterEvent(FoundationEvent(TEXT("World.Travel.Completed")));
    Registry->RegisterEvent(FoundationEvent(TEXT("Combat.Match.Completed")));
    Registry->RegisterEvent(CommerceEvent(TEXT("Commerce.Intent.Created")));
    Registry->RegisterEvent(CommerceEvent(TEXT("Commerce.Payment.VerificationResult"), EGamePlatformTelemetryPriority::CriticalTelemetry));
    Registry->RegisterEvent(CommerceEvent(TEXT("Commerce.Fulfillment.Result"), EGamePlatformTelemetryPriority::CriticalTelemetry));

    Registry->RegisterMetric(
        Metric(
            TEXT("server.frame_ms"),
            EGamePlatformTelemetryMetricType::Histogram,
            TEXT("ms"),
            10,
            20));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.active_players"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("players"),
            2,
            4));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.active_ai"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("actors"),
            2,
            4));
    Registry->RegisterMetric(
        Metric(
            TEXT("server.memory_bytes"),
            EGamePlatformTelemetryMetricType::Gauge,
            TEXT("bytes"),
            1,
            2));
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
            TEXT("ms"),
            10,
            20));
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
