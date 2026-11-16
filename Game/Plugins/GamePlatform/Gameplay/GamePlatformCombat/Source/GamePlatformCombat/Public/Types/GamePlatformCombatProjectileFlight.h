// 服务器通用飞行配置；发射冻结，单位为cm、s。不包含项目身份或客户端资源。
#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "GamePlatformCombatProjectileFlight.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformProjectileSpeedMode : uint8
{
    ConstantSpeed, ConstantAcceleration, AccelerateThenCruise, Decelerate, SpeedCurve
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatProjectileFlightConfig
{
    GENERATED_BODY()
    /** false完全沿用既有SpeedCmPerSecond；配置不自动覆盖旧资产。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") EGamePlatformProjectileSpeedMode SpeedMode = EGamePlatformProjectileSpeedMode::ConstantSpeed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm/s")) float InitialSpeed = 1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm/s")) float MaxSpeed = 1800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm/s")) float MinSpeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm/s^2")) float Acceleration = 4000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm/s^2")) float Deceleration = 4000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="s")) float AccelerationDuration = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="s")) float DecelerationDuration = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="s")) float CurveDuration = 1;
    /** 横轴归一化时间0..1，纵轴MinSpeed..MaxSpeed之间的归一化系数；独立于恒推进策略。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") TObjectPtr<UCurveFloat> SpeedCurve;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") float GravityScale = 0;
    /** 默认保留已选目标的服务器追踪；无目标不自动寻敌。重力与追踪不得同时配置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnableHoming = true;
    /** 曲线追踪转向能力，cm/s²；0保留原Bezier转向。小于30度直追规则不受此值影响。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") float HomingAcceleration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bRotationFollowsVelocity = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnableSubStepping = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="cm")) float MaxFlightDistance = 100000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(Units="s")) float MaxFlightTime = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnableSpeedBasedVFX = true;

    /** 纯验证，不修改、加载或修正配置；可选错误返回首个非法类别。 */
    bool IsValid(FString* OutError = nullptr) const;
    /** 按发射后秒数求推进速度；调用前验证配置。 */
    float SpeedAt(float Seconds) const;
    /** 推进距离积分；恒定策略解析求积，曲线使用有界时间积分。 */
    float DistanceBetween(float StartSeconds, float EndSeconds) const;
};

/** 可被Definition、蓝图默认值或服务器运行时覆盖复用的唯一参数模板。 */
UCLASS(BlueprintType)
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatProjectileFlightProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flight") FGamePlatformCombatProjectileFlightConfig Flight;
};

/** 只在一次发射/复用时复制，曲线为共享权威资产引用；不携带任何表现资源。 */
USTRUCT()
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatProjectileFlightInitialization
{
    GENERATED_BODY()
    UPROPERTY() FGuid OccurrenceId;
    UPROPERTY() FGamePlatformCombatProjectileFlightConfig Config;
    UPROPERTY() double ServerStartTime = 0;
    UPROPERTY() FVector LaunchDirection = FVector::ForwardVector;
    /** 存在重力/螺旋分量时，客户端读取既有ReplicatedMovement总速度，不只猜推进速度。 */
    UPROPERTY() bool bHasSecondaryVelocity = false;
};
