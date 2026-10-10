// 平台客户端移动动画回归：真实Character/Pawn速度驱动动画，不依赖项目、账号、网络服务或内容资产。
// 测试世界与角色只存在于本用例，结束销毁；断言覆盖停下、移动、垂直速度、空中、失活清理。
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformLocomotionAnimInstanceTest,
    "GamePlatform.Animation.Locomotion.PawnMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformLocomotionAnimInstanceTest::RunTest(const FString&)
{
    const UWorld::InitializationValues WorldSettings = UWorld::InitializationValues()
        .CreatePhysicsScene(false).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldSettings);
    if (!TestNotNull(TEXT("瞬态测试世界"), World)) return false;
    ACharacter* Character = World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("真实Character"), Character))
    {
        World->DestroyWorld(false);
        return false;
    }
    UGamePlatformLocomotionAnimInstance* Animation = NewObject<UGamePlatformLocomotionAnimInstance>(Character->GetMesh());
    Animation->NativeInitializeAnimation();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Walking);
    Movement->Velocity = FVector::ZeroVector;
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("站立速度为零"), Animation->GroundSpeedCmPerSecond, 0.0f);
    Movement->Velocity = FVector(90.0, 120.0, 900.0);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("实际水平速度150厘米每秒，垂直速度不污染步态"), Animation->GroundSpeedCmPerSecond, 150.0f);
    TestFalse(TEXT("地面移动不进入空中姿势"), Animation->bIsFalling);
    Movement->SetMovementMode(MOVE_Falling);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestTrue(TEXT("CharacterMovement空中事实直接驱动动画"), Animation->bIsFalling);
    Movement->Velocity = FVector::ZeroVector;
    Movement->SetMovementMode(MOVE_Walking);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("停止立即回到Idle采样，不能保持旧步态"), Animation->GroundSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("落地立即清除空中状态"), Animation->bIsFalling);
    Movement->Velocity = FVector(500.0, 0.0, 0.0);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("跑动采用真实速度而非输入意图"), Animation->GroundSpeedCmPerSecond, 500.0f);
    Animation->NativeUninitializeAnimation();
    TestEqual(TEXT("动画失活清理旧速度"), Animation->GroundSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("动画失活清理旧空中事实"), Animation->bIsFalling);
    // UAnimInstance引擎合同要求Outer为Mesh；无Pawn预览仍使用真实Mesh，不构造无效Outer替身。
    USkeletalMeshComponent* PreviewMesh = NewObject<USkeletalMeshComponent>();
    UGamePlatformLocomotionAnimInstance* Ownerless = NewObject<UGamePlatformLocomotionAnimInstance>(PreviewMesh);
    Ownerless->NativeUpdateAnimation(0.0f);
    TestEqual(TEXT("无Pawn的资产预览使用静止速度"), Ownerless->GroundSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("无Pawn时不伪造空中姿势"), Ownerless->bIsFalling);
    World->DestroyWorld(false);
    return true;
}
#endif
