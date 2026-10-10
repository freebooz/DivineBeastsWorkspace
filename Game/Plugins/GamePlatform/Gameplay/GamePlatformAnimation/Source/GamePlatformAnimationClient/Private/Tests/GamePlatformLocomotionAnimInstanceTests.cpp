// 平台客户端移动动画回归：真实Character/Pawn速度驱动动画，不依赖项目、账号、网络服务或内容资产。
// 测试世界与角色只存在于本用例，结束销毁；断言覆盖停下、移动、垂直速度、空中、失活清理。
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

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
    TestTrue(TEXT("真实行走模式公开已落地事实"), Animation->bIsGrounded);
    TestFalse(TEXT("地面正Z速度不能伪造起跳"), Animation->bIsRising);
    Movement->SetMovementMode(MOVE_Falling);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestTrue(TEXT("CharacterMovement空中事实直接驱动动画"), Animation->bIsFalling);
    TestFalse(TEXT("空中模式不能同时声明已落地"), Animation->bIsGrounded);
    TestTrue(TEXT("离地且正Z直接驱动上升转换"), Animation->bIsRising);
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
    TestEqual(TEXT("动画失活清理旧垂直速度"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("动画失活清理旧空中事实"), Animation->bIsFalling);
    TestFalse(TEXT("动画失活清理旧落地事实"), Animation->bIsGrounded);
    TestFalse(TEXT("动画失活清理旧上升事实"), Animation->bIsRising);
    // UAnimInstance引擎合同要求Outer为Mesh；无Pawn预览仍使用真实Mesh，不构造无效Outer替身。
    USkeletalMeshComponent* PreviewMesh = NewObject<USkeletalMeshComponent>();
    UGamePlatformLocomotionAnimInstance* Ownerless = NewObject<UGamePlatformLocomotionAnimInstance>(PreviewMesh);
    // 预置上一所有者的快照，防止无Pawn测试只验证字段默认值而遗漏实际清理路径。
    Ownerless->GroundSpeedCmPerSecond = 500.0f;
    Ownerless->VerticalSpeedCmPerSecond = 420.0f;
    Ownerless->bIsFalling = true;
    Ownerless->bIsGrounded = true;
    Ownerless->bIsRising = true;
    Ownerless->NativeUpdateAnimation(0.0f);
    TestEqual(TEXT("无Pawn的资产预览使用静止速度"), Ownerless->GroundSpeedCmPerSecond, 0.0f);
    TestEqual(TEXT("无Pawn的资产预览清除旧垂直速度"), Ownerless->VerticalSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("无Pawn时不伪造空中姿势"), Ownerless->bIsFalling);
    TestFalse(TEXT("无Pawn时不能由非下落反推落地"), Ownerless->bIsGrounded);
    TestFalse(TEXT("无Pawn清除上一角色上升事实"), Ownerless->bIsRising);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformLocomotionAirborneVerticalSpeedTest,
    "GamePlatform.Animation.Locomotion.AirborneVerticalSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 验证真实移动组件的上升、顶点、下落与落地快照；错误符号或输入推断会选错跳跃动画段。 */
bool FGamePlatformLocomotionAirborneVerticalSpeedTest::RunTest(const FString&)
{
    const UWorld::InitializationValues WorldSettings = UWorld::InitializationValues()
        .CreatePhysicsScene(false).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldSettings);
    if (!TestNotNull(TEXT("空中快照测试世界"), World)) return false;
    ACharacter* Character = World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("空中快照真实Character"), Character))
    {
        World->DestroyWorld(false);
        return false;
    }
    UGamePlatformLocomotionAnimInstance* Animation = NewObject<UGamePlatformLocomotionAnimInstance>(Character->GetMesh());
    // 初始化不能继承上一个Mesh/Pawn的上升方向；不保留计时器或项目动画状态。
    Animation->GroundSpeedCmPerSecond = 150.0f;
    Animation->VerticalSpeedCmPerSecond = 420.0f;
    Animation->bIsFalling = true;
    Animation->bIsGrounded = true;
    Animation->bIsRising = true;
    Animation->NativeInitializeAnimation();
    TestEqual(TEXT("初始化清理水平速度"), Animation->GroundSpeedCmPerSecond, 0.0f);
    TestEqual(TEXT("初始化清理垂直速度"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("初始化清理空中事实"), Animation->bIsFalling);
    TestFalse(TEXT("初始化不能继承旧角色落地条件"), Animation->bIsGrounded);
    TestFalse(TEXT("初始化不能继承旧角色上升条件"), Animation->bIsRising);

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(90.0, 120.0, 420.0);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestTrue(TEXT("起跳上升仍属于CharacterMovement空中模式"), Animation->bIsFalling);
    TestTrue(TEXT("起跳直接公开状态机上升条件"), Animation->bIsRising);
    TestFalse(TEXT("上升时没有落地条件"), Animation->bIsGrounded);
    TestEqual(TEXT("起跳快照保留向上的420厘米每秒"), Animation->VerticalSpeedCmPerSecond, 420.0f);
    TestEqual(TEXT("空中水平速度仍独立为150厘米每秒"), Animation->GroundSpeedCmPerSecond, 150.0f);

    Movement->Velocity.Z = 0.0;
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestTrue(TEXT("跳跃顶点未落地，不能仅由零垂直速度决定Idle"), Animation->bIsFalling);
    TestFalse(TEXT("顶点零Z结束上升条件"), Animation->bIsRising);
    TestFalse(TEXT("顶点不能误触发Landing"), Animation->bIsGrounded);
    TestEqual(TEXT("顶点垂直速度为零"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    Movement->Velocity.Z = -260.0;
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestTrue(TEXT("下降段保持真实空中模式"), Animation->bIsFalling);
    TestFalse(TEXT("负Z下降不满足上升条件"), Animation->bIsRising);
    TestEqual(TEXT("下降快照保留负的260厘米每秒"), Animation->VerticalSpeedCmPerSecond, -260.0f);

    // 非有限Z不能污染动画比较或混合权重，也不能清除移动组件仍在空中的事实。
    Movement->Velocity.Z = std::numeric_limits<double>::quiet_NaN();
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("非法NaN垂直速度归零"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    TestEqual(TEXT("非法Z不污染独立水平速度"), Animation->GroundSpeedCmPerSecond, 150.0f);
    TestTrue(TEXT("非法速度不伪造落地"), Animation->bIsFalling);
    TestFalse(TEXT("非法Z不能触发上升"), Animation->bIsRising);
    TestFalse(TEXT("非法Z不能触发落地"), Animation->bIsGrounded);
    Movement->Velocity.Z = std::numeric_limits<double>::infinity();
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestEqual(TEXT("无限垂直速度归零"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("无限Z不能触发上升"), Animation->bIsRising);

    // 非下落不等于踩在地面；飞行/游泳预览不能误触发Landing或沿用先前跳跃上升。
    Movement->SetMovementMode(MOVE_Flying);
    Movement->Velocity.Z = 420.0;
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestFalse(TEXT("飞行不是角色下落模式"), Animation->bIsFalling);
    TestFalse(TEXT("飞行不能由取反下落伪造落地"), Animation->bIsGrounded);
    TestFalse(TEXT("飞行正Z不触发地面跳跃上升段"), Animation->bIsRising);
    Movement->SetMovementMode(MOVE_Swimming);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestFalse(TEXT("游泳不能误触发落地段"), Animation->bIsGrounded);

    Movement->Velocity = FVector::ZeroVector;
    Movement->SetMovementMode(MOVE_Walking);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    TestFalse(TEXT("落地由移动模式确认，供图触发非循环落地动画"), Animation->bIsFalling);
    TestTrue(TEXT("落地直接提供Landing布尔条件"), Animation->bIsGrounded);
    TestFalse(TEXT("落地不再处于起跳上升"), Animation->bIsRising);
    TestEqual(TEXT("落地采用最新零垂直速度"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    Movement->Velocity = FVector(0.0, 0.0, -260.0);
    Movement->SetMovementMode(MOVE_Falling);
    Animation->NativeUpdateAnimation(1.0f / 60.0f);
    Animation->NativeUninitializeAnimation();
    TestEqual(TEXT("空中失活清理下降速度"), Animation->VerticalSpeedCmPerSecond, 0.0f);
    TestFalse(TEXT("空中失活清理离地事实"), Animation->bIsFalling);
    TestFalse(TEXT("空中失活不能以非下落误报落地"), Animation->bIsGrounded);
    TestFalse(TEXT("空中失活清理上升事实"), Animation->bIsRising);
    World->DestroyWorld(false);
    return true;
}
#endif
