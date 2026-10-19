// 平台权威投射物回归：真实World/碰撞/ASC/战斗组件，无项目、MOBA、Niagara或网络替身依赖。
// RED阶段曾显式报告缺少生产类型；类型加入后直接编译真实路径，验证曲线运动、伤害与过期来源拒绝。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
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
    bool Initialize(bool bUsePawnSource = false)
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
        Target = SpawnCombatant(FVector(200, 0, 0), TargetCombat, false);
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
    TestTrue(TEXT("交接后弹体继续转向原目标的新位置"), Projectile->GetActorLocation().Y > 20);
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
    auto* Sphere=NewObject<USphereComponent>(Wall);Sphere->InitSphereRadius(35);
    Wall->SetRootComponent(Sphere);Wall->AddInstanceComponent(Sphere);
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Sphere->SetCollisionResponseToAllChannels(ECR_Block);
    Sphere->RegisterComponent();Wall->SetActorLocation(FVector(100,100,0));Wall->DispatchBeginPlay();
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
#endif
