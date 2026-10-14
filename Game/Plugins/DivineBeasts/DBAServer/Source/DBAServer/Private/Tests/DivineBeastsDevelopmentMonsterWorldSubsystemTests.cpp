// 项目服务器开发怪物内部回归：真实瞬态World/物理查询验证开发门禁、安全落点及自有资源退出清理。
// 私有friend只允许本文件检查装配所有权，不提供生产绕过入口；不启动专服/HTTP，不使用或保存真实地图资产。
// 清理夹具显式建立自有Actor/Timer，不能将其视为Village Tutorial完整控制面/体验集成通过的证据。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsDevelopmentMonsterWorldSubsystem.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "UObject/StrongObjectPtr.h"

namespace DivineBeastsDevelopmentMonsterWorldTests
{
/** 夹具拥有实际Game世界和Engine上下文；仅碰撞用例建立原生物理Scene，无AI/导航或资源加载。 */
struct FWorldFixture
{
    TStrongObjectPtr<UWorld> World;
    bool bWorldContextCreated = false;

    bool Create(FAutomationTestBase& Test, bool bPhysics)
    {
        if (!Test.TestTrue(TEXT("服务器内部测试使用显式开发入口"),
            ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess()) ||
            !Test.TestNotNull(TEXT("Engine上下文注册入口存在"), GEngine)) return false;
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(bPhysics).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("实际Game世界"), World.Get())) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
        bWorldContextCreated = true;
        World->InitializeActorsForPlay(FURL());
        return true;
    }

    /** 无Mesh/材质/地图的原生测试碰撞盒；参数厘米，Visibility可忽略以独立验证物体通道胶囊占位。 */
    AActor* AddCollisionBox(FVector Center, FVector Extent, bool bBlocksVisibility) const
    {
        AActor* Actor = World->SpawnActor<AActor>();
        if (!Actor) return nullptr;
        auto* Box = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Box);
        Actor->SetRootComponent(Box);
        Box->SetMobility(EComponentMobility::Static);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->SetCollisionResponseToChannel(ECC_Visibility, bBlocksVisibility ? ECR_Block : ECR_Ignore);
        Box->SetWorldLocation(Center);
        Box->RegisterComponent();
        Actor->DispatchBeginPlay();
        return Actor;
    }

    /** 先路由退出再注销Engine上下文；所有资源只存在于本夹具世界。 */
    ~FWorldFixture()
    {
        if (!World.IsValid()) return;
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        if (bWorldContextCreated && GEngine) GEngine->DestroyWorldContext(World.Get());
    }
};
}

