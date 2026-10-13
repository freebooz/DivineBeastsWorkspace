// 平台权威投射物回归：真实World/碰撞/ASC/战斗组件，无项目、MOBA、Niagara或网络替身依赖。
// RED阶段曾显式报告缺少生产类型；类型加入后直接编译真实路径，验证曲线运动、伤害与过期来源拒绝。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
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
    bool Initialize()
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        if (!World.IsValid()) return false;
        World->InitializeActorsForPlay(FURL());
        const auto SpawnCombatant = [this](FVector Position, UGamePlatformCombatComponent*& OutCombat)
        {
            AActor* Actor = World->SpawnActor<AActor>();
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
        Source = SpawnCombatant(FVector(-100, 0, 0), SourceCombat);
        Target = SpawnCombatant(FVector(200, 0, 0), TargetCombat);
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
#endif
