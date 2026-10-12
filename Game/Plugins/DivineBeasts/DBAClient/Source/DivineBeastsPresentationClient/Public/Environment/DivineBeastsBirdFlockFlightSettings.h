// 神兽联盟项目层正式鸟群飞行配置；保留已有三种模式值0/1/2与鸟种资产身份。
// 该类型只表达无权威的客户端环境表现意图；路径使用弱组件引用，不延长旧世界生命周期。
#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsBirdFlockFlightSettings.generated.h"

class USplineComponent;
class UWorld;

/** 演示与生态路径统一模式；原有Niagara三种模式0..2序列值不允许重排。 */
UENUM(BlueprintType)
enum class EDivineBeastsBirdFlockFlightMode : uint8
{
    Orbit = 0 UMETA(DisplayName="盘旋"),
    Traverse = 1 UMETA(DisplayName="掠过"),
    FigureEight = 2 UMETA(DisplayName="八字"),
    SplineOneWay = 3 UMETA(DisplayName="样条单程"),
    SplineLoop = 4 UMETA(DisplayName="样条循环"),
    SplinePingPong = 5 UMETA(DisplayName="样条往返")
};

/**
 * FDivineBeastsBirdFlockFlightSettings（鸟群可选高级飞行配置）。
 * UObject场景拥有路线引用，服务仅持弱引用；复制整个快照前先完成全部范围和世界校验。
 * 旧调用方不提交此配置，不向Definition多送未经注册的User字段。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONCLIENT_API FDivineBeastsBirdFlockFlightSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Route")
    EDivineBeastsBirdFlockFlightMode FlightMode = EDivineBeastsBirdFlockFlightMode::Orbit;

    /** 路径模式必须提供本世界已注册的Spline；非路径模式不依赖SplineActor。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Route")
    bool bUseSpline = false;

    /** GPU局部群聚仅开启后才发送参数，数值范围0..4且三项之和不能超过4。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Boids")
    bool bEnableFlocking = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Boids", meta=(ClampMin="0.0",ClampMax="4.0"))
    float SeparationWeight = 1.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Boids", meta=(ClampMin="0.0",ClampMax="4.0"))
    float AlignmentWeight = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Boids", meta=(ClampMin="0.0",ClampMax="4.0"))
    float CohesionWeight = 0.6f;

    /** 扑翼转滑翔混合量0..1；默认0继续沿用现有物种材质动画。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Animation", meta=(ClampMin="0.0",ClampMax="1.0"))
    float GlideBlend = 0.0f;

    /** 画质请求比例0..1；不能代替平台EffectType真实GPU预算。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Performance", meta=(ClampMin="0.0",ClampMax="1.0"))
    float QualityFraction = 1.0f;

    /** 稳定相位种子；不得进入每帧随机生成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Behavior", meta=(ClampMin="0",ClampMax="1000000"))
    int32 Seed = 137;

    /** 路线源只读弱引用，地图卸载或Actor被销毁后不能沿用旧指针。 */
    TWeakObjectPtr<USplineComponent> Spline;

    /** 游戏线程合法性检查；错误返回明确中文原因，且不触发任何VFX副作用。 */
    bool ValidateForWorld(const UWorld* World, FString& OutReason) const;

    /** 请求缓存去重：参数或世界Spline身份改变时必须重新提交真实表现请求。 */
    bool EquivalentTo(const FDivineBeastsBirdFlockFlightSettings& Other) const;
};
