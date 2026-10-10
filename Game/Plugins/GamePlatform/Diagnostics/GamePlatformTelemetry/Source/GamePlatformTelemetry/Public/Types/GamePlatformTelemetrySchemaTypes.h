#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"
// 平台遥测稳定Schema值契约：由内建规则提供，只描述允许结构/隐私/预算，不拥有注册表、队列或网络请求。

/**
 * FGamePlatformTelemetryAttributeDefinition（遥测属性结构定义）。
 * PrivacyClass/Type由Schema所有，事件生产者只能提交值，不能自行把敏感字段降级为Operational。
 */
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryAttributeDefinition
{
    /** 必须与提交值类型一致；默认字符串，生产者不能按值自行改类型。 */
    EGamePlatformTelemetryAttributeType Type = EGamePlatformTelemetryAttributeType::String;
    /** 字段隐私分类由规则拥有；默认运行诊断，敏感字段须显式声明并服从过滤策略。 */
    EGamePlatformTelemetryPrivacyClass PrivacyClass = EGamePlatformTelemetryPrivacyClass::Operational;
    /** 为true时缺失该属性使整条事件结构校验失败；false表示可省略。 */
    bool bRequired = false;
    /** 字符串最大字符数，默认512；不是UTF8网络字节上限。 */
    int32 MaxStringLength = 512;
};

/** 中立事件规则值：名称/版本保持契约兼容，运行时规则冻结后只读。 */
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryEventDefinition
{
    /** 事件稳定名称；None无效，不能产生匿名生产记录。 */
    FName EventName = NAME_None;
    /** 大于0的结构版本，默认1；修改字段含义须独立评审消费者兼容。 */
    int32 SchemaVersion = 1;
    /** 优先级属于Schema，不允许调用方把高频普通事件伪装成Critical后驱逐真正关键记录。 */
    EGamePlatformTelemetryPriority Priority = EGamePlatformTelemetryPriority::Normal;
    /** 默认全采；选择比例采样时使用下面[0,1]的规则比例。 */
    EGamePlatformTelemetrySamplingPolicy SamplingPolicy =
        EGamePlatformTelemetrySamplingPolicy::Always;
    /** 概率比例[0,1]；1为全采，不能用于未经批准的性能阈值。 */
    double SamplingRate = 1.0;
    /** 持续每秒记录速率，默认10条；由规则在令牌桶中执行。 */
    int32 SustainedRatePerSecond = 10;
    /** 突发桶容量，默认20条；不会改变单批网络字节预算。 */
    int32 Burst = 20;
    /** 允许属性集合；未声明或类型/隐私不合规的提交属性不能绕过校验。 */
    TMap<FName, FGamePlatformTelemetryAttributeDefinition> Attributes;
};

/** 中立指标规则值：标签白名单与单位属于Schema，调用者只提交已定义指标值。 */
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryMetricDefinition
{
    /** 指标稳定名称；None表示未配置，不可发送。 */
    FName Name = NAME_None;
    /** 指标聚合语义，默认Counter；Duration等类型不得当作计数器合并。 */
    EGamePlatformTelemetryMetricType Type =
        EGamePlatformTelemetryMetricType::Counter;
    /** 明确单位如ms/bytes/count；必须与生产者值一致，空值不能猜测单位。 */
    FString Unit;
    /** 标签键白名单；空集合表示不接受额外标签，避免敏感数据/基数失控。 */
    TSet<FName> AllowedLabels;
    /** 默认全采，比例采样仅使用规则中的SamplingRate。 */
    EGamePlatformTelemetrySamplingPolicy SamplingPolicy =
        EGamePlatformTelemetrySamplingPolicy::Always;
    /** 比例[0,1]，默认1；不同用户/服务器采样种子由子系统上下文决定。 */
    double SamplingRate = 1.0;
    /** 指标独立速率上限；frame_ms等高频生产者不会按渲染帧率无限挤占Buffer。 */
    int32 SustainedRatePerSecond = 10;
    /** 突发桶允许记录数，默认20条。 */
    int32 Burst = 20;
    /** 规则所属模块/领域名称，用于人工追责；不包含用户身份或凭据。 */
    FString Owner;
};
