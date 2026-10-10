#pragma once
// 平台遥测内部冻结注册表：仅模块初始化与记录校验调用，注册完成后只读，不持有世界或用户。
#include "Types/GamePlatformTelemetrySchemaTypes.h"

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetrySchemaRegistry
{
public:
    /** 初始化线程注册有效名称/正版本事件；冻结后忽略修改，同名内建规则由初始化方负责唯一性。 */
    void RegisterEvent(
        FGamePlatformTelemetryEventDefinition Definition);

    /** 初始化线程注册中立指标；冻结后不修改，运行热路径不能插入新标签/隐私规则。 */
    void RegisterMetric(
        FGamePlatformTelemetryMetricDefinition Definition);

    /**
     * 冻结Schema注册表。冻结后运行期只读，后续注册请求会被拒绝，
     * 防止热路径Find与运行时结构修改并发，也防止同一进程中途改变隐私/优先级规则。
     */
    void Freeze();
    /** 查询是否已结束注册阶段；冻结是单向状态，不支持运行时解冻。 */
    bool IsFrozen() const { return bFrozen; }

    /** 只读借用事件规则；缺失返回nullptr，引用有效期为本注册表存活且不再修改期间。 */
    const FGamePlatformTelemetryEventDefinition* FindEvent(
        FName EventName) const;

    /** 只读借用指标规则；缺失必须拒绝记录，不能推算Schema。 */
    const FGamePlatformTelemetryMetricDefinition* FindMetric(
        FName MetricName) const;

    /** 创建平台默认规则，仅供子系统初始化后冻结；不按世界创建第二套业务Schema。 */
    static TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
        CreateFoundationDefaults();

private:
    bool bFrozen = false;
    TMap<FName, FGamePlatformTelemetryEventDefinition> Events;
    TMap<FName, FGamePlatformTelemetryMetricDefinition> Metrics;
};
