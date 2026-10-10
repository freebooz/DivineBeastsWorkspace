// 真实Combat死亡→项目Adapter→通用GameMode回归。Assignment/前赛出生使用明确测试端口；不代表Hero加载/地图/联机通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "CoreGlobals.h"
#include "TimerManager.h"
namespace
{
// 只让已验证的GameMode阶段事务进入测试阶段，不生成或宣称真实Hero/Pawn已初始化。
class FDeathBridgeSetupLifecycle final : public IGamePlatformArenaGameplayLifecycleAdapter
{
public:
    // 此测试端口只推进前赛阶段，不拥有Pawn/代次，明确拒绝只读捕获而非伪造成功。
    bool CapturePlayerGameplayOwnership(const FString&, FGamePlatformArenaGameplayOwnership& Out) const override
    { Out = {}; return false; }

    bool SpawnPlayer(const FString&, FName, FName, FString& Reason) override { Reason.Reset(); return true; }
    bool SetPlayerGameplayActive(const FString&, bool, FString& Reason) override { Reason.Reset(); return true; }
    bool RequestRespawn(const FString&, FName, FName, float, FString&) override { return false; }
};
class FDeathBridgeSetupHeroes final : public IGamePlatformArenaHeroEligibilityProvider
{
public:
    bool IsHeroEligible(const FString&, const FString& Hero, FName, FString& Reason) const override
    { Reason.Reset(); return Hero == TEXT("Test.Hero"); }
};

// 只保存实际清理后的可核验结果，不持有世界或接口；用于命令直接析构的中止回归。
struct FDeathBridgeCleanupEvidence
{
    int32 CleanupExecutions = 0;
    int32 ClearedRespawnRequests = 0;
    bool bModeAdapterDetached = false;
    bool bOwnedTimersAbsent = true;
    bool bOwnedDelegatesUnbound = false;
    bool bAdapterReferencesReleased = false;
    bool bWorldRetired = false;
};

// 仅本自动化用例的GT夹具所有者。所有latent命令共享它，命令被Dequeue而不再Update时也必须收回rooted世界。
// Setup/Adapter由此唯一持有；监听与Timer只清理本夹具拥有的项，不访问生产全局状态。
class FDeathBridgeFixtureOwner final
{
public:
    UWorld* World = nullptr;
    TWeakObjectPtr<AGamePlatformArenaGameMode> Mode;
    TSharedPtr<FDeathBridgeSetupLifecycle> SetupLifecycle;
    TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter> Adapter;
    TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter> NewAdapter;
    TWeakObjectPtr<ADivineBeastsCharacter> DeathObserverPawn;
    FDelegateHandle DeathObserverHandle;
    TWeakObjectPtr<AGamePlatformArenaPlayerState> StatsObserverState;
    FDelegateHandle StatsObserverHandle;
    FGamePlatformCombatEvent ObservedDeath;
    // 回调仅弱引用Adapter，避免夹具/委托/接口互相保活；私有资源访问仍限定在获准的测试友元中。
    TArray<TFunction<void()>> ReleaseAdapterResources;
    TSharedPtr<FDeathBridgeCleanupEvidence> CleanupEvidence = MakeShared<FDeathBridgeCleanupEvidence>();

    ~FDeathBridgeFixtureOwner() { Cleanup(); }

