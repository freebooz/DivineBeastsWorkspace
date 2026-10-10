// 平台层客户端移动动画基类：只读当前Pawn运动事实，供项目动画蓝图向下继承。
// 不拥有权威移动、不保存账号状态、不加载项目资源；生命周期跟随所属SkeletalMesh动画实例。
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GamePlatformLocomotionAnimInstance.generated.h"

/** 由引擎动画评估周期读取本地或复制Pawn速度；蓝图只消费快照，不能改变服务器移动结果。 */
UCLASS(Blueprintable, BlueprintType, Transient)
class GAMEPLATFORMANIMATIONCLIENT_API UGamePlatformLocomotionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    /** 水平速度，厘米/秒；无Pawn、非有限速度或失活时为零。垂直速度不决定地面步态。 */
    UPROPERTY(BlueprintReadOnly, Transient, Category="Locomotion", meta=(Units="cm/s"))
    float GroundSpeedCmPerSecond = 0.0f;

    /**
     * 当前Pawn沿世界Z轴的实际速度，厘米/秒；正值上升、负值下降、零为无垂直速度。
     * 无Pawn、非有限速度或失活时为零。图须结合bIsFalling区分空中顶点与地面，不能仅由Z判断落地；
     * 只表达本地/复制移动事实，不依据输入推断起跳，不持有跳跃计时或改变权威移动。
     */
    UPROPERTY(BlueprintReadOnly, Transient, Category="Locomotion", meta=(Units="cm/s"))
    float VerticalSpeedCmPerSecond = 0.0f;

    /** CharacterMovement当前是否处于空中下落模式（包含跳跃上升）；无Character或失活为false，不推断权威跳跃规则。 */
    UPROPERTY(BlueprintReadOnly, Transient, Category="Locomotion")
    bool bIsFalling = false;

    /** Mesh切换或动画实例初始化后清理上一所有者的状态；由引擎游戏线程调用。 */
    virtual void NativeInitializeAnimation() override;

    /** 每次动画评估只读当前所有者速度/移动模式；DeltaSeconds以秒计，更新不累计业务状态。 */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    /** 离开世界或Mesh更换时清理状态；不持有Pawn指针、委托、计时器或资源租约。 */
    virtual void NativeUninitializeAnimation() override;
};
