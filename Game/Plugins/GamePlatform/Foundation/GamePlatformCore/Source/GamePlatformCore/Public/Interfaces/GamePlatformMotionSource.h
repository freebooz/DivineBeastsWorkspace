// 全端中立的只读运动样本；不包含战斗、Niagara、伤害或位置写入接口。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformMotionSource.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformMotionSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) bool bEnableSpeedBasedVFX = false;
    /** 当前总速度与正的速度上限，cm/s。 */
    UPROPERTY(BlueprintReadOnly) float Speed = 0;
    UPROPERTY(BlueprintReadOnly) float MaxSpeed = 1;
    /** 推进速度的变化率，cm/s²；负值表示减速，重力转向不作为推进加速。 */
    UPROPERTY(BlueprintReadOnly) float Acceleration = 0;
    UPROPERTY(BlueprintReadOnly) float AccelerationNormalized = 0;
    UPROPERTY(BlueprintReadOnly) float SpeedNormalized = 0;
    UPROPERTY(BlueprintReadOnly) FVector FlightDirection = FVector::ForwardVector;
    /** 0停止、1加速、2巡航、3减速。 */
    UPROPERTY(BlueprintReadOnly) int32 FlightPhase = 0;
    UPROPERTY(BlueprintReadOnly) float TrailLengthScale = 1;
    UPROPERTY(BlueprintReadOnly) float ElapsedSeconds = 0;
};

UINTERFACE(BlueprintType)
class GAMEPLATFORMCORE_API UGamePlatformMotionSource : public UInterface { GENERATED_BODY() };

/** 游戏线程按需读取；实现者保有权威/复制来源，调用者不得反向控制运动。 */
class GAMEPLATFORMCORE_API IGamePlatformMotionSource
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Motion")
    FGamePlatformMotionSnapshot GetMotionSnapshot() const;
};
