// 平台客户端动画快照实现：必要动画评估读取Actor本地/复制速度，无输入意图或业务状态轮询。
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UGamePlatformLocomotionAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    GroundSpeedCmPerSecond = 0.0f;
    VerticalSpeedCmPerSecond = 0.0f;
    bIsFalling = false;
    bIsGrounded = false;
    bIsRising = false;
}

void UGamePlatformLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    // 不缓存所有者；Mesh重绑定或Travel后本次评估必须读取新Pawn，空对象回到明确Idle状态。
    const APawn* Pawn = TryGetPawnOwner();
    const FVector Velocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
    const double GroundSpeed = Velocity.Size2D();
    GroundSpeedCmPerSecond = FMath::IsFinite(GroundSpeed) ? static_cast<float>(GroundSpeed) : 0.0f;
    // 先转换为蓝图字段精度再检查，拒绝NaN/Infinity及超出float范围的Z；水平与垂直快照互不污染。
    // IsFalling只说明离地，正负Z补充上升/下降方向，顶点与落地仍由实际移动模式区分。
    const float VerticalSpeed = static_cast<float>(Velocity.Z);
    VerticalSpeedCmPerSecond = FMath::IsFinite(VerticalSpeed) ? VerticalSpeed : 0.0f;
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    bIsFalling = Movement && Movement->IsFalling();
    // 显式快照让简单布尔转换规则无需组合/取反；真实移动模式确认地面，缺Owner或其他移动模式不能误播Landing。
    bIsGrounded = Movement && Movement->IsMovingOnGround();
    bIsRising = bIsFalling && VerticalSpeedCmPerSecond > 0.0f;
}

void UGamePlatformLocomotionAnimInstance::NativeUninitializeAnimation()
{
    GroundSpeedCmPerSecond = 0.0f;
    VerticalSpeedCmPerSecond = 0.0f;
    bIsFalling = false;
    bIsGrounded = false;
    bIsRising = false;
    Super::NativeUninitializeAnimation();
}