// 正式Shared角色/体验必须精确匹配；普通Editor Game及EditorPreview不能凭开发开关自动生成。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterWorldGateTest,
    "DivineBeasts.Server.DevelopmentMonster.WorldAndDevelopmentGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterWorldGateTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterWorldTests;
    FWorldFixture Fixture;
    if (!Fixture.Create(*this, false)) return false;
    const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterWorldSubsystem> Subsystem(
        NewObject<UDivineBeastsDevelopmentMonsterWorldSubsystem>(Fixture.World.Get()));
    FGamePlatformId Tutorial;
    FGamePlatformId Training;
    if (!TestTrue(TEXT("Shared Tutorial转换成平台体验身份"), FGamePlatformId::TryParse(
        FDivineBeastsProjectCatalog::GetVillageTutorialExperience().ToString() + TEXT("@1"), Tutorial)) ||
        !TestTrue(TEXT("Shared Training转换成平台体验身份"), FGamePlatformId::TryParse(
        FDivineBeastsProjectCatalog::GetVillageTrainingExperience().ToString() + TEXT("@1"), Training))) return false;
    TestTrue(TEXT("唯一允许Village/Tutorial身份组合"), Subsystem->IsVillageTutorialIdentity(
        FDivineBeastsProjectCatalog::GetVillageServerRole(), Tutorial));
    TestFalse(TEXT("Village/Training不是此轮怪物入口"), Subsystem->IsVillageTutorialIdentity(
        FDivineBeastsProjectCatalog::GetVillageServerRole(), Training));
    TestFalse(TEXT("OpenWorld不能借用Tutorial体验生成"), Subsystem->IsVillageTutorialIdentity(
        FDivineBeastsProjectCatalog::GetOpenWorldServerRole(), Tutorial));
    TestFalse(TEXT("无有效体验不得授予入口"), Subsystem->IsVillageTutorialIdentity(
        FDivineBeastsProjectCatalog::GetVillageServerRole(), FGamePlatformId()));
    TestEqual(TEXT("普通测试世界实际是Standalone而非专服"), Fixture.World->GetNetMode(), NM_Standalone);
    TestFalse(TEXT("开发开关不足以在Standalone创建服务器装配"), Subsystem->ShouldCreateSubsystem(Fixture.World.Get()));
    const TStrongObjectPtr<UWorld> Preview(UWorld::CreateWorld(EWorldType::EditorPreview, false));
    if (!TestNotNull(TEXT("真实EditorPreview世界"), Preview.Get())) return false;
    TestTrue(TEXT("支持实际Game世界类型"), Subsystem->DoesSupportWorldType(EWorldType::Game));
    TestTrue(TEXT("支持实际PIE世界类型"), Subsystem->DoesSupportWorldType(EWorldType::PIE));
    TestFalse(TEXT("明确排除EditorPreview世界类型"), Subsystem->DoesSupportWorldType(EWorldType::EditorPreview));
    TestFalse(TEXT("编辑器预览永远不支持服务器装配"), Subsystem->ShouldCreateSubsystem(Preview.Get()));
    Preview->DestroyWorld(false);
    UDivineBeastsDevelopmentMonsterWorldSubsystem::FApprovedWorldScope Scope;
    Subsystem->WorldOperationId = FGuid::NewGuid();
    TestFalse(TEXT("缺实际Bootstrap/Ready/Experience的世界不能生成"), Subsystem->CaptureApprovedWorldScope(Scope));
    Subsystem->TryCreateOwnedMonster();
    TestNull(TEXT("门禁失败不留下怪物"), Subsystem->OwnedMonster.Get());
    Subsystem->Deinitialize();
    return true;
}

// 世界退出只撤自有Timer/Actor，迟到Ticket失效，重复清理幂等；外部Actor/Timer不能被顺手清除。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterWorldExitTest,
    "DivineBeasts.Server.DevelopmentMonster.OwnedTimerAndWorldExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterWorldExitTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterWorldTests;
    FWorldFixture Fixture;
    if (!Fixture.Create(*this, false)) return false;
    const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterWorldSubsystem> Subsystem(
        NewObject<UDivineBeastsDevelopmentMonsterWorldSubsystem>(Fixture.World.Get()));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> Monster(
        Fixture.World->SpawnActor<ADivineBeastsDevelopmentMonster>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn));
    AActor* ExternalActor = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("退出夹具真实怪物"), Monster.Get()) ||
        !TestNotNull(TEXT("退出夹具外部Actor"), ExternalActor)) return false;
    Monster->DispatchBeginPlay();
    ExternalActor->DispatchBeginPlay();
    if (!TestTrue(TEXT("退出夹具怪物真实初始化"), Monster->AuthorityInitializeDevelopmentMonster())) return false;
    // 仅建立待清理的所有权，不伪造实际Server Profile或Experience资格，不调用伪造Death事件。
    Subsystem->OwnedMonster = Monster.Get();
    Subsystem->WorldOperationId = FGuid::NewGuid();
    const FGuid Ticket = FGuid::NewGuid();
    const int32 Generation = Monster->GetLifeState().AvatarGeneration;
    Subsystem->RespawnOperationId = Ticket;
    Subsystem->PendingRespawnGeneration = Generation;
    Subsystem->DeathSubscription = Monster->OnAuthoritativeDeath().AddUObject(
        Subsystem.Get(), &UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleOwnedMonsterDeath);
    Monster->OnDestroyed.AddDynamic(Subsystem.Get(),
        &UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleOwnedMonsterDestroyed);
    bool bOwnedTimerRan = false;
    bool bExternalTimerRan = false;
    auto& Timers = Fixture.World->GetTimerManager();
    Timers.SetTimer(Subsystem->RespawnTimer, FTimerDelegate::CreateLambda([&bOwnedTimerRan]
        { bOwnedTimerRan = true; }), 10.0f, false);
    const FTimerHandle OwnedTimer = Subsystem->RespawnTimer;
    FTimerHandle ExternalTimer;
    Timers.SetTimer(ExternalTimer, FTimerDelegate::CreateLambda([&bExternalTimerRan]
        { bExternalTimerRan = true; }), 10.0f, false);
    TestTrue(TEXT("自有Timer真实登记"), Timers.TimerExists(OwnedTimer));
    TestTrue(TEXT("外部Timer真实登记"), Timers.TimerExists(ExternalTimer));
    Subsystem->OnWorldEndPlay(*Fixture.World.Get());
    TestTrue(TEXT("退出先永久封闭作用域"), Subsystem->bClosing);
    TestFalse(TEXT("退出撤销World操作身份"), Subsystem->WorldOperationId.IsValid());
    TestFalse(TEXT("退出撤销重生Ticket"), Subsystem->RespawnOperationId.IsValid());
    TestEqual(TEXT("退出清理死亡待重生代次"), Subsystem->PendingRespawnGeneration, 0);
    TestNull(TEXT("退出释放自有Actor引用"), Subsystem->OwnedMonster.Get());
    TestTrue(TEXT("退出销毁自有怪物Actor"), Monster->IsActorBeingDestroyed());
    TestFalse(TEXT("退出撤销自有Timer"), Timers.TimerExists(OwnedTimer));
    TestTrue(TEXT("退出保留外部Timer"), Timers.TimerExists(ExternalTimer));
    TestTrue(TEXT("退出保留外部Actor"), IsValid(ExternalActor) && !ExternalActor->IsActorBeingDestroyed());
    Subsystem->RespawnOwnedMonster(Ticket, Generation);
    Subsystem->OnWorldEndPlay(*Fixture.World.Get());
    Subsystem->Deinitialize();
    TestNull(TEXT("迟到Ticket和重复退出不能重建Actor"), Subsystem->OwnedMonster.Get());
    TestFalse(TEXT("夹具未Tick，不将未执行Timer冒充回调通过"), bOwnedTimerRan || bExternalTimerRan);
    Timers.ClearTimer(ExternalTimer);
    return true;
}