    // 正常、失败和命令中止共用一次清理：先解除裸接口，再解绑/取消，再释放端口，最后销毁自有世界。
    void Cleanup()
    {
        check(IsInGameThread());
        if (bCleanupStarted) { return; }
        bCleanupStarted = true;
        ++CleanupEvidence->CleanupExecutions;
        if (auto* OwnedMode = Mode.Get())
        {
            OwnedMode->SetGameplayLifecycleAdapter(nullptr);
            CleanupEvidence->bModeAdapterDetached = !OwnedMode->GetGameplayLifecycleBindingId().IsValid();
            if (World) { World->GetTimerManager().ClearAllTimersForObject(OwnedMode); }
        }
        if (auto* Pawn = DeathObserverPawn.Get()) { Pawn->OnAuthoritativeDeath().Remove(DeathObserverHandle); }
        if (auto* State = StatsObserverState.Get()) { State->OnArenaStatsChanged.Remove(StatsObserverHandle); }
        DeathObserverHandle.Reset(); StatsObserverHandle.Reset();
        for (auto& Release : ReleaseAdapterResources) { Release(); }
        ReleaseAdapterResources.Reset();
        CleanupEvidence->bOwnedDelegatesUnbound =
            (!DeathObserverPawn.IsValid() || !DeathObserverPawn->OnAuthoritativeDeath().IsBound()) &&
            (!StatsObserverState.IsValid() || !StatsObserverState->OnArenaStatsChanged.IsBound()) &&
            !DeathObserverHandle.IsValid() && !StatsObserverHandle.IsValid();
        Adapter.Reset(); NewAdapter.Reset(); SetupLifecycle.Reset();
        CleanupEvidence->bAdapterReferencesReleased = !Adapter.IsValid() && !NewAdapter.IsValid() && !SetupLifecycle.IsValid();
        UWorld* OwnedWorld = World; World = nullptr; Mode.Reset(); DeathObserverPawn.Reset(); StatsObserverState.Reset();
        if (IsValid(OwnedWorld))
        {
            // EndPlay即使世界只手动启动过测试Actor，也会先标记TearingDown；随后DestroyWorld收回root并清理场景。
            OwnedWorld->EndPlay(EEndPlayReason::Quit);
            OwnedWorld->DestroyWorld(false);
            CleanupEvidence->bWorldRetired = !OwnedWorld->IsRooted() && OwnedWorld->bIsTearingDown;
        }
    }
private:
    bool bCleanupStarted = false;
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDivineBeastsArenaDeathBridgeTest,
    "DivineBeasts.Arena.Server.AuthoritativeDeathBridgeRejectsRetiredPawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FDivineBeastsArenaDeathBridgeTest::GetTests(TArray<FString>& OutNames, TArray<FString>& OutCommands) const
{
    OutNames.Add(TEXT("NormalAndOldRequest")); OutCommands.Add(TEXT("Normal"));
    OutNames.Add(TEXT("StatsListenerEndsMatch")); OutCommands.Add(TEXT("EndDuringStats"));
    OutNames.Add(TEXT("QueuedRespawnAfterEnd")); OutCommands.Add(TEXT("EndBeforeTimer"));
}

bool FDivineBeastsArenaDeathBridgeTest::RunTest(const FString& Parameters)
{
    const bool bEndDuringStats = Parameters == TEXT("EndDuringStats");
    const bool bEndBeforeTimer = Parameters == TEXT("EndBeforeTimer");
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    TSharedPtr<FDeathBridgeFixtureOwner> Fixture = MakeShared<FDeathBridgeFixtureOwner>();
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    Fixture->World = World;
    if (!TestNotNull(TEXT("死亡桥测试世界"), World)) { return false; }
    // CreateWorld只初始化世界基础设施。真实Actor初始化使Controller注册进世界列表，并允许动态UFUNCTION事件。
    // 不手工AddController或改Actor标志；后续Spawn走完整Pre/PostInitializeComponents。
    World->InitializeActorsForPlay(FURL());
    if (!TestTrue(TEXT("世界实际Actor初始化已完成"), World->AreActorsInitialized())) { return false; }
    auto* Mode = World->SpawnActor<AGamePlatformArenaGameMode>();
    Fixture->Mode = Mode;
    if (!TestNotNull(TEXT("竞技GameMode"), Mode)) { return false; }
    // GameMode的真实PreInitializeComponents创建并装配GameState，不覆盖为第二个手工生成对象。
    auto* ArenaState = Mode->GetGameState<AGamePlatformArenaGameState>();
    if (!TestNotNull(TEXT("引擎装配的竞技GameState"), ArenaState) ||
        !TestTrue(TEXT("Mode与World共享同一竞技GameState"), World->GetGameState() == ArenaState)) { return false; }
    Mode->DispatchBeginPlay(); Mode->CountdownSeconds = 0.01f; Mode->StandardRespawnDelaySeconds = 0.0f;
    // 倒计时跨Automation帧执行，测试端口必须由latent命令持有至阶段事务结束，不能保留栈对象地址。
    Fixture->SetupLifecycle = MakeShared<FDeathBridgeSetupLifecycle>(); FDeathBridgeSetupHeroes Heroes; Mode->SetGameplayLifecycleAdapter(Fixture->SetupLifecycle.Get());
    FGamePlatformArenaAssignment Assignment; const auto* Spec = FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Duel1v1);
    if (!TestNotNull(TEXT("内置Duel1v1结构规格"), Spec))
    { return false; }
    Assignment.MatchId = TEXT("Test.Death.Match"); Assignment.GameServerId = TEXT("Test.Server"); Assignment.ArenaModeId = Spec->ArenaModeId;
    Assignment.MapId = Spec->MapId; Assignment.TeamSize = 1; Assignment.TotalPlayers = 2;
    for (int32 Index = 0; Index < 2; ++Index)
    { FGamePlatformArenaRosterSlot Slot; Slot.PlayerId = FString::Printf(TEXT("Player.%d"), Index);
        Slot.CharacterId = FString::Printf(TEXT("Character.%d"), Index); Slot.TeamId = FName(*FString::Printf(TEXT("Team.%d"), Index));
        Slot.SlotIndex = Index; Assignment.Roster.Add(Slot); }
    FString Error;
    if (!TestTrue(TEXT("应用结构测试Assignment"), Mode->ApplyAssignment(Assignment, Error)))
    { AddError(Error); return false; }
    TArray<AGamePlatformArenaPlayerController*> Controllers; TArray<AGamePlatformArenaPlayerState*> States;
    for (const auto& Slot : Assignment.Roster)
    {
        auto* Controller = World->SpawnActor<AGamePlatformArenaPlayerController>(); Controllers.Add(Controller);
        if (!TestNotNull(TEXT("竞技Controller"), Controller)) { return false; }
        // Controller的真实PostInitializeComponents依据GameState默认GameMode创建PlayerState；准入只写受信Roster身份。
        auto* State = Controller->GetPlayerState<AGamePlatformArenaPlayerState>(); States.Add(State);
        if (!TestNotNull(TEXT("引擎创建的竞技PlayerState"), State) ||
            !TestTrue(TEXT("引擎PlayerState归属真实Controller"), State->GetOwner() == Controller)) { return false; }
        FGamePlatformArenaTransferTicketClaims Claims; Claims.PlayerId = Slot.PlayerId; Claims.CharacterId = Slot.CharacterId;
        Claims.MatchId = Assignment.MatchId; Claims.DestinationServerId = Assignment.GameServerId; Claims.bConsumed = true;
        Claims.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(5);
        if (!TestTrue(TEXT("测试受信准入"), Mode->AdmitPlayer(State, Claims, Error)))
        { AddError(Error); return false; }
    }
    for (auto* State : States)
    {
        if (!TestTrue(TEXT("受信玩家选择测试英雄"), Mode->RequestHeroSelection(State, TEXT("Test.Hero"), &Heroes, Error)))
        { AddError(Error); return false; }
    }
    for (auto* State : States)
    {
        if (!TestTrue(TEXT("受信玩家完成Ready"), Mode->RequestReady(State, Error)))
        { AddError(Error); return false; }
    }
    // 新TimerManager首Tick只将Pending倒计时激活；同GFrameCounter再次Tick会直接返回。
    // 由真实Automation帧分别驱动两次，保留InProgress断言，不修改全局帧计数或直接跳过阶段事务。
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
        [this, Fixture, World, Mode, ArenaState, Controllers, States, Assignment, bEndDuringStats, bEndBeforeTimer,
         CountdownTicks = 0, LastTimerFrame = MAX_uint64]() mutable
    {
        if (LastTimerFrame == GFrameCounter) { return false; }
        LastTimerFrame = GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        if (++CountdownTicks < 2) { return false; }
        if (!TestEqual(TEXT("通过真实阶段事务进入InProgress"), ArenaState->MatchPhase, EGamePlatformArenaMatchPhase::InProgress))
        { Fixture->Cleanup(); return true; }
        FString Error;
        TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter>& Adapter = Fixture->Adapter;
        Adapter = MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(*Mode, TArray<FGamePlatformDataLease>());
        // 友元访问在测试内创建清理动作；弱Adapter不延长其寿命，真正取消每个请求的原世界Timer和死亡绑定。
        const auto TrackAdapterResources = [&Fixture](const TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter>& OwnedAdapter)
        {
            Fixture->ReleaseAdapterResources.Add([WeakAdapter = TWeakPtr<FDivineBeastsArenaGameplayLifecycleAdapter>(OwnedAdapter), Evidence = Fixture->CleanupEvidence]()
            {
                if (auto Owned = WeakAdapter.Pin())
                {
                    TArray<TPair<TWeakObjectPtr<UWorld>, FTimerHandle>> Timers;
                    for (const auto& Pair : Owned->RespawnTimers) { Timers.Emplace(Pair.Value.World, Pair.Value.Timer); }
                    Evidence->ClearedRespawnRequests += Timers.Num();
                    Owned->ClearRespawnTimers();
                    for (const auto& Timer : Timers)
                    { if (auto* OwnerWorld = Timer.Key.Get()) { Evidence->bOwnedTimersAbsent &= !OwnerWorld->GetTimerManager().TimerExists(Timer.Value); } }
                    Evidence->bOwnedTimersAbsent &= Owned->RespawnTimers.IsEmpty();
                    TArray<FString> DeathPlayers; Owned->DeathBindings.GetKeys(DeathPlayers);
                    for (const auto& Player : DeathPlayers) { Owned->UnbindPawnDeath(Player); }
                }
            });
        };
        TrackAdapterResources(Adapter);
        Mode->SetGameplayLifecycleAdapter(Adapter.Get()); const FString PlayerId = States[0]->PlayerIdPublic;
        // 无物理场的机制夹具明确忽略出生点碰撞，避免真实Actor初始化后默认碰撞策略把替代Pawn拒绝在同一测试位置。
        FActorSpawnParameters PawnSpawnParameters; PawnSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn = World->SpawnActor<ADivineBeastsCharacter>(PawnSpawnParameters);
        const auto Cleanup = [&Fixture]() { Fixture->Cleanup(); };
        if (!TestNotNull(TEXT("初代真实项目Pawn"), Pawn)) { Cleanup(); return true; }
        Controllers[0]->Possess(Pawn); Pawn->DispatchBeginPlay();
        if (!TestTrue(TEXT("初代真实Pawn完成Actor初始化与BeginPlay"), Pawn->IsActorInitialized() && Pawn->HasActorBegunPlay()) ||
            !TestTrue(TEXT("真实Possess保持Controller与Pawn双向拥有"), Controllers[0]->GetPawn() == Pawn && Pawn->GetController() == Controllers[0]))
        { Cleanup(); return true; }
        // 生产SpawnPlayer会绑定此代次；夹具只补该拥有前提，不宣称无Definition的Pawn达到Ready。
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        if (!TestNotNull(TEXT("初代真实玩法资格组件"), Eligibility) ||
            !TestTrue(TEXT("绑定初代测试Pawn玩法代次"), Eligibility->BindServerAvatarGeneration(1)))
        { Cleanup(); return true; }
        // 仅绑定实际测试Pawn到本场已准入身份，不运行/伪造资源预热与项目出生成功。
        Adapter->SpawnedPawns.Add(PlayerId, Pawn); Adapter->SpawnGenerations.Add(PlayerId, 1);
        Adapter->BindPawnDeath(PlayerId, *Pawn, *States[0], 1);
        Fixture->DeathObserverPawn = Pawn;
        Fixture->DeathObserverHandle = Pawn->OnAuthoritativeDeath().AddLambda([WeakFixture = TWeakPtr<FDeathBridgeFixtureOwner>(Fixture)](const auto& Event)
        { if (auto Owner = WeakFixture.Pin()) { Owner->ObservedDeath = Event; } });
        auto* Combat = Pawn->GetGamePlatformCombatComponent();
        if (!TestNotNull(TEXT("真实战斗组件"), Combat)) { Cleanup(); return true; }
        auto* Attributes = Combat->GetCombatAttributeSet();
        if (!TestNotNull(TEXT("真实战斗属性"), Attributes)) { Cleanup(); return true; }
        // 死亡事件只允许当前Controller/PlayerState/Pawn拥有快照；缺前提时先保留失败原因，不能继续访问请求表。
        FGamePlatformArenaGameplayOwnership Ownership;
        if (!TestTrue(TEXT("死亡前真实Adapter捕获当前受控Pawn"), Adapter->CapturePlayerGameplayOwnership(PlayerId, Ownership)) ||
            !TestTrue(TEXT("死亡前拥有快照指向初代Pawn"), Ownership.Pawn.Get() == Pawn) ||
            !TestEqual(TEXT("死亡前拥有快照代次"), Ownership.AvatarGeneration, 1))
        { Cleanup(); return true; }
        if (Attributes)
        {
            // 真正的统计通知内结束比赛；结束失活发生在死亡桥接尚未尝试排复活时。
            if (bEndDuringStats)
            {
                Fixture->StatsObserverState = States[0];
                Fixture->StatsObserverHandle = States[0]->OnArenaStatsChanged.AddLambda([this, Mode, WinnerTeam = States[1]->TeamId](AGamePlatformArenaPlayerState*)
                { FString EndError; if (!TestTrue(TEXT("统计通知内受理结束比赛"), Mode->EndMatch(WinnerTeam, EGamePlatformArenaMatchEndReason::PlayerForfeit, EndError))) { AddError(EndError); } });
            }
            FGameplayEffectSpec Effect; Combat->ResolveIncomingDamage(*Attributes, Effect, Attributes->GetHealth() + 1.0f);
            if (!TestEqual(TEXT("真实死亡桥接记录一次死亡"), States[0]->Deaths, 1))
            { Cleanup(); return true; }
            TestTrue(TEXT("零秒复活先退出Combat栈，当前Pawn尚未被销毁替换"), Controllers[0]->GetPawn() == Pawn);
            if (bEndDuringStats)
            {
                TestEqual(TEXT("统计监听者真实结束比赛"), ArenaState->MatchPhase, EGamePlatformArenaMatchPhase::ResultPending);
                TestFalse(TEXT("广播返回后不能再排复活"), Adapter->RespawnTimers.Contains(PlayerId));
                States[0]->OnArenaStatsChanged.Remove(Fixture->StatsObserverHandle); Fixture->StatsObserverHandle.Reset();
            }
            else
            {
                if (!TestTrue(TEXT("本世界实际Adapter受理复活计时器"), Adapter->RespawnTimers.Contains(PlayerId)))
                { Cleanup(); return true; }
            }
            if (bEndDuringStats || bEndBeforeTimer)
            {
                if (bEndBeforeTimer)
                {
                    const auto* Request = Adapter->RespawnTimers.Find(PlayerId);
                    if (!TestNotNull(TEXT("结束前自有复活请求"), Request)) { Cleanup(); return true; }
                    const FGuid OldRequestId = Request->RequestId;
                    if (!TestTrue(TEXT("已排复活之后受理结束比赛"), Mode->EndMatch(States[1]->TeamId, EGamePlatformArenaMatchEndReason::PlayerForfeit, Error)))
                    { AddError(Error); Cleanup(); return true; }
                    TestFalse(TEXT("结束撤销自有复活Timer"), Adapter->RespawnTimers.Contains(PlayerId));
                    // 显式驱动已经投递的旧回调，必须失败关闭；不能仅以ClearTimer证明迟到回调安全。
                    TestFalse(TEXT("已排回调结束后拒绝出生"), Adapter->ExecuteDeferredRespawn(PlayerId, OldRequestId));
                }
                // 观察实际下一帧的Timer调度，不能在已Tick过的同帧重复调用而得到空检查。
                Pawn->OnAuthoritativeDeath().Remove(Fixture->DeathObserverHandle); Fixture->DeathObserverHandle.Reset();
                ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
                    [this, Fixture, World, Controllers, Pawn, PlayerId, DeathFrame = GFrameCounter]() mutable
                {
                    if (GFrameCounter == DeathFrame) { return false; }
                    const auto& Adapter = Fixture->Adapter;
                    World->GetTimerManager().Tick(1.0f);
                    TestTrue(TEXT("下一Tick结束后仍未换Pawn"), Controllers[0]->GetPawn() == Pawn);
                    TestFalse(TEXT("下一Tick结束后仍无复活请求"), Adapter->RespawnTimers.Contains(PlayerId));
                    TestEqual(TEXT("结束后出生代次不推进"), Adapter->SpawnGenerations.FindRef(PlayerId), 1);
                    Fixture->Cleanup(); return true;
                }));
                return true;
            }
            const auto* OldRequest = Adapter->RespawnTimers.Find(PlayerId);
            if (!TestNotNull(TEXT("初代真实复活请求"), OldRequest)) { Cleanup(); return true; }
            const FGuid OldRequestId = OldRequest->RequestId;
            const FGamePlatformCombatEvent Death = Fixture->ObservedDeath;
            TestEqual(TEXT("不猜击杀方或增加击杀分数"), States[1]->Kills, 0);
            TestFalse(TEXT("重复死亡已解绑，拒绝重复处理"), Adapter->HandlePawnDeath(PlayerId, Pawn, States[0], Assignment.MatchId, 1, Death));
            auto* Replacement = World->SpawnActor<ADivineBeastsCharacter>(PawnSpawnParameters);
            if (!TestNotNull(TEXT("第二代真实项目Pawn"), Replacement)) { Cleanup(); return true; }
            Controllers[0]->Possess(Replacement); Replacement->DispatchBeginPlay();
            if (!TestTrue(TEXT("战斗绑定第二代Avatar"), Replacement->GetGamePlatformCombatComponent()->ResetForNewAvatar(2)) ||
                !TestTrue(TEXT("玩法资格绑定第二代Avatar"), Replacement->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>()->BindServerAvatarGeneration(2)))
            { Cleanup(); return true; }
            Adapter->SpawnedPawns.Add(PlayerId, Replacement); Adapter->SpawnGenerations.Add(PlayerId, 2);
            Adapter->BindPawnDeath(PlayerId, *Replacement, *States[0], 2);
            TestFalse(TEXT("旧Pawn死亡不能影响新Pawn"), Adapter->HandlePawnDeath(PlayerId, Pawn, States[0], Assignment.MatchId, 1, Death));
            TestFalse(TEXT("旧比赛身份拒绝"), Adapter->HandlePawnDeath(PlayerId, Replacement, States[0], TEXT("Old.Match"), 2, Death));
            TestTrue(TEXT("迟到事实未清掉当前复活计时器"), Adapter->RespawnTimers.Contains(PlayerId));
            TestEqual(TEXT("迟到事实没有重复死亡计数"), States[0]->Deaths, 1);
            auto* ReplacementCombat = Replacement->GetGamePlatformCombatComponent();
            ReplacementCombat->ResolveIncomingDamage(*ReplacementCombat->GetCombatAttributeSet(), Effect, ReplacementCombat->GetCombatHealth() + 1.0f);
            const auto* NewRequest = Adapter->RespawnTimers.Find(PlayerId);
            if (!TestNotNull(TEXT("新Pawn死亡受理的复活请求"), NewRequest)) { Cleanup(); return true; }
            const FGuid NewRequestId = NewRequest->RequestId;
            TestTrue(TEXT("新Pawn死亡签发新复活请求身份"), NewRequestId != OldRequestId);
            TestFalse(TEXT("旧回调不能操作新Timer"), Adapter->ExecuteDeferredRespawn(PlayerId, OldRequestId));
            const auto* RetainedRequest = Adapter->RespawnTimers.Find(PlayerId);
            if (!TestNotNull(TEXT("旧回调后的新Timer"), RetainedRequest)) { Cleanup(); return true; }
            TestEqual(TEXT("旧回调未清新Timer身份"), RetainedRequest->RequestId, NewRequestId);
            TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter>& NewAdapter = Fixture->NewAdapter;
            NewAdapter = MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(*Mode, TArray<FGamePlatformDataLease>());
            TrackAdapterResources(NewAdapter);
            Mode->SetGameplayLifecycleAdapter(NewAdapter.Get());
            auto* NewPawn = World->SpawnActor<ADivineBeastsCharacter>(PawnSpawnParameters);
            if (!TestNotNull(TEXT("新装配真实项目Pawn"), NewPawn))
            { Cleanup(); return true; }
            Controllers[0]->Possess(NewPawn); NewPawn->DispatchBeginPlay();
            auto* NewEligibility = NewPawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
            if (!TestTrue(TEXT("新装配战斗绑定第三代Avatar"), NewPawn->GetGamePlatformCombatComponent()->ResetForNewAvatar(3)) ||
                !TestTrue(TEXT("新装配玩法资格绑定第三代Avatar"), NewEligibility->BindServerAvatarGeneration(3)))
            { Cleanup(); return true; }
            NewAdapter->SpawnedPawns.Add(PlayerId, NewPawn); NewAdapter->SpawnGenerations.Add(PlayerId, 3);
            // 明确测试输入：预先置Active以观察旧装配是否篡改，不宣称该无Definition夹具通过生产Ready授权。
            NewEligibility->SetServerPlayerActive(true);
            if (!TestTrue(TEXT("新装配拥有自己的Timer"), NewAdapter->RequestRespawn(PlayerId, States[0]->TeamId, Mode->GetModeSpec().RespawnPolicyId, 10.0f, Error)))
            { AddError(Error); Cleanup(); return true; }
            const auto* NewAdapterRequest = NewAdapter->RespawnTimers.Find(PlayerId);
            if (!TestNotNull(TEXT("新装配真实复活请求"), NewAdapterRequest))
            { Cleanup(); return true; }
            const FGuid NewAdapterRequestId = NewAdapterRequest->RequestId;
            TestFalse(TEXT("已替换旧Adapter不得失活新Pawn"), Adapter->SetPlayerGameplayActive(PlayerId, false, Error));
            TestTrue(TEXT("旧失活保留新Pawn资格"), NewEligibility->IsServerPlayerActiveForGameplay());
            TestFalse(TEXT("旧Adapter自有Timer仍已清理"), Adapter->RespawnTimers.Contains(PlayerId));
            const auto* NewAdapterRetainedRequest = NewAdapter->RespawnTimers.Find(PlayerId);
            if (!TestNotNull(TEXT("旧清理后新装配的Timer"), NewAdapterRetainedRequest))
            { Cleanup(); return true; }
            TestEqual(TEXT("旧清理没有动新Adapter的Timer"), NewAdapterRetainedRequest->RequestId, NewAdapterRequestId);
            Adapter.Reset();
            TestTrue(TEXT("旧Adapter析构仍保留新Pawn资格"), NewEligibility->IsServerPlayerActiveForGameplay());
            // 模拟DequeueAllCommands：最后一个命令只析构，从未Update，实际10秒复活Timer仍由新Adapter持有。
            const FTimerHandle AbortTimer = NewAdapterRetainedRequest->Timer;
            TestTrue(TEXT("中止前新装配真实Timer存在"), World->GetTimerManager().TimerExists(AbortTimer));
            const TWeakObjectPtr<UWorld> WeakAbortWorld(World);
            const auto Evidence = Fixture->CleanupEvidence;
            bool bAbortedCommandUpdated = false;
            bool bAbortedStatsNotified = false;
            Fixture->StatsObserverState = States[0];
            Fixture->StatsObserverHandle = States[0]->OnArenaStatsChanged.AddLambda(
                [&bAbortedStatsNotified](AGamePlatformArenaPlayerState*) { bAbortedStatsNotified = true; });
            TestTrue(TEXT("中止前自有统计监听存在"), States[0]->OnArenaStatsChanged.IsBound());
            TSharedPtr<IAutomationLatentCommand> AbortedCommand = MakeShared<FFunctionLatentCommand>(
                [OwnedFixture = MoveTemp(Fixture), &bAbortedCommandUpdated]() { bAbortedCommandUpdated = true; return false; });
            TestTrue(TEXT("命令仍拥有rooted测试世界"), WeakAbortWorld.IsValid() && WeakAbortWorld->IsRooted());
            AbortedCommand.Reset();
            TestFalse(TEXT("中止不依赖再次Update"), bAbortedCommandUpdated);
            TestFalse(TEXT("中止清理不触发统计通知"), bAbortedStatsNotified);
            TestEqual(TEXT("命令析构只清理一次"), Evidence->CleanupExecutions, 1);
            TestTrue(TEXT("中止先摘Mode裸接口"), Evidence->bModeAdapterDetached);
            TestTrue(TEXT("中止真实取消在途复活请求"), Evidence->ClearedRespawnRequests >= 1 && Evidence->bOwnedTimersAbsent);
            TestTrue(TEXT("中止解绑自有死亡和统计委托"), Evidence->bOwnedDelegatesUnbound);
            TestTrue(TEXT("中止释放所有Adapter和Setup端口"), Evidence->bAdapterReferencesReleased);
            TestTrue(TEXT("命令析构收回rooted世界"), Evidence->bWorldRetired &&
                (!WeakAbortWorld.IsValid() || (!WeakAbortWorld->IsRooted() && WeakAbortWorld->bIsTearingDown)));
            return true;
        }
        Cleanup(); return true;
    }));
    return true;
}
#endif
