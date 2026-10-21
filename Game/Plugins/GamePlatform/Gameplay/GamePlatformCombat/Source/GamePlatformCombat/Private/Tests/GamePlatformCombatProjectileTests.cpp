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
    TestTrue(TEXT("交接后继续逐步转向原目标，不能不追踪或直接吸附目标方向"),
        Projectile->GetActorLocation().Y > 1 && Projectile->GetActorLocation().Y < InstantAimY);
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
// 微小偏角也必须保留起飞切线；不能在首个运动子步直接吸附到目标方向。
// 覆盖近/中/远距离与最大合法射程，距离不应决定锁定弹体是否退化为直射。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformProjectileSmallAngleArcTest,
    "GamePlatform.Combat.Projectile.SmallAngleArcAtEveryDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformProjectileSmallAngleArcTest::RunTest(const FString&)
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
        TestTrue(FString::Printf(TEXT("%.0f厘米的5度偏角仍先起飞再弧形转向，实测偏离%.4f厘米"), Distance, MaxDeviation),
            MaxDeviation > Distance * .002f);
        // 同一距离真正朝向目标时维持直线；不能靠对全部目标强加抛物线使上面的断言通过。
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
    TestTrue(TEXT("15厘米子步内也不能吸附为目标方向"),
        FVector::CrossProduct(FastProjectile->GetActorLocation(), CloseDirection).Size() > .05);
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
#endif