// 空世界拒绝固定坐标；原生地面允许胶囊；忽略Visibility的实际障碍仍须拒绝，PlayerStart始终不改。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterSpawnSafetyTest,
    "DivineBeasts.Server.DevelopmentMonster.SafeGroundAndPlayerStartPreservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterSpawnSafetyTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterWorldTests;
    FWorldFixture Fixture;
    if (!Fixture.Create(*this, true)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APlayerStart* Start = Fixture.World->SpawnActor<APlayerStart>(FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("夹具自有PlayerStart"), Start)) return false;
    Start->DispatchBeginPlay();
    const FTransform OriginalStart = Start->GetActorTransform();
    const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterWorldSubsystem> Subsystem(
        NewObject<UDivineBeastsDevelopmentMonsterWorldSubsystem>(Fixture.World.Get()));
    FTransform Candidate;
    TestFalse(TEXT("无真实地面必须拒绝安全落点"), Subsystem->FindSafeSpawnTransform(Candidate));
    if (!TestNotNull(TEXT("夹具原生地面碰撞盒"), Fixture.AddCollisionBox(
        FVector(600, 0, -20), FVector(1000, 500, 20), true))) return false;
    TestTrue(TEXT("真实可行走地面和空闲胶囊允许候选"), Subsystem->FindSafeSpawnTransform(Candidate));
    TestTrue(TEXT("候选在PlayerStart前方而非占用出生点"), Candidate.GetLocation().X >= 400.0f);
    if (!TestNotNull(TEXT("夹具原生占位障碍，Visibility忽略"), Fixture.AddCollisionBox(
        FVector(600, 0, 200), FVector(600, 450, 300), false))) return false;
    TestFalse(TEXT("胶囊物体通道查询拒绝全部实际占位候选"), Subsystem->FindSafeSpawnTransform(Candidate));
    TestTrue(TEXT("所有成功/失败查询保持PlayerStart原始Transform"),
        Start->GetActorTransform().Equals(OriginalStart));
    TestNull(TEXT("只读选址没有生成怪物Actor"), Subsystem->OwnedMonster.Get());
    Subsystem->Deinitialize();
    return true;
}

#endif
