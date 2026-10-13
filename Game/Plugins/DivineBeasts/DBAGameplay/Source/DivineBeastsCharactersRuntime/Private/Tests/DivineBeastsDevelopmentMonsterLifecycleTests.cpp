// 项目开发怪物独立生命周期回归：真实原生Actor/ASC/Combat，验证开发拒绝、退出及生命通知同步重入。
// 仅游戏线程瞬态世界夹具；不修改原有三个反射RED用例，不读地图/资产，不启动服务或伪造战斗事件。
// 入口要求真实开发Config和-DBADevelopmentMonsters；负例只短暂修改GConfig内存值并原样恢复，不写配置文件。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Types/GamePlatformCombatSpec.h"
#include "UObject/StrongObjectPtr.h"

namespace DivineBeastsDevelopmentMonsterLifecycleTests
{
/** 夹具独占World及Engine WorldContext；来源仍从真实SourceCombat.ApplyDamage进入唯一战斗结算。 */
struct FMonsterLifecycleFixture
{
    TStrongObjectPtr<UWorld> World;
    ADivineBeastsDevelopmentMonster* Monster = nullptr;
    AActor* Source = nullptr;
    UGamePlatformCombatComponent* SourceCombat = nullptr;
    bool bWorldContextCreated = false;

    /** 无正式GameInstance/地图/准入；显式初始化Actor，使生产角色的BeginPlay及组件边界真实执行。 */
    bool Create(FAutomationTestBase& Test)
    {
        if (!Test.TestTrue(TEXT("独立用例须使用已授权开发入口"),
            ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess()) ||
            !Test.TestNotNull(TEXT("Engine WorldContext注册入口存在"), GEngine)) return false;
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("夹具拥有真实瞬态Game世界"), World.Get())) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
        bWorldContextCreated = true;
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Monster = World->SpawnActor<ADivineBeastsDevelopmentMonster>(FVector::ZeroVector,
            FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("实际开发怪物Actor"), Monster)) return false;
        Monster->DispatchBeginPlay();
        Source = World->SpawnActor<AActor>();
        if (!Test.TestNotNull(TEXT("真实独立伤害来源"), Source)) return false;
        auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Source);
        SourceCombat = NewObject<UGamePlatformCombatComponent>(Source);
        Source->AddInstanceComponent(ASC);
        Source->AddInstanceComponent(SourceCombat);
        ASC->RegisterComponent();
        SourceCombat->RegisterComponent();
        if (!Test.TestTrue(TEXT("来源真实ASC绑定成功"), ASC->BindAbilityActorInfo(Source, Source))) return false;
        Source->DispatchBeginPlay();
        return Test.TestTrue(TEXT("真实来源Combat已进入BeginPlay"),
            SourceCombat->HasBegunPlay() && SourceCombat->GetCombatAttributeSet() != nullptr);
    }

    /** 正代次目标Spec；每次事件身份独立，不依赖零代次自动补全或平台投射物行为。 */
    FGamePlatformCombatResult Damage(float Magnitude) const
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid();
        Spec.Source = Source;
        Spec.Target = Monster;
        Spec.Magnitude = Magnitude;
        Spec.DamageType = EGamePlatformDamageType::Magic;
        Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
        Spec.TargetAvatarGeneration = Monster->GetCombatAvatarGeneration();
        return SourceCombat->ApplyDamage(Spec);
    }

    /** 所有成功/失败路径均路由真实EndPlay；只释放自有世界及WorldContext。 */
    ~FMonsterLifecycleFixture()
    {
        if (!World.IsValid()) return;
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        if (bWorldContextCreated && GEngine) GEngine->DestroyWorldContext(World.Get());
    }
};

/** 只恢复一个非秘密开发键的原始内存文本；从不Flush或保存Config，不影响CLI和其他字段。 */
struct FScopedDevelopmentConfigRejection
{
    FString OriginalValue;
    bool bCaptured = false;
    FScopedDevelopmentConfigRejection()
    {
        bCaptured = GConfig && GConfig->GetString(TEXT("DivineBeasts.Development"),
            TEXT("bAllowDevelopmentMonsters"), OriginalValue, GGameIni);
        if (bCaptured) GConfig->SetBool(TEXT("DivineBeasts.Development"),
            TEXT("bAllowDevelopmentMonsters"), false, GGameIni);
    }
    ~FScopedDevelopmentConfigRejection()
    {
        if (bCaptured && GConfig) GConfig->SetString(TEXT("DivineBeasts.Development"),
            TEXT("bAllowDevelopmentMonsters"), *OriginalValue, GGameIni);
    }
};
}

