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
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
/** 实际引擎帧中的舞台动画回归；保持正式Stage默认属性，不手动TickAnimation、不改全局帧号或资产。 */
class FDivineBeastsPreviewStageAnimationCommand final : public IAutomationLatentCommand
{
public:
    explicit FDivineBeastsPreviewStageAnimationCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual ~FDivineBeastsPreviewStageAnimationCommand() override
    {
        // 独立GameInstance/World作用域最终关闭；不让测试持有的Stage、动画和流式资源跨到正式世界。
        if (Instance.IsValid())
        {
            UWorld* World = Instance->GetWorld();
            if (World) { World->EndPlay(EEndPlayReason::Quit); }
            Instance->Shutdown();
            if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
        }
    }

    virtual bool Update() override
    {
        if (!Instance.IsValid())
        {
            AnimationClass = LoadClass<UAnimInstance>(nullptr,
                TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.ABP_DBA_PreviewIdle_C"));
            auto* Asset = LoadObject<USkeletalMesh>(nullptr,
                TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
            if (!Test->TestNotNull(TEXT("已保存真实预览动画类"), AnimationClass) ||
                !Test->TestNotNull(TEXT("真实预览模型"), Asset)) { return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            UWorld* World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("舞台独立游戏世界"), World)) { return true; }
            FURL URL;
            URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
            if (!Test->TestTrue(TEXT("受控基础GameMode启动"), World->SetGameMode(URL))) { return true; }
            World->InitializeActorsForPlay(URL);
            World->BeginPlay();
            Stage = World->SpawnActor<AGamePlatformCharacterPreviewStage>();
            if (!Test->TestTrue(TEXT("真实Stage生成"), Stage.IsValid())) { return true; }
            if (!Test->TestTrue(TEXT("正式ApplyPreviewAppearance装配"), Stage->ApplyPreviewAppearance(
                Asset, {}, AnimationClass, FVector(0, 0, -88), FRotator(0, -90, 0), FVector::OneVector))) { return true; }
            auto* Mesh = Stage->GetPreviewMeshComponent();
            if (!Test->TestNotNull(TEXT("舞台拥有真实Mesh组件"), Mesh) ||
                !Test->TestNotNull(TEXT("舞台已创建实际AnimInstance"), Mesh->GetAnimInstance())) { return true; }
            Test->TestFalse(TEXT("Stage无Actor业务Tick，动画仍由独立Mesh组件评估"), Stage->PrimaryActorTick.bCanEverTick);
            Test->TestTrue(TEXT("实际Mesh组件Tick已启用"), Mesh->IsComponentTickEnabled());
            Test->TestTrue(TEXT("实际Mesh已注册"), Mesh->IsRegistered());
            Test->TestFalse(TEXT("实际Stage未暂停动画"), Mesh->bPauseAnims);
            Test->TestEqual(TEXT("实际Stage默认持续更新姿势与骨骼"), Mesh->VisibilityBasedAnimTickOption,
                EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
            Test->TestTrue(TEXT("游戏世界Actor已初始化"), World->AreActorsInitialized());
            StartSeconds = FPlatformTime::Seconds();
            LastEngineFrame = GFrameCounter;
            return false;
        }
        if (FPlatformTime::Seconds() - StartSeconds > 30.0)
        { Test->AddError(TEXT("舞台动画回归等待真实引擎帧超时，不能将冻结截图当成通过")); return true; }
        // ShouldTickPose按GFrameCounter去重；Automation真正跨帧才能复现游戏路径，禁止测试伪造帧号。
        if (LastEngineFrame == GFrameCounter) { return false; }
        LastEngineFrame = GFrameCounter;
        UWorld* World = Instance->GetWorld();
        auto* Mesh = Stage.IsValid() ? Stage->GetPreviewMeshComponent() : nullptr;
        if (!Test->TestNotNull(TEXT("帧推进时真实Mesh仍存活"), Mesh)) { return true; }
        World->Tick(LEVELTICK_All, 1.0f / 30.0f);
        World->SendAllEndOfFrameUpdates();
        ++AdvancedFrames;
        auto* Animation = Mesh->GetAnimInstance();
        if (!Test->TestNotNull(TEXT("帧推进时动画实例仍存活"), Animation)) { return true; }
        if (AdvancedFrames == 5)
        {
            PlayerIndex = Animation->GetInstanceAssetPlayerIndex(TEXT("PreviewIdle"), TEXT("Idle"));
            if (!Test->TestTrue(TEXT("保存动画图实际拥有Idle资产播放器"), PlayerIndex != INDEX_NONE)) { return true; }
            FirstTime = Animation->GetInstanceAssetPlayerTime(PlayerIndex);
            FirstPose = Mesh->GetComponentSpaceTransforms();
        }
        if (AdvancedFrames < 23) { return false; }
        const float SecondTime = Animation->GetInstanceAssetPlayerTime(PlayerIndex);
        Test->TestTrue(TEXT("正式World组件Tick使Idle播放器时间推进"), FMath::Abs(SecondTime - FirstTime) > 0.05f);
        Test->TestEqual(TEXT("预览保持实际Idle状态"), Animation->GetCurrentStateName(0), FName(TEXT("Idle")));
        const auto& SecondPose = Mesh->GetComponentSpaceTransforms();
        Test->TestTrue(TEXT("真实预览模型骨骼已求值"), !FirstPose.IsEmpty() && FirstPose.Num() == SecondPose.Num());
        double MaximumAngle = 0;
        double MaximumTranslationCm = 0;
        for (int32 Index = 0; Index < FMath::Min(FirstPose.Num(), SecondPose.Num()); ++Index)
        {
            MaximumAngle = FMath::Max(MaximumAngle, FirstPose[Index].GetRotation().AngularDistance(SecondPose[Index].GetRotation()));
            MaximumTranslationCm = FMath::Max(MaximumTranslationCm, FVector::Distance(FirstPose[Index].GetTranslation(), SecondPose[Index].GetTranslation()));
        }
        Test->TestTrue(TEXT("实际Stage骨姿势随动画时间改变，不能停留A姿势"), MaximumAngle > 0.0001 || MaximumTranslationCm > 0.001);
        Test->AddInfo(FString::Printf(TEXT("PreviewStage: 真实引擎帧=%d IdleTime=%.4f→%.4f MaxBoneAngle=%.6f rad MaxBoneTranslation=%.6f cm"),
            AdvancedFrames, FirstTime, SecondTime, MaximumAngle, MaximumTranslationCm));
        return true;
    }

private:
    FAutomationTestBase* Test;
    TStrongObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<AGamePlatformCharacterPreviewStage> Stage;
    UClass* AnimationClass = nullptr;
    uint64 LastEngineFrame = 0;
    double StartSeconds = 0;
    int32 AdvancedFrames = 0;
    int32 PlayerIndex = INDEX_NONE;
    float FirstTime = 0;
    TArray<FTransform> FirstPose;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPreviewStageAnimationTest,
    "DivineBeasts.Presentation.Characters.PreviewStageAnimation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPreviewStageAnimationTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FDivineBeastsPreviewStageAnimationCommand(this));
    return true;
}

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
