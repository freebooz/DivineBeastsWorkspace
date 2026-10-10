// 平台客户端动画快照实现：必要动画评估读取Actor本地/复制速度，无输入意图或业务状态轮询。
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UGamePlatformLocomotionAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    GroundSpeedCmPerSecond = 0.0f;
    bIsFalling = false;
}

void UGamePlatformLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    // 不缓存所有者；Mesh重绑定或Travel后本次评估必须读取新Pawn，空对象回到明确Idle状态。
    const APawn* Pawn = TryGetPawnOwner();
    const FVector Velocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
    const double GroundSpeed = Velocity.Size2D();
    GroundSpeedCmPerSecond = FMath::IsFinite(GroundSpeed) ? static_cast<float>(GroundSpeed) : 0.0f;
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    bIsFalling = Movement && Movement->IsFalling();
}

void UGamePlatformLocomotionAnimInstance::NativeUninitializeAnimation()
{
    GroundSpeedCmPerSecond = 0.0f;
    bIsFalling = false;
    Super::NativeUninitializeAnimation();
}
