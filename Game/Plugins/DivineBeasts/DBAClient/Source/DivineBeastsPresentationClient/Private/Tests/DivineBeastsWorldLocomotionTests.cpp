// 项目客户端移动资源回归：真实公共Mesh+保存的世界ABP读取真实CharacterMovement速度并改变腿骨姿势。
// 本用例不创建PIE、不加载正式世界或网络，不改出生点；瞬态世界离开作用域即销毁。
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldLocomotionTest,
    "DivineBeasts.Presentation.Characters.WorldLocomotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldLocomotionTest::RunTest(const FString&)
{
    UClass* AnimationClass = LoadClass<UAnimInstance>(nullptr,
        TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.ABP_DBA_WorldLocomotion_C"));
    if (!TestNotNull(TEXT("已编译保存的世界动画蓝图"), AnimationClass)) return false;
    // 资产蓝图向下继承平台类型；用反射检查真实继承属性，不为只读测试引入项目到客户端实现的额外链接。
    const FFloatProperty* Speed = FindFProperty<FFloatProperty>(AnimationClass, TEXT("GroundSpeedCmPerSecond"));
    const FBoolProperty* Falling = FindFProperty<FBoolProperty>(AnimationClass, TEXT("bIsFalling"));
    if (!TestNotNull(TEXT("平台水平速度快照"), Speed) || !TestNotNull(TEXT("平台下落快照"), Falling)) return false;
    const UWorld::InitializationValues Settings = UWorld::InitializationValues()
        .CreatePhysicsScene(false).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("瞬态资源测试世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    for (const TCHAR* Name : { TEXT("Manny"), TEXT("Quinn") })
    {
        const FString MeshPath = FString::Printf(TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_%s_Simple.SKM_%s_Simple"), Name, Name);
        USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
        if (!TestNotNull(TEXT("实际男女蒙皮网格"), Asset)) return false;
        // 两个模型只做独立姿势评估，不做碰撞验收；测试世界一次清理全部Actor。
        // 避免逐Actor销毁访问不存在的游戏WorldContext，产生与动画无关的测试警告。
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACharacter* Character = World->SpawnActor<ACharacter>(SpawnParameters);
        if (!TestNotNull(TEXT("真实移动所有者"), Character)) return false;
        USkeletalMeshComponent* Mesh = Character->GetMesh();
        Mesh->SetSkeletalMesh(Asset);
        Mesh->SetAnimInstanceClass(AnimationClass);
        UAnimInstance* Instance = Mesh->GetAnimInstance();
        if (!TestNotNull(TEXT("动画类已实际装配"), Instance)) return false;
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        Movement->SetMovementMode(MOVE_Walking);
        const int32 ThighIndex = Mesh->GetBoneIndex(TEXT("thigh_l"));
        if (!TestTrue(TEXT("测试网格拥有实际腿骨"), ThighIndex != INDEX_NONE)) return false;
        for (const float SpeedCmPerSecond : { 150.0f, 500.0f })
        {
            Movement->Velocity = FVector(SpeedCmPerSecond, 0.0, 0.0);
            // 显式评估独立测试Mesh，避免同一游戏帧的ShouldTickPose过滤使普通Tick截图保持首帧。
            Mesh->TickAnimation(1.0f / 30.0f, false);
            Mesh->RefreshBoneTransforms();
            const FQuat FirstPose = Mesh->GetBoneTransform(ThighIndex).GetRotation();
            Mesh->TickAnimation(0.375f, false);
            Mesh->RefreshBoneTransforms();
            const FQuat SecondPose = Mesh->GetBoneTransform(ThighIndex).GetRotation();
            TestEqual(TEXT("实际速度驱动保存ABP的混合输入"), Speed->GetPropertyValue_InContainer(Instance), SpeedCmPerSecond);
            TestFalse(TEXT("地面姿势不进入下落分支"), Falling->GetPropertyValue_InContainer(Instance));
            TestTrue(TEXT("Walk/Run腿骨随评估时间运动"), FirstPose.AngularDistance(SecondPose) > 0.01f);
        }
        Movement->Velocity = FVector::ZeroVector;
        Mesh->TickAnimation(1.0f / 30.0f, false);
        Mesh->RefreshBoneTransforms();
        TestEqual(TEXT("停止后的实际ABP回到零速度Idle"), Speed->GetPropertyValue_InContainer(Instance), 0.0f);
        Movement->SetMovementMode(MOVE_Falling);
        Mesh->TickAnimation(1.0f / 30.0f, false);
        Mesh->RefreshBoneTransforms();
        TestTrue(TEXT("实际ABP获得下落状态"), Falling->GetPropertyValue_InContainer(Instance));
    }
    return true;
}
#endif
