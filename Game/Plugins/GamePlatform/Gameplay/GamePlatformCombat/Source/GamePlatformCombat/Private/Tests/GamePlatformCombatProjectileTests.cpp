// 平台权威投射物回归：真实World/碰撞/ASC/战斗组件，无项目、MOBA、Niagara或网络替身依赖。
// RED阶段曾显式报告缺少生产类型；类型加入后直接编译真实路径，验证曲线运动、伤害与过期来源拒绝。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 夹具只在开发自动化中建立，不装载地图、不改生产配置，析构销毁自己拥有的World。 */
struct FProjectileTestWorld
{
    TStrongObjectPtr<UWorld> World;
    AActor* Source = nullptr;
    AActor* Target = nullptr;
    UGamePlatformCombatComponent* SourceCombat = nullptr;
    UGamePlatformCombatComponent* TargetCombat = nullptr;
    /** 默认Actor来源保持原七项运动测试；控制权回归才使用真实Pawn，以走引擎Possess通知。 */
    bool Initialize(bool bUsePawnSource = false, bool bUsePawnTarget = false)
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        if (!World.IsValid()) return false;
        World->InitializeActorsForPlay(FURL());
        const auto SpawnCombatant = [this](FVector Position, UGamePlatformCombatComponent*& OutCombat, bool bSpawnPawn)
        {
            AActor* Actor = bSpawnPawn ? World->SpawnActor<APawn>() : World->SpawnActor<AActor>();
            auto* Sphere = NewObject<USphereComponent>(Actor);
            Sphere->InitSphereRadius(30.0f);
            Actor->SetRootComponent(Sphere);
            Actor->AddInstanceComponent(Sphere);
            Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Sphere->SetCollisionResponseToAllChannels(ECR_Block);
            Sphere->RegisterComponent();
            Actor->SetActorLocation(Position);
            auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Actor);
            OutCombat = NewObject<UGamePlatformCombatComponent>(Actor);
            Actor->AddInstanceComponent(ASC); Actor->AddInstanceComponent(OutCombat);
            ASC->RegisterComponent(); OutCombat->RegisterComponent();
            ASC->InitAbilityActorInfo(Actor, Actor);
            Actor->DispatchBeginPlay();
            return Actor;
        };
        Source = SpawnCombatant(FVector(-100, 0, 0), SourceCombat, bUsePawnSource);
        Target = SpawnCombatant(FVector(200, 0, 0), TargetCombat, bUsePawnTarget);
        return SourceCombat && TargetCombat;
    }
    FGamePlatformCombatSpec CastSpec() const
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid(); Spec.Source = Source; Spec.Magnitude = 12.0f;
        Spec.DamageType = EGamePlatformDamageType::Magic;
        Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
        Spec.SourceAbilityId = TEXT("Ability.Test.Projectile");
        return Spec;
    }
    ~FProjectileTestWorld() { if (World.IsValid()) World->DestroyWorld(false); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHelixTest,
    "GamePlatform.Combat.Projectile.HelixAndAuthorityDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHelixTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!TestTrue(TEXT("真实带碰撞的权威世界"), Fixture.Initialize())) return false;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    FGamePlatformCombatProjectileMotion Motion;
    Motion.SpeedCmPerSecond = 1000.0f; Motion.RangeCm = 500.0f;
    Motion.HelixRadiusCm = 18.0f; Motion.HelixTurnsPerSecond = 3.0f;
    TestTrue(TEXT("可信服务器初始化"), Projectile->AuthorityInitialize(Fixture.CastSpec(), Motion, FVector::ForwardVector));
    Projectile->Tick(1.0f / 12.0f);
    TestTrue(TEXT("轴向推进且不是直线装饰"), Projectile->GetActorLocation().Equals(FVector(83.333333, -18, 18), 0.1));
    Projectile->Tick(1.0f / 12.0f);
    Projectile->Tick(1.0f / 12.0f);
    TestEqual(TEXT("实际Sweep命中通过平台结算一次12伤害"), Fixture.TargetCombat->GetCombatHealth(), 88.0f);
    Projectile->Tick(0.1f);
    TestEqual(TEXT("重复Tick不能重复结算"), Fixture.TargetCombat->GetCombatHealth(), 88.0f);
    return true;
}

// 生产角色的Pawn胶囊允许忽略Visibility；投射物仍须命中，且不得越过真正遮挡物。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectilePawnCollisionTest,
    "GamePlatform.Combat.Projectile.PawnCollisionIgnoresVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectilePawnCollisionTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!TestTrue(TEXT("真实Pawn目标世界"), Fixture.Initialize(false, true))) return false;
    auto* TargetCollision = Cast<USphereComponent>(Fixture.Target->GetRootComponent());
    if (!TestNotNull(TEXT("Pawn目标碰撞体"), TargetCollision)) return false;
    TargetCollision->SetCollisionObjectType(ECC_Pawn);
    TargetCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    FGamePlatformCombatProjectileMotion Motion;
    Motion.SpeedCmPerSecond = 1000.0f; Motion.RangeCm = 500.0f;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("Pawn目标前方初始化弹体"),
        Projectile->AuthorityInitialize(Fixture.CastSpec(), Motion, FVector::ForwardVector))) return false;
    Projectile->Tick(0.3f);
    TestEqual(TEXT("忽略Visibility的Pawn仍由权威碰撞结算"), Fixture.TargetCombat->GetCombatHealth(), 88.0f);

    FProjectileTestWorld Blocked;
    if (!TestTrue(TEXT("遮挡验证世界"), Blocked.Initialize(false, true))) return false;
    auto* BlockedCollision = Cast<USphereComponent>(Blocked.Target->GetRootComponent());
    if (!TestNotNull(TEXT("遮挡验证Pawn碰撞体"), BlockedCollision)) return false;
    BlockedCollision->SetCollisionObjectType(ECC_Pawn);
    BlockedCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    AActor* Wall = Blocked.World->SpawnActor<AActor>();
    auto* WallCollision = NewObject<USphereComponent>(Wall);
    WallCollision->InitSphereRadius(25.0f);
    Wall->SetRootComponent(WallCollision); Wall->AddInstanceComponent(WallCollision);
    WallCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
    WallCollision->RegisterComponent(); Wall->SetActorLocation(FVector(100, 0, 0));
    Wall->DispatchBeginPlay();
    auto* BlockedProjectile = Blocked.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("墙前初始化弹体"),
        BlockedProjectile->AuthorityInitialize(Blocked.CastSpec(), Motion, FVector::ForwardVector))) return false;
    BlockedProjectile->Tick(0.3f);
    TestEqual(TEXT("Visibility墙仍先于Pawn阻挡弹体"), Blocked.TargetCombat->GetCombatHealth(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileStaleSourceTest,
    "GamePlatform.Combat.Projectile.StaleSourceAndExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileStaleSourceTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    FGamePlatformCombatProjectileMotion Motion;
    Motion.SpeedCmPerSecond = 1000; Motion.RangeCm = 500;
    TestTrue(TEXT("初始合法来源"), Projectile->AuthorityInitialize(Fixture.CastSpec(), Motion, FVector::ForwardVector));
    Fixture.SourceCombat->ResetForNewAvatar(2);
    Projectile->Tick(0.5f);
    TestEqual(TEXT("旧化身弹体不伤害新世界中的目标"), Fixture.TargetCombat->GetCombatHealth(), 100.0f);
    TestTrue(TEXT("旧来源立即停止飞行"), Projectile->IsFlightFinished());
    auto* Empty = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    Motion.RangeCm = 50;
    TestTrue(TEXT("新来源短射程合法"), Empty->AuthorityInitialize(Fixture.CastSpec(), Motion, FVector::UpVector));
    Empty->Tick(0.1f);
    TestTrue(TEXT("射程结束停止，无虚假命中"), Empty->IsFlightFinished());
    Motion.SpeedCmPerSecond = -1;
    auto* Invalid = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestFalse(TEXT("非法速度不能生成有效施法"), Invalid->AuthorityInitialize(Fixture.CastSpec(), Motion, FVector::ForwardVector));
    return true;
}
// 当前选中目标在服务器规格中冻结。移动后必须按真实位置转向，而不是只向施放时坐标飞行。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHomingTest,
    "GamePlatform.Combat.Projectile.SelectedTargetHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHomingTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm=500;
    auto Spec=Fixture.CastSpec(); Spec.Target=Fixture.Target;
    Spec.TargetAvatarGeneration=Fixture.TargetCombat->GetCombatAvatarGeneration();
    auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("冻结有效目标后初始化"),Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector));
    Fixture.Target->SetActorLocation(FVector(200,200,0));
    Projectile->Tick(0.1f);
    TestTrue(TEXT("移动目标使弹体实际横向转弯"),Projectile->GetActorLocation().Y>20);
    Projectile->Tick(0.2f);
    TestEqual(TEXT("真实扫掠命中移动目标只结算一次"),Fixture.TargetCombat->GetCombatHealth(),88.f);
    Projectile->Tick(0.2f);
    TestEqual(TEXT("完成后不重复伤害"),Fixture.TargetCombat->GetCombatHealth(),88.f);
    // 未选择目标时不能扫描或自动吸附同世界任何敌人。
    auto* Straight=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("无选中目标仍可施放"),Straight->AuthorityInitialize(Fixture.CastSpec(),Motion,FVector::ForwardVector));
    Straight->Tick(0.1f);
    TestTrue(TEXT("无目标维持直线"),Straight->GetActorLocation().Equals(FVector(100,0,0),0.1));
    return true;
}

