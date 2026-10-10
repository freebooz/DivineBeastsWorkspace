// 仅自动化生命周期端口夹具：验证GameMode真实重连事务/宽限所有权，不冒充项目Pawn、Definition或联网验签成功。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "CoreGlobals.h"
#include "TimerManager.h"
namespace
{
class FArenaReconnectLifecycleFixture final : public IGamePlatformArenaGameplayLifecycleAdapter
{
public:
    // 此测试端口只推进前赛阶段，不拥有Pawn/代次，明确拒绝只读捕获而非伪造成功。
    bool CapturePlayerGameplayOwnership(const FString&, FGamePlatformArenaGameplayOwnership& Out) const override
    { Out = {}; return false; }

    bool bAcceptSpawn = true;
    int32 SpawnRequests = 0;
    TMap<FString, bool> ActiveRequests;
    bool SpawnPlayer(const FString&, FName, FName, FString& OutReason) override
    { ++SpawnRequests; OutReason = bAcceptSpawn ? FString() : TEXT("TestInitializationFailure"); return bAcceptSpawn; }
    bool SetPlayerGameplayActive(const FString& PlayerId, bool bActive, FString& OutReason) override
    { ActiveRequests.Add(PlayerId, bActive); OutReason.Reset(); return true; }
    bool RequestRespawn(const FString& PlayerId, FName Team, FName Policy, float, FString& OutReason) override
    { return SpawnPlayer(PlayerId, Team, Policy, OutReason); }
};
class FArenaReconnectHeroFixture final : public IGamePlatformArenaHeroEligibilityProvider
{
public:
    bool IsHeroEligible(const FString&, const FString& Hero, FName, FString& OutReason) const override
    { OutReason.Reset(); return Hero == TEXT("Test.Hero"); }
};
// GT机制夹具独占世界与测试端口。latent正常结束、断言失败或命令直接析构都先摘Mode裸接口再销毁世界。
class FArenaReconnectFixtureOwner final
{
public:
    UWorld* World = nullptr;
    TWeakObjectPtr<AGamePlatformArenaGameMode> Mode;
    FArenaReconnectLifecycleFixture Lifecycle;
    ~FArenaReconnectFixtureOwner() { Cleanup(); }
    void Cleanup()
    {
        check(IsInGameThread());
        if (bCleanupStarted) { return; }
        bCleanupStarted = true;
        if (auto* OwnedMode = Mode.Get())
        {
            OwnedMode->SetGameplayLifecycleAdapter(nullptr);
            if (World) { World->GetTimerManager().ClearAllTimersForObject(OwnedMode); }
        }
        UWorld* OwnedWorld = World; World = nullptr; Mode.Reset();
        if (IsValid(OwnedWorld)) { OwnedWorld->EndPlay(EEndPlayReason::Quit); OwnedWorld->DestroyWorld(false); }
    }
private:
    bool bCleanupStarted = false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformArenaReconnectTest,
    "GamePlatform.Arena.Lifecycle.ReconnectPreservesGraceUntilSpawnAccepted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformArenaReconnectTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    const auto Fixture = MakeShared<FArenaReconnectFixtureOwner>();
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    Fixture->World = World;
    if (!TestNotNull(TEXT("重连机制测试世界"), World)) { return false; }
    // 完整路由Actor初始化，使GameMode按引擎流程创建GameState，不用未初始化Actor跳过组件和反射事件前提。
    World->InitializeActorsForPlay(FURL());
    auto* Mode = World->SpawnActor<AGamePlatformArenaGameMode>(); Fixture->Mode = Mode;
    if (!TestNotNull(TEXT("真实重连GameMode"), Mode)) { return false; }
    Mode->DispatchBeginPlay(); Mode->CountdownSeconds = 0.01f;
    FArenaReconnectHeroFixture Heroes;
    Mode->SetGameplayLifecycleAdapter(&Fixture->Lifecycle);
    FGamePlatformArenaAssignment Assignment; const auto* Spec = FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Duel1v1);
    if (!TestNotNull(TEXT("Duel1v1结构规格"), Spec)) { return false; }
    Assignment.MatchId = TEXT("Test.Match"); Assignment.GameServerId = TEXT("Test.Server");
    Assignment.ArenaModeId = Spec->ArenaModeId; Assignment.MapId = Spec->MapId; Assignment.TeamSize = 1; Assignment.TotalPlayers = 2;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FGamePlatformArenaRosterSlot Slot; Slot.PlayerId = FString::Printf(TEXT("Player.%d"), Index);
        Slot.CharacterId = FString::Printf(TEXT("Character.%d"), Index); Slot.TeamId = FName(*FString::Printf(TEXT("Team.%d"), Index));
        Slot.SlotIndex = Index; Assignment.Roster.Add(Slot);
    }
    FString Error;
    if (!TestTrue(TEXT("真实GameMode应用结构测试Assignment"), Mode->ApplyAssignment(Assignment, Error)))
    { AddError(Error); return false; }
    TArray<AGamePlatformArenaPlayerState*> States;
    for (const auto& Slot : Assignment.Roster)
    {
        auto* State = World->SpawnActor<AGamePlatformArenaPlayerState>(); States.Add(State);
        if (!TestNotNull(TEXT("重连Roster PlayerState"), State)) { return false; }
        FGamePlatformArenaTransferTicketClaims Claims; Claims.PlayerId = Slot.PlayerId; Claims.CharacterId = Slot.CharacterId;
        Claims.MatchId = Assignment.MatchId; Claims.DestinationServerId = Assignment.GameServerId;
        Claims.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(5); Claims.bConsumed = true;
        if (!TestTrue(TEXT("可信夹具准入"), Mode->AdmitPlayer(State, Claims, Error)))
        { AddError(Error); return false; }
    }
    for (auto* State : States)
    {
        if (!TestTrue(TEXT("测试Hero选取"), Mode->RequestHeroSelection(State, TEXT("Test.Hero"), &Heroes, Error)))
        { AddError(Error); return false; }
    }
    for (auto* State : States)
    {
        if (!TestTrue(TEXT("测试Ready"), Mode->RequestReady(State, Error)))
        { AddError(Error); return false; }
    }
    // 新TimerManager首Tick只激活Pending倒计时；必须跨两个真实帧，才能测试InProgress下的重连出生事务。
    // 共享owner保证命令被中止而不再Update时，也不遗留rooted世界或栈测试端口裸指针。
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
        [this, Fixture, World, Mode, States, Assignment, CountdownTicks = 0, LastTimerFrame = MAX_uint64]() mutable
    {
        if (LastTimerFrame == GFrameCounter) { return false; }
        LastTimerFrame = GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        if (++CountdownTicks < 2) { return false; }
        auto& Lifecycle = Fixture->Lifecycle; FString Error;
        if (!TestEqual(TEXT("真实阶段事务进入InProgress"), Mode->GetMatchPhase(), EGamePlatformArenaMatchPhase::InProgress) ||
            !TestEqual(TEXT("倒计时调用每玩家一次出生适配"), Lifecycle.SpawnRequests, 2))
        { Fixture->Cleanup(); return true; }
        Mode->MarkPlayerDisconnected(States[0]->PlayerIdPublic);
        TestFalse(TEXT("断线发出失活命令"), Lifecycle.ActiveRequests.FindRef(States[0]->PlayerIdPublic));
        auto* Reconnected = World->SpawnActor<AGamePlatformArenaPlayerState>(); Lifecycle.bAcceptSpawn = false;
        if (!TestNotNull(TEXT("真实新重连PlayerState"), Reconnected)) { Fixture->Cleanup(); return true; }
        TestFalse(TEXT("出生失败重连拒绝"), Mode->TryReconnectPlayer(Reconnected, Assignment.Roster[0].PlayerId, Error));
        TestTrue(TEXT("失败保留原重连宽限"), Mode->IsPlayerAwaitingReconnect(Assignment.Roster[0].PlayerId));
        TestEqual(TEXT("失败新PlayerState保持断线"), Reconnected->ConnectionState, EGamePlatformArenaConnectionState::Disconnected);
        Lifecycle.bAcceptSpawn = true;
        TestTrue(TEXT("出生受理后重连成功"), Mode->TryReconnectPlayer(Reconnected, Assignment.Roster[0].PlayerId, Error));
        TestFalse(TEXT("成功才消费宽限"), Mode->IsPlayerAwaitingReconnect(Assignment.Roster[0].PlayerId));
        TestEqual(TEXT("两次重连都走实际出生调用"), Lifecycle.SpawnRequests, 4);
        Fixture->Cleanup(); return true;
    }));
    return true;
}
#endif