// 缺开发配置必须拒绝初始化/受伤；恢复许可后真实初始化，Actor退出后所有命令拒绝且不再通知。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterGuardAndExitTest,
    "DivineBeasts.Character.DevelopmentMonster.DevelopmentGuardAndExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterGuardAndExitTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterLifecycleTests;
    FMonsterLifecycleFixture Fixture;
    if (!Fixture.Create(*this)) return false;
    {
        FScopedDevelopmentConfigRejection Config;
        if (!TestTrue(TEXT("开发负例已捕获可恢复的原始配置值"), Config.bCaptured)) return false;
        TestFalse(TEXT("唯一公开守卫拒绝配置关闭"),
            ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess());
        TestFalse(TEXT("开发关闭不得初始化生命"), Fixture.Monster->AuthorityInitializeDevelopmentMonster());
        TestFalse(TEXT("开发关闭不得接纳Combat"), Fixture.Monster->CanReceiveCombat());
        TestEqual(TEXT("失败初始化不发布有效生命代次"), Fixture.Monster->GetLifeState().AvatarGeneration, 0);
    }
    TestTrue(TEXT("配置原样恢复后守卫重新允许"),
        ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess());
    if (!TestTrue(TEXT("恢复开发许可后真实初始化成功"),
        Fixture.Monster->AuthorityInitializeDevelopmentMonster())) return false;
    int32 Notifications = 0;
    const auto Handle = Fixture.Monster->OnLifeStateChanged().AddLambda(
        [&Notifications](const FDivineBeastsDevelopmentMonsterLifeState&) { ++Notifications; });
    const int32 Generation = Fixture.Monster->GetLifeState().AvatarGeneration;
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepMonster(Fixture.Monster);
    TestTrue(TEXT("自有怪物销毁路由真实EndPlay"), Fixture.Monster->Destroy());
    TestFalse(TEXT("退出后初始化必须拒绝"), Fixture.Monster->AuthorityInitializeDevelopmentMonster());
    TestFalse(TEXT("退出后匹配代次重生也必须拒绝"),
        Fixture.Monster->AuthorityRespawnDevelopmentMonster(Generation));
    TestFalse(TEXT("退出后受伤资格必须失效"), Fixture.Monster->CanReceiveCombat());
    TestEqual(TEXT("退出后命令不再发布生命事件"), Notifications, 0);
    Fixture.Monster->OnLifeStateChanged().Remove(Handle);
    return true;
}

// 第一订阅者在270生命通知中同步提交真实致死伤害；当前const&必须仍为270，最终投影不能尾写回存活。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterLifeReentryTest,
    "DivineBeasts.Character.DevelopmentMonster.LifeStateReentrySnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterLifeReentryTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterLifecycleTests;
    FMonsterLifecycleFixture Fixture;
    if (!Fixture.Create(*this) || !TestTrue(TEXT("重入夹具初始化真实生命"),
        Fixture.Monster->AuthorityInitializeDevelopmentMonster())) return false;
    const int32 Generation = Fixture.Monster->GetLifeState().AvatarGeneration;
    bool bNestedDamageEntered = false;
    int32 DeathNotifications = 0;
    const TWeakObjectPtr<ADivineBeastsDevelopmentMonster> WeakMonster(Fixture.Monster);
    const auto Handle = Fixture.Monster->OnLifeStateChanged().AddLambda(
        [this, &Fixture, WeakMonster, &bNestedDamageEntered, &DeathNotifications, Generation]
        (const FDivineBeastsDevelopmentMonsterLifeState& State)
        {
            if (!WeakMonster.IsValid()) return;
            if (State.bDead)
            {
                ++DeathNotifications;
                TestFalse(TEXT("通知同步栈中不得重置唯一Combat生命"),
                    WeakMonster->AuthorityRespawnDevelopmentMonster(State.AvatarGeneration));
                return;
            }
            if (bNestedDamageEntered || !FMath::IsNearlyEqual(State.Health, 270.0f)) return;
            bNestedDamageEntered = true;
            const FDivineBeastsDevelopmentMonsterLifeState Before = State;
            const auto Result = Fixture.Damage(1000.0f);
            TestTrue(TEXT("同步后继伤害经过真实Combat.ApplyDamage"), Result.IsSuccess());
            TestEqual(TEXT("后继死亡不能修改本次通知Health"), State.Health, Before.Health);
            TestEqual(TEXT("后继死亡不能修改本次通知bDead"), State.bDead, Before.bDead);
            TestEqual(TEXT("后继事件不能修改本次通知代次"), State.AvatarGeneration, Generation);
        });
    const auto InitialDamage = Fixture.Damage(30.0f);
    TestTrue(TEXT("第一条真实伤害结算成功"), InitialDamage.IsSuccess());
    TestTrue(TEXT("生命监听确实进入同步后继伤害"), bNestedDamageEntered);
    const auto Final = Fixture.Monster->GetLifeState();
    TestTrue(TEXT("最终投影保持Combat已提交死亡"), Final.bDead);
    TestEqual(TEXT("最终投影保持零生命"), Final.Health, 0.0f);
    TestEqual(TEXT("死亡通知仅一次"), DeathNotifications, 1);
    TestEqual(TEXT("受伤死亡没有伪造推进代次"), Final.AvatarGeneration, Generation);
    TestTrue(TEXT("监听退栈后匹配死亡代次才允许真实重生"),
        Fixture.Monster->AuthorityRespawnDevelopmentMonster(Generation));
    TestTrue(TEXT("真实重生推进Combat代次"), Fixture.Monster->GetLifeState().AvatarGeneration > Generation);
    Fixture.Monster->OnLifeStateChanged().Remove(Handle);
    return true;
}

#endif