// 目标原点可能位于脚底；锁定与命中表现使用真实碰撞中心，伤害仍经过服务器扫掠。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileTargetCenterTest,
    "GamePlatform.Combat.Projectile.TargetCollisionCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileTargetCenterTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    auto* Collision = CastChecked<USphereComponent>(Fixture.Target->GetRootComponent());
    auto* FootOrigin = NewObject<USceneComponent>(Fixture.Target);
    Fixture.Target->AddInstanceComponent(FootOrigin);
    Fixture.Target->SetRootComponent(FootOrigin);
    FootOrigin->RegisterComponent();
    FootOrigin->SetWorldLocation(FVector(200, 0, 0));
    Collision->AttachToComponent(FootOrigin, FAttachmentTransformRules::KeepRelativeTransform);
    Collision->SetRelativeLocation(FVector(0, 0, 100));
    const FVector Center(200, 0, 100);
    TestTrue(TEXT("目标中心区别于脚底Actor原点"), Fixture.TargetCombat->GetCombatTargetPoint().Equals(Center, .1));
    auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target;
    Spec.TargetAvatarGeneration = Fixture.TargetCombat->GetCombatAvatarGeneration();
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = 500;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("选中脚底原点目标后可锁定"), Projectile->AuthorityInitialize(Spec, Motion, Center.GetSafeNormal()));
    for (int32 Step = 0; Step < 8 && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(.05f);
    TestTrue(TEXT("服务器确认实际命中"), Projectile->GetFlightFacts().bImpacted);
    TestTrue(TEXT("复制给VFX与SFX的命中点为碰撞中心"), Projectile->GetFlightFacts().ImpactLocation.Equals(Center, .1));
    TestEqual(TEXT("中心定位不重复结算伤害"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
    return true;
}

// 同一个Actor重生已是新目标，旧弹体必须结束；不能把引用地址相同当作同一化身继续追踪。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHomingGenerationTest,
    "GamePlatform.Combat.Projectile.HomingTargetGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHomingGenerationTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm=500;
    auto Spec=Fixture.CastSpec();Spec.Target=Fixture.Target;
    Spec.TargetAvatarGeneration=Fixture.TargetCombat->GetCombatAvatarGeneration();
    auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("有效目标初始化"),Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector));
    Fixture.TargetCombat->ResetForNewAvatar(2);
    Projectile->Tick(0.1f);
    TestTrue(TEXT("目标化身失效立即终止旧弹体"),Projectile->IsFlightFinished());
    Projectile->Tick(0.3f);
    TestEqual(TEXT("旧弹体不伤害新化身"),Fixture.TargetCombat->GetCombatHealth(),100.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSelectionTest,
    "GamePlatform.Combat.Projectile.SelectionAuthorityAndGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSelectionTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    int32 AvatarObservationCount = 0;
    const auto ObservationHandle = Fixture.TargetCombat->OnCombatObservationChanged().AddLambda([&]() { ++AvatarObservationCount; });
    ON_SCOPE_EXIT { Fixture.TargetCombat->OnCombatObservationChanged().Remove(ObservationHandle); };
    int32 Generation=-1;
    TestNull(TEXT("初始无选中目标"),Fixture.SourceCombat->GetSelectedProjectileTarget(1000,Generation));
    TestEqual(TEXT("空快照代次归零"),Generation,0);
    TWeakObjectPtr<AActor> IntentTarget; int32 IntentSource = -1, IntentGeneration = -1;
    TestFalse(TEXT("未选择时不存在服务器意图快照"), Fixture.SourceCombat->GetSelectedProjectileTargetIntent(IntentTarget, IntentSource, IntentGeneration));
    TestTrue(TEXT("可信服务器选择同世界目标"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(Fixture.Target,1,1));
    TestEqual(TEXT("射程内读取同一目标"),Fixture.SourceCombat->GetSelectedProjectileTarget(1000,Generation),Fixture.Target);
    TestNull(TEXT("超出具体技能射程不锁定"),Fixture.SourceCombat->GetSelectedProjectileTarget(100,Generation));
    TestTrue(TEXT("超射程仍保留选择意图，策略不得误认为无选择"), Fixture.SourceCombat->GetSelectedProjectileTargetIntent(IntentTarget, IntentSource, IntentGeneration));
    TestTrue(TEXT("意图弱身份及冻结代次正确"), IntentTarget.Get() == Fixture.Target && IntentSource == 1 && IntentGeneration == 1);
    TestFalse(TEXT("错误目标代次的替换选择被拒绝"), Fixture.SourceCombat->AuthoritySelectProjectileTarget(Fixture.Target, 1, 2));
    TestTrue(TEXT("拒绝替换仍保留旧选择意图而非隐式清空"),
        Fixture.SourceCombat->GetSelectedProjectileTargetIntent(IntentTarget, IntentSource, IntentGeneration) &&
        IntentTarget.Get() == Fixture.Target && IntentSource == 1 && IntentGeneration == 1);
    TestFalse(TEXT("旧来源取消不能覆盖后继选择"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(nullptr,0,0));
    TestEqual(TEXT("旧取消后当前选择仍在"),Fixture.SourceCombat->GetSelectedProjectileTarget(1000,Generation),Fixture.Target);
    Fixture.TargetCombat->ResetForNewAvatar(2);
    TestEqual(TEXT("化身重置提交后只读观察者立即获得一次通知，不依赖位置/生命差值"), AvatarObservationCount, 1);
    TestNull(TEXT("同Actor新化身不继承旧选择"),Fixture.SourceCombat->GetSelectedProjectileTarget(1000,Generation));
    TestTrue(TEXT("目标换代后仍可识别旧意图"), Fixture.SourceCombat->GetSelectedProjectileTargetIntent(IntentTarget, IntentSource, IntentGeneration) && IntentGeneration == 1);
    TestTrue(TEXT("显式选择新化身"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(Fixture.Target,1,2));
    Fixture.SourceCombat->ResetForNewAvatar(2);
    TestNull(TEXT("来源换化身清除旧资格"),Fixture.SourceCombat->GetSelectedProjectileTarget(1000,Generation));
    TestFalse(TEXT("禁止追踪自身"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(Fixture.Source,2,2));
    FProjectileTestWorld Other;
    if (!Other.Initialize()) return false;
    TestFalse(TEXT("禁止跨世界目标"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(Other.Target,2,1));
    TestTrue(TEXT("显式取消选择幂等"),Fixture.SourceCombat->AuthoritySelectProjectileTarget(nullptr,2,0));
    const UFunction* RPC=UGamePlatformCombatComponent::StaticClass()->FindFunctionByName(TEXT("ServerSelectProjectileTarget"));
    TestTrue(TEXT("网络意图使用拥有者服务器可靠RPC"),RPC && RPC->HasAllFunctionFlags(FUNC_Net|FUNC_NetServer|FUNC_NetReliable));
    return true;
}

// 保留Pawn重连不换Avatar：选择属于本次控制者，已发出的弹体则保留自己的冻结目标。
// 删除生产ControllerChanged清理会使两处空选择断言失败；把清理扩大为重置Avatar/取消弹体也必须失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectilePossessionSelectionTest,
    "GamePlatform.Combat.Projectile.SelectionClearsOnControllerChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectilePossessionSelectionTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!TestTrue(TEXT("真实Pawn及战斗组件已进入BeginPlay"), Fixture.Initialize(true))) return false;
    auto* Pawn = Cast<APawn>(Fixture.Source);
    auto* FirstController = Fixture.World->SpawnActor<APlayerController>();
    auto* SecondController = Fixture.World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("实际来源Pawn"), Pawn) ||
        !TestNotNull(TEXT("第一位真实控制者"), FirstController) ||
        !TestNotNull(TEXT("后继真实控制者"), SecondController)) return false;

    FirstController->Possess(Pawn);
    TestEqual(TEXT("第一位控制者实际拥有来源"), Pawn->GetController(), static_cast<AController*>(FirstController));
    const int32 SourceGeneration = Fixture.SourceCombat->GetCombatAvatarGeneration();
    Fixture.Target->SetActorLocation(FVector(600, 0, 0));
    if (!TestTrue(TEXT("本次拥有者通过正式选择入口选中目标"), Fixture.SourceCombat->RequestProjectileTarget(Fixture.Target))) return false;
    int32 TargetGeneration = 0;
    auto Spec = Fixture.CastSpec();
    Spec.Target = Fixture.SourceCombat->GetSelectedProjectileTarget(1500, TargetGeneration);
    Spec.TargetAvatarGeneration = TargetGeneration;
    if (!TestEqual(TEXT("弹体初始化前读取本次实际选择"), Spec.Target.Get(), Fixture.Target)) return false;
    FGamePlatformCombatProjectileMotion Motion;
    Motion.RangeCm = 1500;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("交接前弹体冻结原目标"), Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;

    FirstController->UnPossess();
    TestNull(TEXT("原控制者已真正释放保留Pawn"), Pawn->GetController());
    TestNull(TEXT("释放控制权时服务器立即清除旧选择"), Fixture.SourceCombat->GetSelectedProjectileTarget(1500, TargetGeneration));
    TestEqual(TEXT("释放后的空选择没有目标代次"), TargetGeneration, 0);
    TestEqual(TEXT("控制权交接不冒充角色重生"), Fixture.SourceCombat->GetCombatAvatarGeneration(), SourceGeneration);
    SecondController->Possess(Pawn);
    TestEqual(TEXT("新控制者实际拥有同一个Pawn"), Pawn->GetController(), static_cast<AController*>(SecondController));
    TestNull(TEXT("新控制者未发送选择或清空RPC时仍无目标"), Fixture.SourceCombat->GetSelectedProjectileTarget(1500, TargetGeneration));
    TestEqual(TEXT("重新拥有仍保留原Avatar代次"), Fixture.SourceCombat->GetCombatAvatarGeneration(), SourceGeneration);

    // 通过移动目标、真实子步和Sweep证明弹体仍追踪冻结身份，不能只检查未结束标志。
    Fixture.Target->SetActorLocation(FVector(600, 200, 0));
    Projectile->Tick(0.1f);
    TestFalse(TEXT("清选择没有取消已初始化弹体"), Projectile->IsFlightFinished());
    const double InstantAimY = 100.f * FVector(600, 200, 0).GetSafeNormal().Y;
    // 本夹具正面0度初始化，按当前<30度合同直追；控制权交接不能把弹体改回弧线或换目标。
    TestTrue(TEXT("正面弹体在交接后仍直追原目标，不能停追或恢复旧弧线"),
        FMath::Abs(Projectile->GetActorLocation().Y - InstantAimY) < .05f);
    for (int32 Step = 0; Step < 4 && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(0.25f);
    TestTrue(TEXT("原冻结目标正常被真实Sweep命中"), Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
    TestEqual(TEXT("交接前弹体仍仅结算一次原有12伤害"), Fixture.TargetCombat->GetCombatHealth(), 88.f);

    // 同一Pawn再次交接仍须成对清理；后继拥有者的显式选择不因首次清理失去资格。
    TestTrue(TEXT("新控制者可以重新显式选择"), Fixture.SourceCombat->RequestProjectileTarget(Fixture.Target));
    TestEqual(TEXT("后继选择真实生效"), Fixture.SourceCombat->GetSelectedProjectileTarget(1500, TargetGeneration), Fixture.Target);
    SecondController->UnPossess();
    TestNull(TEXT("重复交接也清除本轮选择"), Fixture.SourceCombat->GetSelectedProjectileTarget(1500, TargetGeneration));
    TestEqual(TEXT("重复交接不增加Avatar代次"), Fixture.SourceCombat->GetCombatAvatarGeneration(), SourceGeneration);
    return true;
}

// 追踪只改变运动，真实阻挡仍先于目标结算，不能穿墙命中或扫描换敌。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHomingWallTest,
    "GamePlatform.Combat.Projectile.HomingRespectsBlockers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHomingWallTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Target->SetActorLocation(FVector(200,200,0));
    AActor* Wall=Fixture.World->SpawnActor<AActor>();
    // 小球可被加宽的真实弧线合法绕过；改用跨越飞行平面的实体墙，保持验证真实遮挡而非固定旧路线。
    auto* Box=NewObject<UBoxComponent>(Wall);Box->InitBoxExtent(FVector(5,500,200));
    Wall->SetRootComponent(Box);Wall->AddInstanceComponent(Box);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();Wall->SetActorLocation(FVector(100,100,0));Wall->DispatchBeginPlay();
    auto Spec=Fixture.CastSpec();Spec.Target=Fixture.Target;Spec.TargetAvatarGeneration=1;
    FGamePlatformCombatProjectileMotion Motion;Motion.RangeCm=500;
    auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("受阻追踪弹体初始化"),Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector));
    Projectile->Tick(0.25f);
    TestTrue(TEXT("真实阻挡结束飞行"),Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
    TestEqual(TEXT("墙后目标不受到虚假命中"),Fixture.TargetCombat->GetCombatHealth(),100.f);
    return true;
}
// 非零螺旋在半圈时急转90度仍连续；每段位移上界由轴向步长和相邻相位弦长决定。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHomingHelixContinuityTest,
    "GamePlatform.Combat.Projectile.HomingHelixContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHomingHelixContinuityTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Target->SetActorLocation(FVector(2000,0,0));
    auto Spec=Fixture.CastSpec(); Spec.Target=Fixture.Target; Spec.TargetAvatarGeneration=1;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm=5000;
    Motion.HelixRadiusCm=100; Motion.HelixTurnsPerSecond=3;
    auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("大半径追踪初始化"),Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector))) return false;
    Projectile->Tick(1.f/6.f);
    const FVector Before=Projectile->GetActorLocation();
    Fixture.Target->SetActorLocation(FVector(Before.X,2000,0));
    constexpr float Step=1.f/120.f;
    Projectile->Tick(Step);
    const float Axial=Step*Motion.SpeedCmPerSecond;
    const float Chord=2*Motion.HelixRadiusCm*FMath::Sin(UE_PI*Step*Motion.HelixTurnsPerSecond);
    TestTrue(TEXT("急转不重放历史偏移或跨段瞬移"),
        FVector::Distance(Before,Projectile->GetActorLocation())<=FMath::Sqrt(Axial*Axial+Chord*Chord)+0.2f);
    TestFalse(TEXT("无阻挡急转仍在飞行"),Projectile->IsFlightFinished());
    return true;
}
// 施法朝向保留为起飞切线；锁定偏离该方向时应形成弧线并继续追踪同一移动目标。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileCurvedFacingTest,
    "GamePlatform.Combat.Projectile.CurvedLaunchPreservesFacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileCurvedFacingTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Target->SetActorLocation(FVector(0, 1000, 0));
    auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = 2000; Motion.SpeedCmPerSecond = 1200;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("侧向锁定目标初始化"), Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;
    Projectile->Tick(1.f / 120.f);
    const FVector First = Projectile->GetActorLocation();
    TestTrue(TEXT("第一段沿施法前方起飞，不能瞬间改为侧向直射"), First.X > 9.f && First.Y > 0.f && First.Y < 2.f);
    Fixture.Target->SetActorLocation(FVector(-250, 900, 100));
    for (int32 Step = 0; Step < 50 && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(.05f);
    TestTrue(TEXT("弧线持续追踪移动目标并由Sweep确认命中"), Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
    TestEqual(TEXT("移动目标仍只受一次权威伤害"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
    Projectile->Tick(.25f);
    TestEqual(TEXT("弧线命中不重复结算"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileCurvedReverseTest,
    "GamePlatform.Combat.Projectile.CurvedReverseTargetAtRangeBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileCurvedReverseTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Source->SetActorLocation(FVector::ZeroVector);
    Fixture.Target->SetActorLocation(FVector(-2000, 0, 0));
    auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = 2000; Motion.SpeedCmPerSecond = 1200;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>(FVector(50, 0, 0), FRotator::ZeroRotator);
    if (!TestTrue(TEXT("正后方射程边界目标不因起射偏移而被拒绝"), Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;
    Projectile->Tick(1.f / 120.f);
    TestTrue(TEXT("正后方先沿前方起飞而非立即反向"), Projectile->GetActorLocation().X > 59.f);
    // 贝塞尔的起点切线严格沿前方，横向偏移二阶增长；先验证微小曲率，再在完整飞行中验证真实命中。
    TestTrue(TEXT("180度退化情况下仍生成有限弯曲"), FMath::Abs(Projectile->GetActorLocation().Y) > .01f);
    for (int32 Step = 0; Step < 80 && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(.05f);
    TestEqual(TEXT("加宽贝塞尔的有限弧长预算让合法射程目标正常被命中"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
    TestTrue(TEXT("后方追踪正常收敛，不无限绕圈"), Projectile->IsFlightFinished());
    FProjectileTestWorld Vertical;
    if (!Vertical.Initialize()) return false;
    Vertical.Target->SetActorLocation(FVector(0, 0, -900));
    auto VerticalSpec = Vertical.CastSpec(); VerticalSpec.Target = Vertical.Target; VerticalSpec.TargetAvatarGeneration = 1;
    auto* Upward = Vertical.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!Upward->AuthorityInitialize(VerticalSpec, Motion, FVector::UpVector)) return false;
    Upward->Tick(1.f / 120.f);
    TestTrue(TEXT("竖直反向也保留向上起飞切线"), Upward->GetActorLocation().Z > 9.f);
    for (int32 Step = 0; Step < 50 && !Upward->IsFlightFinished(); ++Step) Upward->Tick(.05f);
    TestEqual(TEXT("竖直叉积退化不产生NaN，仍命中原目标"), Vertical.TargetCombat->GetCombatHealth(), 88.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileCurvedBlockerTest,
    "GamePlatform.Combat.Projectile.CurvedLaunchRespectsBlockers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileCurvedBlockerTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Target->SetActorLocation(FVector(0, 1000, 0));
    AActor* Wall = Fixture.World->SpawnActor<AActor>();
    auto* Collision = NewObject<USphereComponent>(Wall); Collision->InitSphereRadius(20);
    Wall->SetRootComponent(Collision); Wall->AddInstanceComponent(Collision);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->RegisterComponent(); Wall->SetActorLocation(FVector(55, 0, 0)); Wall->DispatchBeginPlay();
    auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
    FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = 2000; Motion.SpeedCmPerSecond = 1200;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector)) return false;
    Projectile->Tick(.2f);
    TestTrue(TEXT("前方起飞弧段中的真实障碍阻止飞行"), Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
    TestEqual(TEXT("受阻弧线不能绕过真实碰撞伪造目标伤害"), Fixture.TargetCombat->GetCombatHealth(), 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileCurvedHelixConvergenceTest,
    "GamePlatform.Combat.Projectile.CurvedHelixConvergesOnTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileCurvedHelixConvergenceTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if (!Fixture.Initialize()) return false;
    Fixture.Source->SetActorLocation(FVector::ZeroVector);
    Fixture.Target->SetActorLocation(FVector(0, 500, 0));
    auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
    FGamePlatformCombatProjectileMotion Motion;
    Motion.RangeCm = 2000; Motion.SpeedCmPerSecond = 1200;
    Motion.HelixRadiusCm = 60; Motion.HelixTurnsPerSecond = 3;
    auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>(FVector(50, 0, 50), FRotator::ZeroRotator);
    if (!Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector)) return false;
    for (int32 Step = 0; Step < 300 && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(1.f / 120.f);
    TestEqual(TEXT("大半径螺旋沿弧线接近时收束，不能持续绕过静止目标"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
    TestTrue(TEXT("螺旋收束仍由真实Sweep命中"), Projectile->GetFlightFacts().bImpacted);
    return true;
}
// 用户确认的新合同：发射时角色中心夹角小于30度直追原目标，取消额外Bezier弧线。
// 覆盖近/中/远与最大合法射程，距离和高速不能偷偷恢复小偏角弧线。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileFrontalConeTest,
    "GamePlatform.Combat.Projectile.FrontalConeStraightAtEveryDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileFrontalConeTest::RunTest(const FString&)
{
    for (const float Distance : {100.f, 1000.f, 10000.f, 100000.f})
    {
        FProjectileTestWorld Fixture;
        if (!Fixture.Initialize()) return false;
        Fixture.Source->SetActorLocation(FVector::ZeroVector);
        const FVector TargetDirection = FRotator(0, 5, 0).Vector();
        Fixture.Target->SetActorLocation(TargetDirection * Distance);
        auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
        FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = Distance;
        auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
        if (!TestTrue(FString::Printf(TEXT("%.0f厘米目标初始化"), Distance),
            Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;
        double MaxDeviation = 0;
        while (!Projectile->IsFlightFinished() && Projectile->GetActorLocation().X < Distance * .25f)
        {
            Projectile->Tick(1.f / 120.f);
            MaxDeviation = FMath::Max(MaxDeviation,
                FVector::CrossProduct(Projectile->GetActorLocation(), TargetDirection).Size());
        }
        TestTrue(FString::Printf(TEXT("%.0f厘米的5度法术沿目标中心直追，偏弦%.4f厘米"), Distance, MaxDeviation),
            MaxDeviation < .02);
        // 同一距离真正朝向目标也保持直线，射程边界仍使用原有严格资格。
        Projectile->AuthorityCancel();
        auto* Facing = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
        Spec.EventId = FGuid::NewGuid();
        if (!Facing->AuthorityInitialize(Spec, Motion, TargetDirection)) return false;
        Facing->Tick(FMath::Min(.1f, Distance / Motion.SpeedCmPerSecond * .25f));
        TestTrue(TEXT("朝向静止目标时仍为直线"),
            FVector::CrossProduct(Facing->GetActorLocation(), TargetDirection).Size() < .01);
        Fixture.Target->SetActorLocation(TargetDirection * (Distance + .1));
        auto* OutOfRange = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
        Spec.EventId = FGuid::NewGuid();
        TestFalse(TEXT("浮点边界容差不能接受超出0.1厘米的真实目标"),
            OutOfRange->AuthorityInitialize(Spec, Motion, FVector::ForwardVector));
    }
    FProjectileTestWorld Close;
    if (!Close.Initialize()) return false;
    Close.Source->SetActorLocation(FVector::ZeroVector);
    const FVector CloseDirection = FRotator(0, 5, 0).Vector();
    Close.Target->SetActorLocation(CloseDirection * 50.f);
    // 第一段留在目标外；否则已确认命中后的中心定位会掩盖实际Sweep弧段。
    CastChecked<USphereComponent>(Close.Target->GetRootComponent())->SetSphereRadius(1.f);
    auto CloseSpec = Close.CastSpec(); CloseSpec.Target = Close.Target; CloseSpec.TargetAvatarGeneration = 1;
    FGamePlatformCombatProjectileMotion Fast; Fast.RangeCm = 100; Fast.SpeedCmPerSecond = 20000;
    auto* FastProjectile = Close.World->SpawnActor<AGamePlatformCombatProjectile>();
    if (!TestTrue(TEXT("极近高速小偏角初始化"), FastProjectile->AuthorityInitialize(CloseSpec, Fast, FVector::ForwardVector))) return false;
    FastProjectile->Tick(15.f / Fast.SpeedCmPerSecond);
    TestTrue(TEXT("极近高速的小偏角法术也沿目标中心直追"),
        FVector::CrossProduct(FastProjectile->GetActorLocation(), CloseDirection).Size() < .02);
    return true;
}

// 严格30度边界与角色中心判定；手部偏移不能把29.9度误分类为大角度。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileFrontalBoundaryTest,
    "GamePlatform.Combat.Projectile.FrontalConeBoundaryAndSourceCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileFrontalBoundaryTest::RunTest(const FString&)
{
    for (const float Yaw : {29.9f, 30.f, 30.1f})
    {
        FProjectileTestWorld Fixture;
        if (!Fixture.Initialize()) return false;
        Fixture.Source->SetActorLocation(FVector::ZeroVector);
        const FVector TargetDirection=FRotator(0,Yaw,0).Vector();
        Fixture.Target->SetActorLocation(TargetDirection*1000.);
        auto Spec=Fixture.CastSpec();Spec.Target=Fixture.Target;Spec.TargetAvatarGeneration=1;
        FGamePlatformCombatProjectileMotion Motion;Motion.RangeCm=1500;
        auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
        if(!Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector))return false;
        double Deviation=0;
        for(int32 Step=0;Step<30&&!Projectile->IsFlightFinished();++Step)
        {
            Projectile->Tick(1.f/120.f);
            Deviation=FMath::Max(Deviation,FVector::CrossProduct(Projectile->GetActorLocation(),TargetDirection).Size());
        }
        TestTrue(FString::Printf(TEXT("%.1f度严格锥边界，偏弦%.4f厘米"),Yaw,Deviation),Yaw<30.f?Deviation<.02:Deviation>1.);
        Projectile->AuthorityCancel();
    }
    FProjectileTestWorld Offset;
    if(!Offset.Initialize())return false;
    Offset.Source->SetActorLocation(FVector::ZeroVector);
    Offset.Target->SetActorLocation(FRotator(0,29.9,0).Vector()*1000.);
    const FVector Origin(300.,-100.,0.);
    const FVector Chord=(Offset.TargetCombat->GetCombatTargetPoint()-Origin).GetSafeNormal();
    auto Spec=Offset.CastSpec();Spec.Target=Offset.Target;Spec.TargetAvatarGeneration=1;
    FGamePlatformCombatProjectileMotion Motion;Motion.RangeCm=1500;
    auto* Projectile=Offset.World->SpawnActor<AGamePlatformCombatProjectile>(Origin,FRotator::ZeroRotator);
    if(!Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector))return false;
    Projectile->Tick(.1f);
    TestTrue(TEXT("发射偏移不改变角色中心29.9度的直追资格"),FVector::CrossProduct(Projectile->GetActorLocation()-Origin,Chord).Size()<.02);
    return true;
}

// 分类在发射时冻结；原目标移动到大角度时仍直追，不重添弧线、不停止推进。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileFrontalMovingTest,
    "GamePlatform.Combat.Projectile.FrontalConeTracksMovingTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileFrontalMovingTest::RunTest(const FString&)
{
    FProjectileTestWorld Fixture;
    if(!Fixture.Initialize())return false;
    Fixture.Source->SetActorLocation(FVector::ZeroVector);
    Fixture.Target->SetActorLocation(FRotator(0,20,0).Vector()*1000.);
    auto Spec=Fixture.CastSpec();Spec.Target=Fixture.Target;Spec.TargetAvatarGeneration=1;
    FGamePlatformCombatProjectileMotion Motion;Motion.RangeCm=2000;
    auto* Projectile=Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
    if(!Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector))return false;
    Projectile->Tick(.05f);
    Fixture.Target->SetActorLocation(FVector(0,900,100));
    const FVector Before=Projectile->GetActorLocation();
    const FVector Desired=(Fixture.TargetCombat->GetCombatTargetPoint()-Before).GetSafeNormal();
    Projectile->Tick(1.f/120.f);
    const FVector Moved=Projectile->GetActorLocation()-Before;
    TestTrue(TEXT("原目标移动后每子步仍有实际推进"),Moved.Size()>1.);
    TestTrue(TEXT("目标转到侧向后不恢复额外Bezier弧度"),FVector::CrossProduct(Moved,Desired).Size()<.02);
    for(int32 Step=0;Step<300&&!Projectile->IsFlightFinished();++Step)Projectile->Tick(1.f/120.f);
    TestTrue(TEXT("直追仍由真实Sweep确认移动目标命中"),Projectile->IsFlightFinished()&&Projectile->GetFlightFacts().bImpacted);
    TestEqual(TEXT("移动原目标只承受一次权威伤害"),Fixture.TargetCombat->GetCombatHealth(),88.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileLongRangeConvergenceTest,
    "GamePlatform.Combat.Projectile.CurvedLongRangeConvergence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileLongRangeConvergenceTest::RunTest(const FString&)
{
    for (const float Range : {10000.f, 100000.f})
        for (const float Yaw : {45.f, 90.f, 180.f})
        {
            FProjectileTestWorld Fixture;
            if (!Fixture.Initialize()) return false;
            Fixture.Source->SetActorLocation(FVector::ZeroVector);
            Fixture.Target->SetActorLocation(FRotator(0, Yaw, 0).Vector() * Range);
            auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = 1;
            FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = Range;
            auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>();
            if (!TestTrue(TEXT("合法远射程边界目标初始化"), Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;
            // 大偏角的贝塞尔弧段比直线路程长；以两倍直飞时间作为测试等待上限，期限不随追逐延长。
            const int32 MaxSteps = FMath::CeilToInt((2.f * Range / Motion.SpeedCmPerSecond + .5f) / .1f) + 1;
            for (int32 Step = 0; Step < MaxSteps && !Projectile->IsFlightFinished(); ++Step) Projectile->Tick(.1f);
            TestEqual(FString::Printf(TEXT("%.0f厘米/%.0f度弧线仍在有限期限内命中"), Range, Yaw),
                Fixture.TargetCombat->GetCombatHealth(), 88.f);
            TestTrue(TEXT("远距离弧线真实Sweep确认且已收敛"), Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
        }
    return true;
}

// 50米是中心间的最大施法距离；侧向/背向必须有米级连续弧段，而不是在枪口瞬间吸附再直飞。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileVisibleRangeArcTest,
    "GamePlatform.Combat.Projectile.VisibleArcAtFiftyMetres",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileVisibleRangeArcTest::RunTest(const FString&)
{
    for (const float Yaw : {45.f, 90.f, 135.f, 180.f})
    {
        FProjectileTestWorld Fixture;
        if (!Fixture.Initialize()) return false;
        Fixture.Source->SetActorLocation(FVector::ZeroVector);
        const FVector TargetDirection = FRotator(0, Yaw, 0).Vector();
        Fixture.Target->SetActorLocation(TargetDirection * 5000.f);
        if (!TestTrue(TEXT("50米内目标可以明确选择"), Fixture.SourceCombat->AuthoritySelectProjectileTarget(Fixture.Target, 1, 1))) return false;
        int32 Generation = 0;
        if (!TestEqual(TEXT("50米边界保留锁定"), Fixture.SourceCombat->GetSelectedProjectileTarget(5000.f, Generation), Fixture.Target)) return false;
        auto Spec = Fixture.CastSpec(); Spec.Target = Fixture.Target; Spec.TargetAvatarGeneration = Generation;
        FGamePlatformCombatProjectileMotion Motion; Motion.RangeCm = 5000; Motion.SpeedCmPerSecond = 1200;
        auto* Projectile = Fixture.World->SpawnActor<AGamePlatformCombatProjectile>(FVector(50, 0, 0), FRotator::ZeroRotator);
        if (!TestTrue(TEXT("50米边界从角色前方真实起射"), Projectile->AuthorityInitialize(Spec, Motion, FVector::ForwardVector))) return false;
        double MaxDeviation = 0;
        int32 Steps = 0;
        const int32 Limit = FMath::CeilToInt((2.f * Motion.RangeCm / Motion.SpeedCmPerSecond + .5f) * 120.f);
        while (!Projectile->IsFlightFinished() && Steps++ < Limit)
        {
            Projectile->Tick(1.f / 120.f);
            MaxDeviation = FMath::Max(MaxDeviation, FVector::CrossProduct(Projectile->GetActorLocation(), TargetDirection).Size());
        }
        // 50米下加大弧形后的最低可见宽度：45/90/135/180度分别8.5/14.5/15.5/12.5米。
        const double MinimumDeviation = Yaw < 90.f ? 850. : (Yaw < 135.f ? 1450. : (Yaw < 180.f ? 1550. : 1250.));
        TestTrue(FString::Printf(TEXT("%.0f度具有可见宽弧：偏离弦线%.1f厘米，至少%.0f厘米"), Yaw, MaxDeviation, MinimumDeviation), MaxDeviation >= MinimumDeviation);
        AddInfo(FString::Printf(TEXT("50米贝塞尔实测：初始偏角%.0f度，最大偏弦%.1f厘米，步数%d"), Yaw, MaxDeviation, Steps));
        TestTrue(TEXT("有限期限内真实Sweep命中原目标"), Projectile->IsFlightFinished() && Projectile->GetFlightFacts().bImpacted);
        TestEqual(TEXT("弧形飞行只结算一次真实伤害"), Fixture.TargetCombat->GetCombatHealth(), 88.f);
        Fixture.Target->SetActorLocation(TargetDirection * 5000.1f);
        TestNull(TEXT("超过最远50米不允许锁定施法"), Fixture.SourceCombat->GetSelectedProjectileTarget(5000.f, Generation));
    }
    return true;
}
#if __has_include("Types/GamePlatformCombatProjectileFlight.h")
#include "Types/GamePlatformCombatProjectileFlight.h"
#define GP_HAS_PROJECTILE_FLIGHT 1
#else
#define GP_HAS_PROJECTILE_FLIGHT 0
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSpeedPolicyTest,
    "GamePlatform.Combat.Projectile.Flight.SpeedPolicies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSpeedPolicyTest::RunTest(const FString&)
{
#if GP_HAS_PROJECTILE_FLIGHT
    FGamePlatformCombatProjectileFlightConfig C; C.bEnabled=true;
    C.SpeedMode=EGamePlatformProjectileSpeedMode::AccelerateThenCruise;
    TestTrue(TEXT("1200/4000/1800/0.15配置有效"),C.IsValid());
    TestEqual(TEXT("初速"),C.SpeedAt(0),1200.f);
    TestEqual(TEXT("75毫秒"),C.SpeedAt(.075f),1500.f);
    TestEqual(TEXT("150毫秒到顶"),C.SpeedAt(.15f),1800.f);
    TestEqual(TEXT("巡航不再加速"),C.SpeedAt(1),1800.f);
    TestTrue(TEXT("积分覆盖加速与巡航"),FMath::Abs(C.DistanceBetween(0,.3f)-495.f)<.05f);
    C.SpeedMode=EGamePlatformProjectileSpeedMode::ConstantSpeed;
    TestEqual(TEXT("常速不叠加推进"),C.SpeedAt(1),1200.f);
    C.SpeedMode=EGamePlatformProjectileSpeedMode::ConstantAcceleration;
    TestEqual(TEXT("恒加速受最大速度限制"),C.SpeedAt(5),1800.f);
    C.SpeedMode=EGamePlatformProjectileSpeedMode::Decelerate; C.MinSpeed=200; C.DecelerationDuration=1;
    TestEqual(TEXT("减速到下限不倒飞"),C.SpeedAt(10),200.f);
    return true;
#else
    AddError(TEXT("缺少通用飞行速度策略；现有运动只支持固定SpeedCmPerSecond。")); return false;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSpeedFrameTest,
    "GamePlatform.Combat.Projectile.Flight.FrameStepConsistency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSpeedFrameTest::RunTest(const FString&)
{
#if GP_HAS_PROJECTILE_FLIGHT
    for (int32 Hz : {30,60,120})
    {
        FProjectileTestWorld F; if(!F.Initialize())return false;
        FGamePlatformCombatProjectileMotion M; M.RangeCm=10000; M.Flight.bEnabled=true;
        M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::AccelerateThenCruise;
        auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
        TestTrue(TEXT("加速发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
        for(int32 I=0;I<Hz;++I)P->Tick(1.f/Hz);
        TestTrue(FString::Printf(TEXT("%dHz一秒位移1755cm"),Hz),FMath::Abs(P->GetActorLocation().Y-1755)<.15);
        TestTrue(TEXT("最大速度1800"),FMath::Abs(P->GetVelocity().Size()-1800)<.1);
        TestFalse(TEXT("发射后禁止改配置"),P->AuthoritySetFlightConfig(M.Flight));
    }
    return true;
#else
    AddError(TEXT("缺少按时间积分的加速度运动。"));return false;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSpeedGravityTest,
    "GamePlatform.Combat.Projectile.Flight.GravityAndAuthorityImpact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSpeedGravityTest::RunTest(const FString&)
{
#if GP_HAS_PROJECTILE_FLIGHT
    FProjectileTestWorld F;if(!F.Initialize())return false;
    FGamePlatformCombatProjectileMotion M;M.RangeCm=10000;M.Flight.bEnabled=true;
    M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::ConstantSpeed;
    M.Flight.InitialSpeed=1000;M.Flight.MaxSpeed=2000;M.Flight.GravityScale=1;M.Flight.bEnableHoming=false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("抛物线发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
    P->Tick(.1f);
    TestTrue(TEXT("重力未被推进速度重置"),P->GetActorLocation().Z < -4 && P->GetActorLocation().Y>99);
    M.Flight.GravityScale=0;M.Flight.bEnableHoming=true;M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::AccelerateThenCruise;
    M.Flight.InitialSpeed=1200;M.Flight.MaxSpeed=1800;M.RangeCm=1000;
    auto* Shot=F.World->SpawnActor<AGamePlatformCombatProjectile>();auto S=F.CastSpec();S.Target=F.Target;
    S.TargetAvatarGeneration=F.TargetCombat->GetCombatAvatarGeneration();
    TestTrue(TEXT("加速仍追踪已选目标"),Shot->AuthorityInitialize(S,M,FVector::ForwardVector));
    Shot->Tick(.25f);Shot->Tick(.25f);Shot->AuthorityCancel();
    TestTrue(TEXT("Sweep确认命中"),Shot->GetFlightFacts().bImpacted);
    TestEqual(TEXT("只结算一次"),F.TargetCombat->GetCombatHealth(),88.f);
    return true;
#else
    AddError(TEXT("缺少推进与重力分离的统一运动积分。"));return false;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSpeedValidationTest,
    "GamePlatform.Combat.Projectile.Flight.ValidationAndCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSpeedValidationTest::RunTest(const FString&)
{
#if GP_HAS_PROJECTILE_FLIGHT
    FGamePlatformCombatProjectileFlightConfig C;C.bEnabled=true;C.MaxSpeed=0;
    TestFalse(TEXT("0不是无限速"),C.IsValid());C.MaxSpeed=1800;C.AccelerationDuration=-1;
    TestFalse(TEXT("负时长拒绝"),C.IsValid());C.AccelerationDuration=.15f;
    C.SpeedMode=EGamePlatformProjectileSpeedMode::SpeedCurve;
    TestFalse(TEXT("空曲线拒绝"),C.IsValid());
    auto Curve=TStrongObjectPtr<UCurveFloat>(NewObject<UCurveFloat>());
    Curve->FloatCurve.AddKey(0,0);Curve->FloatCurve.AddKey(1,1);C.SpeedCurve=Curve.Get();C.CurveDuration=.15f;C.MinSpeed=1200;
    TestTrue(TEXT("归一化CurveFloat有效"),C.IsValid());
    TestTrue(TEXT("曲线不叠加恒推进"),FMath::Abs(C.SpeedAt(.075f)-1500)<.1f);
    TestTrue(TEXT("曲线积分"),FMath::Abs(C.DistanceBetween(0,.3f)-495)<.1f);
    C.InitialSpeed=NAN;TestFalse(TEXT("非有限输入拒绝"),C.IsValid());
    C.InitialSpeed=1200;
    const auto BadKey=Curve->FloatCurve.AddKey(.5f,.5f);
    Curve->FloatCurve.SetKeyInterpMode(BadKey,RCIM_Cubic);
    Curve->FloatCurve.GetKey(BadKey).LeaveTangent=NAN;
    TestFalse(TEXT("关键帧值正常但切线非有限也拒绝"),C.IsValid());
    return true;
#else
    AddError(TEXT("缺少Profile校验与CurveFloat策略。"));return false;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileCurveFreezeTest,
    "GamePlatform.Combat.Projectile.Flight.CurveSnapshotAndFrameSteps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileCurveFreezeTest::RunTest(const FString&)
{
    for(int32 Hz:{30,60,120})
    {
        FProjectileTestWorld F;if(!F.Initialize())return false;
        auto* Profile=NewObject<UGamePlatformCombatProjectileFlightProfile>();
        auto* Curve=NewObject<UCurveFloat>(Profile);
        Curve->FloatCurve.AddKey(0,0);Curve->FloatCurve.AddKey(.5f,.75f);Curve->FloatCurve.AddKey(1,1);
        auto& C=Profile->Flight;C.bEnabled=true;C.SpeedMode=EGamePlatformProjectileSpeedMode::SpeedCurve;
        C.MinSpeed=1200;C.CurveDuration=.15f;C.SpeedCurve=Curve;
        FGamePlatformCombatProjectileMotion M;M.RangeCm=10000;M.FlightProfile=Profile;
        auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
        TestTrue(TEXT("共享曲线Profile发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
        // 发射后修改模板和曲线，已发射Actor仍消费冻结快照。
        Curve->FloatCurve.Reset();Curve->FloatCurve.AddKey(0,0);Curve->FloatCurve.AddKey(1,0);C.MaxSpeed=5000;
        for(int32 I=0;I<Hz;++I)P->Tick(1.f/Hz);
        TestTrue(FString::Printf(TEXT("%dHz非线性曲线积分1766.25cm"),Hz),FMath::Abs(P->GetActorLocation().Y-1766.25)<.2);
        const auto S=P->GetMotionSnapshot_Implementation();
        TestTrue(TEXT("快照保留冻结巡航上限"),S.bValid && S.Speed==1800 && S.MaxSpeed==1800 && S.FlightPhase==2);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileDecelLimitTest,
    "GamePlatform.Combat.Projectile.Flight.DecelerationAndLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileDecelLimitTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    FGamePlatformCombatProjectileMotion M;M.RangeCm=10000;M.Flight.bEnabled=true;
    auto& C=M.Flight;C.SpeedMode=EGamePlatformProjectileSpeedMode::Decelerate;
    C.InitialSpeed=600;C.Deceleration=600;C.DecelerationDuration=1;C.MaxFlightTime=.4f;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("减速发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
    P->Tick(.25f);P->Tick(.25f);
    TestTrue(TEXT("寿命边界192cm，无虚假命中"),P->IsFlightFinished() && !P->GetFlightFacts().bImpacted && FMath::Abs(P->GetActorLocation().Y-192)<.1);
    C.SpeedMode=EGamePlatformProjectileSpeedMode::ConstantSpeed;C.InitialSpeed=1500;C.MaxFlightTime=10;C.MaxFlightDistance=100;
    auto* Short=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("有限距离发射"),Short->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));Short->Tick(.2f);
    TestTrue(TEXT("最大距离100cm停止"),Short->IsFlightFinished() && FMath::Abs(Short->GetActorLocation().Y-100)<.1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileLaunchReentryTest,
    "GamePlatform.Combat.Projectile.Flight.LaunchCancellationReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileLaunchReentryTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    FScriptDelegate Cancel;Cancel.BindUFunction(P,TEXT("AuthorityCancel"));
    P->OnFlightLaunched.Add(Cancel);P->OnFlightStopped.Add(Cancel);
    FGamePlatformCombatProjectileMotion M;M.Flight.bEnabled=true;
    TestTrue(TEXT("真实蓝图事件在发射回调中取消"),P->AuthorityInitialize(F.CastSpec(),M,FVector::ForwardVector));
    P->Tick(.25f);P->AuthorityCancel();
    TestTrue(TEXT("终态不可复活或移动"),P->IsFlightFinished() && P->GetActorLocation().IsNearlyZero() && P->GetVelocity().IsNearlyZero());
    TestEqual(TEXT("重入不造成伤害"),F.TargetCombat->GetCombatHealth(),100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileRotationFlagTest,
    "GamePlatform.Combat.Projectile.Flight.OptionalRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileRotationFlagTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();P->SetActorRotation(FRotator(0,45,0));
    FGamePlatformCombatProjectileMotion M;M.Flight.bEnabled=true;M.Flight.bRotationFollowsVelocity=false;
    TestTrue(TEXT("关闭朝向跟随仍可发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));P->Tick(.05f);
    TestTrue(TEXT("位置运动但不改写指定旋转"),P->GetActorLocation().Y>0 && FMath::Abs(P->GetActorRotation().Yaw-45)<.01);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSteeringSpeedTest,
    "GamePlatform.Combat.Projectile.Flight.PropulsionAndSteering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSteeringSpeedTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    F.Target->SetActorLocation(FVector(1000,200,0));
    FGamePlatformCombatProjectileMotion M;M.RangeCm=3000;M.Flight.bEnabled=true;
    M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::ConstantAcceleration;M.Flight.HomingAcceleration=6000;
    auto S=F.CastSpec();S.Target=F.Target;S.TargetAvatarGeneration=F.TargetCombat->GetCombatAvatarGeneration();
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    TestTrue(TEXT("推进加速度与有限转向共用唯一运动链"),P->AuthorityInitialize(S,M,FVector::RightVector));
    for(int32 I=0;I<400 && !P->IsFlightFinished();++I)
    {
        if(I==40)F.Target->SetActorLocation(FVector(1100,300,0));
        P->Tick(.01f);
        if(P->GetVelocity().Size()>1800.1){AddError(TEXT("追踪不能叠加越过最大速度"));return false;}
    }
    TestTrue(TEXT("有限转向追上移动中的原目标"),P->GetFlightFacts().bImpacted);
    TestEqual(TEXT("真实Sweep只结算一次"),F.TargetCombat->GetCombatHealth(),88.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileDecelReentryTest,
    "GamePlatform.Combat.Projectile.Flight.DecelerationCancellationReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileDecelReentryTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    FScriptDelegate Cancel;Cancel.BindUFunction(P,TEXT("AuthorityCancel"));P->OnDecelerationStarted.Add(Cancel);
    FScriptDelegate Destroy;Destroy.BindUFunction(P,TEXT("K2_DestroyActor"));P->OnMaxSpeedReached.Add(Destroy);
    FGamePlatformCombatProjectileMotion M;M.Flight.bEnabled=true;
    M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::Decelerate;M.Flight.InitialSpeed=M.Flight.MaxSpeed;
    TestTrue(TEXT("减速发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
    TestTrue(TEXT("减速回调已取消"),P->IsFlightFinished());
    TestFalse(TEXT("取消后不能继续触发最高速事件"),P->IsActorBeingDestroyed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileDecelLifetimeTest,
    "GamePlatform.Combat.Projectile.Flight.ExplicitDecelerationLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileDecelLifetimeTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    FGamePlatformCombatProjectileMotion M;M.RangeCm=4000;M.Flight.bEnabled=true;
    M.Flight.SpeedMode=EGamePlatformProjectileSpeedMode::Decelerate;
    M.Flight.InitialSpeed=1800;M.Flight.MaxSpeed=1800;M.Flight.MinSpeed=200;
    M.Flight.Deceleration=3200;M.Flight.DecelerationDuration=.5f;M.Flight.MaxFlightTime=5;
    TestTrue(TEXT("减速寿命发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::RightVector));
    for(int32 I=0;I<30;++I)P->Tick(.1f);
    TestFalse(TEXT("三秒时未达显式寿命或距离，不被旧常速期限截断"),P->IsFlightFinished());
    TestTrue(TEXT("减速积分1000cm"),FMath::Abs(P->GetActorLocation().Y-1000)<.2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileHelixVelocityTest,
    "GamePlatform.Combat.Projectile.Flight.HelixTotalVelocity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileHelixVelocityTest::RunTest(const FString&)
{
    FProjectileTestWorld F;if(!F.Initialize())return false;
    auto* P=F.World->SpawnActor<AGamePlatformCombatProjectile>();
    FGamePlatformCombatProjectileMotion M;M.RangeCm=4000;M.HelixRadiusCm=100;M.HelixTurnsPerSecond=1;M.Flight.bEnabled=true;
    TestTrue(TEXT("螺旋Profile发射"),P->AuthorityInitialize(F.CastSpec(),M,FVector::ForwardVector));
    P->Tick(1.f/120);
    const FVector V=P->GetVelocity();const auto S=P->GetMotionSnapshot_Implementation();
    TestTrue(TEXT("总速度包含真实横向分量"),V.Size()>1300 && FMath::Abs(V.Z)>600);
    TestTrue(TEXT("总速仍受上限约束"),V.Size()<=1800.1);
    TestTrue(TEXT("复制/快照/朝向一致"),FMath::Abs(S.Speed-V.Size())<.1 && FVector::DotProduct(P->GetActorForwardVector(),V.GetSafeNormal())>.999);
    return true;
}
#endif
