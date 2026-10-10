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
#include "Characters/DivineBeastsCharacterAppearanceComponent.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/CapsuleComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
// 回归真实异步外观完成后的偏移，模拟Definition晚于外观把胶囊从88cm调整到96cm。
// 不连接后端、不伪造Ready，不改正式资产；直接消费已加载资源，只验证客户端表现和引擎缓存合同。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsNetworkMeshPlacementTest,
    "DivineBeasts.Presentation.Characters.NetworkMeshPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsNetworkMeshPlacementTest::RunTest(const FString&)
{
    const auto Settings = UWorld::InitializationValues().CreatePhysicsScene(false)
        .ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    if (!TestNotNull(TEXT("独立外观回归世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ACharacter* Character = World->SpawnActor<ACharacter>();
    auto* Appearance = NewObject<UDivineBeastsCharacterAppearanceComponent>(Character);
    // 夹具组件未注册，不依赖EndPlay；快速切换分支发起的真实租约必须先取消，再销毁测试世界。
    ON_SCOPE_EXIT { Appearance->CancelPendingLoads(); };
    auto* Identity = NewObject<UDivineBeastsCharacterComponent>(Character);
    auto* Profile = NewObject<UDivineBeastsCharacterAppearanceProfile>(Appearance);
    Profile->HeroDefinitionId = TEXT("Hero.Zodiac.Horse");
    Profile->bDevelopmentPlaceholder = true;
    Profile->SkeletalMesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    Profile->AnimInstanceClass = LoadClass<UAnimInstance>(nullptr,
        TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.ABP_DBA_WorldLocomotion_C"));
    if (!TestNotNull(TEXT("真实Mesh"), Profile->SkeletalMesh.Get()) || !TestNotNull(TEXT("真实动画类"), Profile->AnimInstanceClass.Get())) return false;
    Appearance->CharacterState = Identity;
    Appearance->ApprovedVisualHero = Profile->HeroDefinitionId;
    Appearance->PendingProfile = Profile;
    Appearance->HandleVisualResourcesLoaded(Profile, Profile->HeroDefinitionId, Appearance->RequestGeneration);
    auto* Mesh = Character->GetMesh();
    TestTrue(TEXT("网络平滑缓存必须保存异步模型偏移"), Character->GetBaseTranslationOffset().Equals(Mesh->GetRelativeLocation(), 0.01));
    TestTrue(TEXT("网络平滑缓存必须保存异步模型朝向"), Character->GetBaseRotationOffset().Equals(Mesh->GetRelativeRotation().Quaternion(), 0.001));
    TestEqual(TEXT("原型脚底锚点与初始胶囊底部重合"), Mesh->GetRelativeLocation().Z, -88.0);
    Character->GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f, false);
    Appearance->HandleCharacterReadinessChanged(true);
    TestEqual(TEXT("Definition到达后同Hero也必须更新落地偏移"), Mesh->GetRelativeLocation().Z, -96.0);
    TestTrue(TEXT("更新胶囊后网络平滑不得恢复旧高度"), Character->GetBaseTranslationOffset().Equals(Mesh->GetRelativeLocation(), 0.01));
    // 正式模型允许保留美术校准；胶囊缩放不能把相对厘米补偿重复缩放。
    Profile->MeshRelativeLocation = FVector(2.0, 3.0, -92.0);
    Character->SetActorScale3D(FVector(2.0));
    Appearance->HandleCharacterReadinessChanged(true);
    TestTrue(TEXT("美术偏移保留且胶囊采用未缩放单位"), Mesh->GetRelativeLocation().Equals(FVector(2.0, 3.0, -98.0), 0.01));
    Profile->MeshReferenceCapsuleHalfHeightCm = 0.0f;
    Appearance->HandleCharacterReadinessChanged(true);
    TestTrue(TEXT("固定绝对偏移兼容选项"), Mesh->GetRelativeLocation().Equals(Profile->MeshRelativeLocation, 0.01));
    const FVector BeforeStaleCallback = Mesh->GetRelativeLocation();
    Profile->MeshRelativeLocation = FVector(0.0, 0.0, -200.0);
    Appearance->HandleVisualResourcesLoaded(Profile, Profile->HeroDefinitionId, Appearance->RequestGeneration - 1);
    TestTrue(TEXT("过期异步完成不能修改有效模型基准"), Mesh->GetRelativeLocation().Equals(BeforeStaleCallback, 0.01));
    Profile->ProfileId = TEXT("Appearance.Hero.Zodiac.Horse.Default");
    Profile->SkeletonCompatibilityId = TEXT("UE5Mannequin");
    Profile->MeshReferenceCapsuleHalfHeightCm = -1.0f;
    FString Error;
    TestFalse(TEXT("非法参考高度拒绝装配"), Profile->IsProfileValid(Error));
    TestTrue(TEXT("非法参考高度返回具体中文错误"), Error.Contains(TEXT("参考胶囊半高")));
    // A外观仍在显示，B的Profile已到但Mesh尚未完成，此时切回A不能把B的变换缓存到A。
    auto* OtherProfile = NewObject<UDivineBeastsCharacterAppearanceProfile>(Appearance);
    OtherProfile->HeroDefinitionId = TEXT("Hero.Zodiac.Rooster");
    OtherProfile->MeshRelativeLocation = FVector(0.0, 0.0, -300.0);
    Appearance->PendingProfile = OtherProfile;
    Appearance->RefreshAppearance();
    TestTrue(TEXT("切回已显示角色不能应用另一身份的待加载变换"), Mesh->GetRelativeLocation().Equals(BeforeStaleCallback, 0.01));
    TestTrue(TEXT("切回角色必须撤销另一身份的待加载Profile"), Appearance->PendingProfile != OtherProfile);
    return true;
}

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
