#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

/**
 * FGamePlatformTelemetryAttributeDefinition（遥测属性结构定义）。
 * PrivacyClass/Type由Schema所有，事件生产者只能提交值，不能自行把敏感字段降级为Operational。
 */
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryAttributeDefinition
{
    EGamePlatformTelemetryAttributeType Type = EGamePlatformTelemetryAttributeType::String;
    EGamePlatformTelemetryPrivacyClass PrivacyClass = EGamePlatformTelemetryPrivacyClass::Operational;
    bool bRequired = false;
    int32 MaxStringLength = 512;
};

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryEventDefinition
{
    FName EventName = NAME_None;
    int32 SchemaVersion = 1;
    /** 优先级属于Schema，不允许调用方把高频普通事件伪装成Critical后驱逐真正关键记录。 */
    EGamePlatformTelemetryPriority Priority = EGamePlatformTelemetryPriority::Normal;
    EGamePlatformTelemetrySamplingPolicy SamplingPolicy =
        EGamePlatformTelemetrySamplingPolicy::Always;
    double SamplingRate = 1.0;
    int32 SustainedRatePerSecond = 10;
    int32 Burst = 20;
    TMap<FName, FGamePlatformTelemetryAttributeDefinition> Attributes;
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
    /** 指标独立速率上限；frame_ms等高频生产者不会按渲染帧率无限挤占Buffer。 */
    int32 SustainedRatePerSecond = 10;
    int32 Burst = 20;
    FString Owner;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetrySchemaRegistry
{
public:
    void RegisterEvent(
        FGamePlatformTelemetryEventDefinition Definition);

    void RegisterMetric(
        FGamePlatformTelemetryMetricDefinition Definition);

    /**
     * 冻结Schema注册表。冻结后运行期只读，后续注册请求会被拒绝，
     * 防止热路径Find与运行时结构修改并发，也防止同一进程中途改变隐私/优先级规则。
     */
    void Freeze();
    bool IsFrozen() const { return bFrozen; }

    const FGamePlatformTelemetryEventDefinition* FindEvent(
        FName EventName) const;

    const FGamePlatformTelemetryMetricDefinition* FindMetric(
        FName MetricName) const;

    static TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
        CreateFoundationDefaults();

private:
    bool bFrozen = false;
    TMap<FName, FGamePlatformTelemetryEventDefinition> Events;
    TMap<FName, FGamePlatformTelemetryMetricDefinition> Metrics;
};
