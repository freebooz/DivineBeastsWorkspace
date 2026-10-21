// 项目服务器资料边界回归：响应字符串仅测试夹具，不联网、不生成准入、不作为真实玩家资料。
// 验证选择/拥有/Active/Catalog/唯一性、死亡拒绝与世界作用域清理；生产PlayerId仍必须来自当前真实准入。
#if WITH_DEV_AUTOMATION_TESTS
#include "Characters/DivineBeastsWorldCharacterAdmission.h"
#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionRosterTest, "DivineBeasts.Server.WorldCharacterAdmission.Roster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionRosterTest::RunTest(const FString&)
{
    using namespace DivineBeasts::WorldCharacterAdmission;
    FString Selected = TEXT("stale"); FName Hero(TEXT("stale"));
    const FString Profile = TEXT(R"({"found":true,"playerId":"fixture-player","selectedCharacterId":"fixture-b","ownedCharacterIds":["fixture-a","fixture-b"]})");
    TestTrue(TEXT("可信玩家选择必须属于拥有列表"), ParseSelectedCharacter(Profile, TEXT("fixture-player"), Selected));
    TestEqual(TEXT("使用选择而非首角色"), Selected, FString(TEXT("fixture-b")));
    TestFalse(TEXT("拒绝错玩家响应"), ParseSelectedCharacter(Profile, TEXT("other"), Selected));
    TestTrue(TEXT("失败清空旧CharacterID"), Selected.IsEmpty());
    TestFalse(TEXT("缺少资料拒绝"), ParseSelectedCharacter(TEXT("{}"), TEXT("fixture-player"), Selected));
    TestFalse(TEXT("未拥有选中角色拒绝"), ParseSelectedCharacter(TEXT(R"({"found":true,"playerId":"fixture-player","selectedCharacterId":"foreign","ownedCharacterIds":["fixture-a"]})"), TEXT("fixture-player"), Selected));
    const FString Roster = TEXT(R"({"characters":[{"characterId":"fixture-a","status":"Active","heroDefinitionId":"Hero.Zodiac.Rat"},{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Tiger"}]})");
    TestTrue(TEXT("选择角色映射到真实核心Hero"), ParseSelectedHero(Roster, TEXT("fixture-b"), Hero));
    TestEqual(TEXT("不是首英雄固定回退"), Hero, FName(TEXT("Hero.Zodiac.Tiger")));
    TestFalse(TEXT("角色不在该玩家Roster拒绝"), ParseSelectedHero(Roster, TEXT("foreign"), Hero));
    TestTrue(TEXT("失败清空旧Hero"), Hero.IsNone());
    TestFalse(TEXT("重复角色身份不能任意选一个"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Tiger"},{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Rat"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("Disabled拒绝"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Disabled","heroDefinitionId":"Hero.Zodiac.Tiger"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("未知Hero拒绝，不推算资产路径"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Unknown"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("损坏JSON拒绝"), ParseSelectedHero(TEXT("invalid-json"), TEXT("fixture-b"), Hero));
    return true;
}
// 使用真实Controller/Pawn/身份组件，无GameInstance数据服务是明确失败夹具；不注册或签发生产服务器准入。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionRequiredDefinitionTest, "DivineBeasts.Server.WorldCharacterAdmission.RequiredDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionRequiredDefinitionTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("真实瞬态世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Character = World->SpawnActor<ADivineBeastsCharacter>();
    if (!TestNotNull(TEXT("真实Controller"), Controller) || !TestNotNull(TEXT("真实项目角色"), Character)) return false;
    TestFalse(TEXT("未绑定Pawn不能激活"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    Controller->Possess(Character);
    auto* Identity = Character->FindComponentByClass<UDivineBeastsCharacterComponent>();
    FGamePlatformCharacterInitializationContext Context;
    Context.CharacterId = TEXT("required-definition-fixture"); Context.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Context.SpawnGeneration = 1; Context.AvatarGeneration = 1; FString Error;
    TestTrue(TEXT("身份受理仍可能异步初始化失败"), Identity->AuthorityBindTrustedContext(Context, Error));
    TestFalse(TEXT("缺真实Data服务不会伪装为Ready"), Identity->IsCharacterReady());
    TestFalse(TEXT("身份受理不等于允许Active"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    TestEqual(TEXT("缺服务保留真实失败诊断"), Identity->GetLastDefinitionLoadResult().Code, FName(TEXT("DataGameInstanceUnavailable")));
    Controller->UnPossess();
    TestFalse(TEXT("撤销Pawn后不能重新激活"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    return true;
}

// 直接消费Shared生成目录的体验映射；未知/竞技体验不得因名字含world或村庄而获得自动重生资格。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionRespawnRoleTest,
    "DivineBeasts.Server.WorldCharacterAdmission.RespawnRole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionRespawnRoleTest::RunTest(const FString&)
{
    using DivineBeasts::WorldCharacterAdmission::SupportsAutomaticRespawn;
    for (const TCHAR* Experience : {TEXT("experience.openworld.hub"), TEXT("experience.openworld.main"),
        TEXT("experience.lobby.main"), TEXT("experience.village.main"), TEXT("experience.village.tutorial"),
        TEXT("experience.village.training")})
        TestTrue(*FString::Printf(TEXT("共享目录常驻/村庄体验允许自动重生：%s"), Experience), SupportsAutomaticRespawn(FName(Experience)));
    TestFalse(TEXT("MainArena继续单局淘汰，不进入世界自动重生"), SupportsAutomaticRespawn(TEXT("experience.mainarena.main")));
    TestFalse(TEXT("未登记体验失败关闭"), SupportsAutomaticRespawn(TEXT("experience.village.unknown")));
    TestFalse(TEXT("空体验失败关闭"), SupportsAutomaticRespawn(NAME_None));
    return true;
}

// 私有桥的原生生命周期夹具：使用真实Controller/Pawn/Combat死亡及FTimerManager。
// 不签发生产准入、不伪造平台Active记录；显式植入计时器仅检验资源归属，成功出生另需真实Village联机验证。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionDeathLifecycleTest,
    "DivineBeasts.Server.WorldCharacterAdmission.DeathLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionDeathLifecycleTest::RunTest(const FString&)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("真实死亡生命周期世界"), World)) return false;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); };
    auto* Instance = NewObject<UGameInstance>(); World->SetGameInstance(Instance);
    auto* Admission = NewObject<UGamePlatformServerAdmissionSubsystem>(Instance);
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mode = World->SpawnActor<ADivineBeastsWorldGameMode>(Spawn);
    auto* Controller = World->SpawnActor<APlayerController>(Spawn);
    auto* Pawn = World->SpawnActor<ADivineBeastsCharacter>(Spawn);
    if (!TestNotNull(TEXT("真实项目Mode"), Mode) || !TestNotNull(TEXT("真实Controller"), Controller)
        || !TestNotNull(TEXT("真实死亡Pawn"), Pawn)) return false;
    Controller->Possess(Pawn); Pawn->DispatchBeginPlay();
    auto Bridge = MakeShared<FDivineBeastsWorldCharacterAdmission>(*Mode, *Admission);
    ON_SCOPE_EXIT { Bridge->Close(); };
    Bridge->ObserveAdmissions();
    auto Connection = MakeShared<FDivineBeastsWorldCharacterAdmission::FConnection>();
    Connection->OperationId = FGuid::NewGuid(); Connection->PawnOperationId = FGuid::NewGuid();
    Connection->BoundPawn = Pawn; Connection->CharacterIdentity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    Bridge->Connections.Add(Controller, Connection);
    Bridge->ObservePawnDeath(*Controller, Connection);
    const FDelegateHandle DeathHandle = Connection->DeathHandle;
    Bridge->ObservePawnDeath(*Controller, Connection);
    TestTrue(TEXT("重复绑定保持同一死亡订阅"), DeathHandle.IsValid() && Connection->DeathHandle == DeathHandle);
    auto* Combat = Pawn->GetGamePlatformCombatComponent();
    auto* Attributes = Combat ? Combat->GetCombatAttributeSet() : nullptr;
    if (!TestNotNull(TEXT("真实战斗属性"), Attributes)) return false;
    int32 DeathCount = 0; FGamePlatformCombatEvent Death;
    const auto FactHandle = Pawn->OnAuthoritativeDeath().AddLambda([&](const FGamePlatformCombatEvent& Event)
        { ++DeathCount; Death = Event; });
    FGameplayEffectSpec Effect; Combat->ResolveIncomingDamage(*Attributes, Effect, Attributes->GetHealth() + 1.0f);
    Pawn->OnAuthoritativeDeath().Remove(FactHandle);
    TestEqual(TEXT("真实伤害结算发布一次死亡"), DeathCount, 1);
    TestTrue(TEXT("真实Combat死亡状态"), Combat->IsCombatDead());
    TestFalse(TEXT("未验证准入的真实死亡不得签发重生票据"), Connection->RespawnOperationId.IsValid());
    TestFalse(TEXT("未验证准入的真实死亡不得创建重生截止"), World->GetTimerManager().TimerExists(Connection->RespawnTimer));
    Pawn->OnAuthoritativeDeath().Broadcast(Death);
    TestEqual(TEXT("重复死亡没有偷渡已受理代次"), Connection->DeathAvatarGeneration, 0);

    // 只植入本连接的资源票据，不改变准入/平台生命周期；旧闭包必须连资源状态也不能触碰。
    bool bOwnedTimerRan = false;
    auto SeedOwnedTimer = [&]()
    {
        Connection->RespawnOperationId = FGuid::NewGuid();
        Connection->DeathAvatarGeneration = Combat->GetCombatAvatarGeneration();
        World->GetTimerManager().SetTimer(Connection->RespawnTimer,
            FTimerDelegate::CreateLambda([&]() { bOwnedTimerRan = true; }), 5.0f, false);
    };
    SeedOwnedTimer();
    const FGuid CurrentPawnOperation = Connection->PawnOperationId;
    const FGuid CurrentRespawnOperation = Connection->RespawnOperationId;
    const int32 CurrentAvatar = Connection->DeathAvatarGeneration;
    Bridge->RestartDeadPawn(Controller, Connection, FGuid::NewGuid(), CurrentRespawnOperation, CurrentAvatar);
    Bridge->RestartDeadPawn(Controller, Connection, CurrentPawnOperation, FGuid::NewGuid(), CurrentAvatar);
    Bridge->RestartDeadPawn(Controller, Connection, CurrentPawnOperation, CurrentRespawnOperation, CurrentAvatar + 1);
    TestTrue(TEXT("旧Pawn/旧票据/旧Avatar闭包不能清理后继票据"), Connection->RespawnOperationId == CurrentRespawnOperation);
    TestTrue(TEXT("旧闭包不能清理后继截止"), World->GetTimerManager().TimerExists(Connection->RespawnTimer));
    Bridge->RestartDeadPawn(Controller, Connection, CurrentPawnOperation, CurrentRespawnOperation, CurrentAvatar);
    TestFalse(TEXT("当前截止先消费票据再复核准入"), Connection->RespawnOperationId.IsValid());
    TestFalse(TEXT("准入失败的当前截止也清理自身计时器"), World->GetTimerManager().TimerExists(Connection->RespawnTimer));
    TestTrue(TEXT("未验证准入失败关闭，保留原Pawn且不执行Spawn"), Controller->GetPawn() == Pawn);
    Bridge->RestartDeadPawn(Controller, Connection, CurrentPawnOperation, CurrentRespawnOperation, CurrentAvatar);
    TestTrue(TEXT("已消费的同一截止幂等"), Controller->GetPawn() == Pawn && !Connection->RespawnOperationId.IsValid());

    SeedOwnedTimer();
    FTimerHandle IndependentTimer;
    World->GetTimerManager().SetTimer(IndependentTimer, FTimerDelegate::CreateLambda([]() {}), 8.0f, false);
    Bridge->ReleasePawnDeath(Connection);
    TestFalse(TEXT("替换Pawn清理旧死亡订阅"), Pawn->OnAuthoritativeDeath().IsBound());
    TestFalse(TEXT("替换Pawn清理旧截止"), World->GetTimerManager().TimerExists(Connection->RespawnTimer));
    TestEqual(TEXT("新Pawn可独立接收自己的死亡代次"), Connection->DeathAvatarGeneration, 0);
    TestTrue(TEXT("本连接清理不影响其它世界任务计时器"), World->GetTimerManager().TimerExists(IndependentTimer));
    World->GetTimerManager().ClearTimer(IndependentTimer);
    Bridge->ObservePawnDeath(*Controller, Connection); SeedOwnedTimer();
    Bridge->ReleaseConnection(Connection);
    TestFalse(TEXT("连接撤销精确解绑死亡"), Pawn->OnAuthoritativeDeath().IsBound());
    TestFalse(TEXT("连接撤销取消重生截止"), World->GetTimerManager().TimerExists(Connection->RespawnTimer));
    TestFalse(TEXT("连接撤销使旧Pawn操作失效"), Connection->PawnOperationId.IsValid());

    // 新的本桥资源归属记录只测试Close；仍不构造任何有效准入。
    Connection->OperationId = FGuid::NewGuid(); Connection->PawnOperationId = FGuid::NewGuid();
    Bridge->ObservePawnDeath(*Controller, Connection); SeedOwnedTimer();
    const FTimerHandle ClosingTimer = Connection->RespawnTimer;
    Bridge->Close(); Bridge->Close();
    TestFalse(TEXT("Close取消所有本桥重生截止"), World->GetTimerManager().TimerExists(ClosingTimer));
    TestFalse(TEXT("Close解绑Pawn死亡"), Pawn->OnAuthoritativeDeath().IsBound());
    TestFalse(TEXT("Close解绑准入通知"), Admission->OnAdmissionChanged().IsBound());
    TestTrue(TEXT("Close移除连接资源账本"), Bridge->Connections.IsEmpty());
    TestFalse(TEXT("清理过程没有执行植入的延迟工作"), bOwnedTimerRan);
    return true;
}
#endif
