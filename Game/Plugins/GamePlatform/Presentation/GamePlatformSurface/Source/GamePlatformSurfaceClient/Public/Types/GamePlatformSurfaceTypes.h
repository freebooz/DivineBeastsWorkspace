#pragma once

#include "CoreMinimal.h"
#include "GamePlatformSurfaceTypes.generated.h"

/** 客户端环境表面状态更新结果。 */
UENUM(BlueprintType)
enum class EGamePlatformSurfaceUpdateStatus : uint8
{
    Applied UMETA(DisplayName="已应用"),
    Unchanged UMETA(DisplayName="无变化"),
    InvalidWorld UMETA(DisplayName="世界不可用"),
    InvalidState UMETA(DisplayName="状态非法"),
    MaterialBindingUnavailable UMETA(DisplayName="材质参数绑定不可用"),
    ParameterContractMismatch UMETA(DisplayName="材质参数契约不匹配")
};

/**
 * 跨游戏通用的环境表面状态。
 *
 * 该结构只描述客户端视觉环境，不承载天气权威、碰撞、移动摩擦或玩法判定。
 * 调用方可由天气、世界事件或项目表现层产生这些值；Surface 插件只负责验证、缓存并推送到材质参数集合。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceEnvironmentState
{
    GENERATED_BODY()

    /** 全局湿润程度，0为干燥，1为完全湿润。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float GlobalWetness = 0.0f;

    /** 全局积雪覆盖倍率，0为关闭，1为完整使用各材质自身积雪规则。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float GlobalSnowAmount = 0.0f;

    /** 世界空间积雪参考高度，单位为厘米；具体过渡宽度由材质实例控制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(Units="cm"))
    float GlobalSnowHeightCm = 0.0f;

    /** 苔藓全局影响倍率，0为禁用，1为完整使用各材质自身苔藓规则。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float GlobalMossInfluence = 1.0f;

    /** 全局积水程度，0为无积水，1为完整使用材质自身积水遮罩。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float GlobalPuddleAmount = 0.0f;

    /** 降雨视觉强度，仅作为表面材质输入，不代表服务器天气权威事实。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float RainIntensity = 0.0f;

    /** 降雪视觉强度，仅作为表面材质输入，不代表服务器天气权威事实。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SnowIntensity = 0.0f;

    /** 环境温度，单位摄氏度；只供材质表现使用，默认20摄氏度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(Units="Celsius"))
    float TemperatureCelsius = 20.0f;

    /** 所有字段是否为有限数值；出现NaN/Inf时拒绝更新，避免污染全世界材质统一缓冲。 */
    bool IsFinite() const;

    /** 返回限制到平台安全范围后的副本，不修改调用方原始值。 */
    FGamePlatformSurfaceEnvironmentState GetClamped() const;

    /** 用于事件去重，避免无变化状态重复写入MPC。 */
    bool IsNearlyEqual(const FGamePlatformSurfaceEnvironmentState& Other, float Tolerance = KINDA_SMALL_NUMBER) const;
};

/** 一次表面状态更新的可诊断结果。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceUpdateResult
{
    GENERATED_BODY()

    /** 更新终态；MaterialBindingUnavailable/ParameterContractMismatch表示状态已缓存但GPU材质同步不完整。 */
    UPROPERTY(BlueprintReadOnly, Category="Surface")
    EGamePlatformSurfaceUpdateStatus Status = EGamePlatformSurfaceUpdateStatus::InvalidState;

    /** 当前世界表面状态修订号；仅在状态真正变化时递增。 */
    UPROPERTY(BlueprintReadOnly, Category="Surface")
    int32 Revision = 0;

    /** 本次成功写入材质参数集合的标量参数数量。 */
    UPROPERTY(BlueprintReadOnly, Category="Surface")
    int32 UpdatedParameterCount = 0;

    /** 本次因参数缺失等原因未写入的标量参数数量。 */
    UPROPERTY(BlueprintReadOnly, Category="Surface")
    int32 FailedParameterCount = 0;

    /** 状态是否已被系统接受；即使MPC暂时缺失，已接受状态也会保留以便后续重新绑定。 */
    bool IsStateAccepted() const;
};
