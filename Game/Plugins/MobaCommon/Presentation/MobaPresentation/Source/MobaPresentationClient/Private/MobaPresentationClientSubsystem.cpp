// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// MOBA本地玩家事实接线：事件驱动处理迟到Controller/Pawn/GameState与重生；不持有玩法权威或资产租约。
#include "MobaPresentationClientSubsystem.h"

#include "Adapters/MobaPresentationFactAdapters.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Feedback/GamePlatformLocalHitstopSubsystem.h"
#include "Feedback/GamePlatformHitFlashWorldSubsystem.h"
#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "Camera/CameraShakeBase.h"
#include "Tags/MobaPresentationTags.h"
#include "Feedback/MobaHitFeedbackPolicy.h"
#include "GameFramework/Character.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Events/MobaPresentationContextContributor.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "MobaPresentationRequestBuilder.h"
#include "MobaPresentationSemanticRegistry.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

void UMobaPresentationClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    ++BindingGeneration;
    BoundWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (ULocalPlayer* Player = GetLocalPlayer())
        PlayerControllerChangedHandle = Player->OnPlayerControllerChanged().AddUObject(
            this, &UMobaPresentationClientSubsystem::HandleControllerChanged);
    BindWorldEvents(BoundWorld.Get());

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &UMobaPresentationClientSubsystem::HandlePostLoadMap);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UMobaPresentationClientSubsystem::HandleWorldCleanup);

    RefreshBindings();
}

void UMobaPresentationClientSubsystem::Deinitialize()
{
    if (bClosing) return;
    bClosing = true;
    ++BindingGeneration;
    UnbindCombatFeedbackWorld();
    UnbindCombat();
    UnbindArena();

    if (ULocalPlayer* Player = GetLocalPlayer()) Player->OnPlayerControllerChanged().Remove(PlayerControllerChangedHandle);
    PlayerControllerChangedHandle.Reset();
    UnbindWorldEvents();
    BindController(nullptr);

    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    ContextContributors.Reset();
    PredictedFacts.Reset();
    ConfirmedFacts.Reset();
    FactOrder.Reset();
    LatestAvatarGeneration.Reset();
    BoundWorld.Reset();
    Super::Deinitialize();
    RenderedHitEvents.Reset();
    RenderedHitOrder.Reset();
    ResolvingHitEvents.Reset();
    ClearHitFeedbackResolver();
    ConfigureHitFeedbackProfile(nullptr, NAME_None, NAME_None);
}

bool UMobaPresentationClientSubsystem::RegisterContextContributor(
    FName ContributorId,
    TSharedRef<IMobaPresentationContextContributor> Contributor)
{
    if (bClosing || ContributorId.IsNone() || ContextContributors.Contains(ContributorId))
    {
        return false;
    }
    ContextContributors.Add(ContributorId, Contributor);
    return true;
}

bool UMobaPresentationClientSubsystem::UnregisterContextContributor(FName ContributorId)
{
    return ContextContributors.Remove(ContributorId) > 0;
}

void UMobaPresentationClientSubsystem::ResetWorldState(UWorld* NewWorld)
{
    if (bClosing) return;
    ++BindingGeneration;
    UnbindWorldEvents();
    BindController(nullptr);
    UnbindCombatFeedbackWorld();
    UnbindCombat();
    UnbindArena();

    BoundWorld = NewWorld;
    BindWorldEvents(NewWorld);
    ++WorldGeneration;
    RequestGeneration = 0;
    LastPhaseRevision = INDEX_NONE;
    TeamRevisions.Reset();
    ObjectiveRevisions.Reset();
    ArenaPlayerSnapshots.Reset();
    PredictedFacts.Reset();
    ConfirmedFacts.Reset();
    FactOrder.Reset();
    LatestAvatarGeneration.Reset();

    RenderedHitEvents.Reset();
    RenderedHitOrder.Reset();
    ResolvingHitEvents.Reset();
    ConfigureHitFeedbackProfile(nullptr, NAME_None, NAME_None);
    RefreshBindings();
}

void UMobaPresentationClientSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (bClosing) return;
    UWorld* LocalWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (LoadedWorld && LoadedWorld == LocalWorld && LoadedWorld != BoundWorld.Get())
    {
        ResetWorldState(LoadedWorld);
    }
    else if (LoadedWorld && LoadedWorld == LocalWorld)
    {
        RefreshBindings();
    }
}

void UMobaPresentationClientSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool /*bSessionEnded*/,
    bool /*bCleanupResources*/)
{
    if (World && World == BoundWorld.Get())
    {
        ++BindingGeneration;
        UnbindWorldEvents();
        BindController(nullptr);
        UnbindCombat();
        UnbindArena();
        UnbindCombatFeedbackWorld();
        BoundWorld.Reset();
        ++WorldGeneration;
        PredictedFacts.Reset();
        ConfirmedFacts.Reset();
        FactOrder.Reset();
        LatestAvatarGeneration.Reset();
        RenderedHitEvents.Reset();
        RenderedHitOrder.Reset();
        ResolvingHitEvents.Reset();
        ConfigureHitFeedbackProfile(nullptr, NAME_None, NAME_None);
    }
}

void UMobaPresentationClientSubsystem::RefreshBindings()
{
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (bClosing || !World || World->bIsTearingDown)
    {
        return;
    }
    if (World != BoundWorld.Get())
    {
        ResetWorldState(World);
        return;
    }

    // 由权威组件单向复制到本World，避免LocalPawn未出生时漏掉战斗事实。
    BindCombatFeedbackWorld(*World);

    if (AGamePlatformArenaGameState* ArenaState =
        World->GetGameState<AGamePlatformArenaGameState>())
    {
        BindArena(ArenaState);
    }

    APlayerController* PlayerController =
        GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
    BindController(PlayerController);
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    UGamePlatformCombatComponent* Combat =
        Pawn ? Pawn->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;

    if (Combat != BoundCombatComponent.Get())
    {
        UnbindCombat();
        if (Combat)
        {
            BoundCombatComponent = Combat;
            Combat->OnCombatEvent.AddUniqueDynamic(
                this,
                &UMobaPresentationClientSubsystem::HandleCombatEvent);
        }
    }
}

void UMobaPresentationClientSubsystem::BindWorldEvents(UWorld* World)
{
    if (bClosing || !World || World->bIsTearingDown) return;
    GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UMobaPresentationClientSubsystem::HandleGameStateSet);
    ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(
        this, &UMobaPresentationClientSubsystem::HandleActorSpawned));
}
void UMobaPresentationClientSubsystem::UnbindWorldEvents()
{
    if (UWorld* World = BoundWorld.Get())
    {
        World->GameStateSetEvent.Remove(GameStateSetHandle);
        World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
        World->GetTimerManager().ClearTimer(PendingBindingRefreshTimer);
    }
    PendingBindingRefreshTimer.Invalidate();
    GameStateSetHandle.Reset(); ActorSpawnedHandle.Reset();
}
void UMobaPresentationClientSubsystem::BindController(APlayerController* Controller)
{
    if (bClosing && Controller) return;
    if (BoundController.Get() == Controller) return;
    if (APlayerController* Old = BoundController.Get()) Old->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
    PawnChangedHandle.Reset(); BoundController = Controller;
    if (Controller) PawnChangedHandle = Controller->GetOnNewPawnNotifier().AddUObject(
        this, &UMobaPresentationClientSubsystem::HandlePawnChanged);
}
void UMobaPresentationClientSubsystem::HandleControllerChanged(APlayerController* Controller)
{
    if (bClosing) return;
    BindController(Controller); RefreshBindings();
}
void UMobaPresentationClientSubsystem::HandlePawnChanged(APawn* Pawn)
{
    (void)Pawn; RefreshBindings();
}
void UMobaPresentationClientSubsystem::HandleGameStateSet(AGameStateBase* GameState)
{
    (void)GameState; RefreshBindings();
}
void UMobaPresentationClientSubsystem::HandleActorSpawned(AActor* Actor)
{
    if (bClosing || !Actor || Actor->GetWorld() != BoundWorld.Get()) return;
    if (Actor->IsA<AGamePlatformArenaPlayerState>()) RefreshArenaPlayerBindings();
    // Spawn通知可早于BeginPlay；下一调度轮补一次组件查找，无逐帧或永久重试。
    if (Actor->IsA<APawn>() || Actor->IsA<AGamePlatformArenaGameState>())
        if (UWorld* World = BoundWorld.Get(); World && !PendingBindingRefreshTimer.IsValid())
            PendingBindingRefreshTimer = World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &UMobaPresentationClientSubsystem::HandleDeferredBindingRefresh,
                    BindingGeneration, TWeakObjectPtr<UWorld>(World)));
}
void UMobaPresentationClientSubsystem::HandleDeferredBindingRefresh(const uint64 ExpectedGeneration,
    const TWeakObjectPtr<UWorld> ExpectedWorld)
{
    // ClearTimer覆盖排队回调；即使回调已进入调度，完整世代/World校验仍拒绝旧服务接线。
    if (bClosing || BindingGeneration != ExpectedGeneration || ExpectedWorld != BoundWorld ||
        !ExpectedWorld.IsValid() || ExpectedWorld->bIsTearingDown) return;
    PendingBindingRefreshTimer.Invalidate();
    RefreshBindings();
}

void UMobaPresentationClientSubsystem::BindCombatFeedbackWorld(UWorld& World)
{
    if (bClosing || &World != BoundWorld.Get() || World.bIsTearingDown) return;
    UGamePlatformCombatFeedbackWorldSubsystem* Bus =
        World.GetSubsystem<UGamePlatformCombatFeedbackWorldSubsystem>();
    if (BoundCombatWorldBus.Get() == Bus)
    {
        return;
    }
    UnbindCombatFeedbackWorld();
    if (Bus)
    {
        BoundCombatWorldBus = Bus;
        // 总线回调同样属于本次World/服务世代；已进入广播栈的旧订阅也不得操作下一世界。
        const uint64 ExpectedGeneration = BindingGeneration;
        const TWeakObjectPtr<UWorld> ExpectedWorld(&World);
        CombatWorldFeedbackHandle = Bus->OnConfirmedFeedback().AddWeakLambda(this,
            [this, ExpectedGeneration, ExpectedWorld](const FGamePlatformCombatEvent& Event)
            {
                if (!bClosing && BindingGeneration == ExpectedGeneration && ExpectedWorld == BoundWorld &&
                    ExpectedWorld.IsValid() && !ExpectedWorld->bIsTearingDown)
                    HandleConfirmedNetworkCombatEvent(Event);
            });
    }
}

void UMobaPresentationClientSubsystem::UnbindCombatFeedbackWorld()
{
    if (UGamePlatformCombatFeedbackWorldSubsystem* Bus = BoundCombatWorldBus.Get())
    {
        if (CombatWorldFeedbackHandle.IsValid())
        {
            Bus->OnConfirmedFeedback().Remove(CombatWorldFeedbackHandle);
        }
    }
    CombatWorldFeedbackHandle.Reset();
    BoundCombatWorldBus.Reset();
}

void UMobaPresentationClientSubsystem::HandleConfirmedNetworkCombatEvent(
    const FGamePlatformCombatEvent& Event)
{
    if (bClosing || !BoundWorld.IsValid() || BoundWorld->bIsTearingDown) return;
    // 只投影服务器已确认事实到表现层，不接受客户端自报命中。
    AdaptCombatEvent(Event);
}

void UMobaPresentationClientSubsystem::UnbindCombat()
{
    if (UGamePlatformCombatComponent* Combat = BoundCombatComponent.Get())
    {
        Combat->OnCombatEvent.RemoveDynamic(
            this,
            &UMobaPresentationClientSubsystem::HandleCombatEvent);
    }
    BoundCombatComponent.Reset();
}

void UMobaPresentationClientSubsystem::BindArena(
    AGamePlatformArenaGameState* GameState)
{
    if (!GameState || GameState == BoundArenaGameState.Get())
    {
        return;
    }

    UnbindArena();
    BoundArenaGameState = GameState;

    ArenaPhaseHandle = GameState->OnArenaPhaseChanged.AddLambda(
        [this](EGamePlatformArenaMatchPhase /*Phase*/, int32 Revision)
        {
            HandleArenaPhaseChangedTyped(Revision);
        });
    ArenaTeamsHandle = GameState->OnArenaTeamStatesChanged.AddLambda(
        [this](const TArray<FGamePlatformArenaTeamState>&)
        {
            HandleArenaTeamStatesChanged();
        });
    ArenaObjectivesHandle = GameState->OnArenaObjectiveStatesChanged.AddLambda(
        [this](const TArray<FGamePlatformArenaObjectiveState>&)
        {
            HandleArenaObjectiveStatesChanged();
        });
    ArenaResultHandle = GameState->OnArenaResultChanged.AddLambda(
        [this](const FGamePlatformArenaResultSummary&)
        {
            HandleArenaResultChanged();
        });

    // Late Join：不重放Match.Start/End等瞬时历史，只建立Phase基线。
    LastPhaseRevision = GameState->PhaseRevision;

    // 当前比分属于持续状态，可用Persistent请求恢复，而不是伪造历史Score Popup。
    for (const FGamePlatformArenaTeamState& Team : GameState->TeamStates)
    {
        TeamRevisions.Add(Team.TeamId, Team.Revision);
        FMobaPresentationArenaFact ArenaFact;
        ArenaFact.Identity.FactId = MakeArenaFactId(
            GameState->MatchIdPublic + Team.TeamId.ToString(),
            Team.Revision,
            0x53434F52u);
        ArenaFact.Identity.Revision = Team.Revision;
        ArenaFact.Identity.WorldGeneration = WorldGeneration;
        ArenaFact.Type = EMobaPresentationArenaFactType::ScoreChanged;
        ArenaFact.MatchId = GameState->MatchIdPublic;
        ArenaFact.ArenaModeId = GameState->ArenaModeId;
        ArenaFact.TeamId = Team.TeamId;
        ArenaFact.Value = Team.Score;
        FMobaPresentationAdaptedFact Fact =
            FMobaPresentationFactAdapters::FromArenaFact(ArenaFact);
        Fact.bTransient = false;
        Fact.Lifetime = EGamePlatformPresentationLifetime::Persistent;
        RecoverPersistentFact(MoveTemp(Fact));
    }

    for (const FGamePlatformArenaObjectiveState& Objective : GameState->ObjectiveStates)
    {
        ObjectiveRevisions.Add(Objective.ObjectiveId, Objective.Revision);
    }

    RefreshArenaPlayerBindings();
}

void UMobaPresentationClientSubsystem::UnbindArena()
{
    if (AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get())
    {
        if (ArenaPhaseHandle.IsValid()) GameState->OnArenaPhaseChanged.Remove(ArenaPhaseHandle);
        if (ArenaTeamsHandle.IsValid()) GameState->OnArenaTeamStatesChanged.Remove(ArenaTeamsHandle);
        if (ArenaObjectivesHandle.IsValid()) GameState->OnArenaObjectiveStatesChanged.Remove(ArenaObjectivesHandle);
        if (ArenaResultHandle.IsValid()) GameState->OnArenaResultChanged.Remove(ArenaResultHandle);
    }

    for (const TPair<TWeakObjectPtr<AGamePlatformArenaPlayerState>, FDelegateHandle>& Pair : ArenaPlayerHandles)
    {
        if (AGamePlatformArenaPlayerState* PlayerState = Pair.Key.Get())
        {
            PlayerState->OnArenaStatsChanged.Remove(Pair.Value);
        }
    }

    ArenaPhaseHandle.Reset();
    ArenaTeamsHandle.Reset();
    ArenaObjectivesHandle.Reset();
    ArenaResultHandle.Reset();
    ArenaPlayerHandles.Reset();
    ArenaPlayerSnapshots.Reset();
    BoundArenaGameState.Reset();
}

void UMobaPresentationClientSubsystem::RefreshArenaPlayerBindings()
{
    AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get();
    if (!GameState)
    {
        return;
    }

    for (APlayerState* BasePlayerState : GameState->PlayerArray)
    {
        AGamePlatformArenaPlayerState* PlayerState =
            Cast<AGamePlatformArenaPlayerState>(BasePlayerState);
        if (!PlayerState || ArenaPlayerHandles.Contains(PlayerState))
        {
            continue;
        }

        const FDelegateHandle Handle = PlayerState->OnArenaStatsChanged.AddUObject(
            this,
            &UMobaPresentationClientSubsystem::HandleArenaPlayerStatsChanged);
        ArenaPlayerHandles.Add(PlayerState, Handle);

        FPlayerSnapshot Snapshot;
        Snapshot.StatsRevision = PlayerState->StatsRevision;
        Snapshot.Score = PlayerState->ArenaScore;
        Snapshot.ObjectiveScore = PlayerState->ObjectiveScore;
        ArenaPlayerSnapshots.Add(PlayerState, Snapshot);
    }
}

void UMobaPresentationClientSubsystem::HandleArenaPhaseChangedTyped(int32 Revision)
{
    AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get();
    if (!GameState || Revision <= LastPhaseRevision)
    {
        return;
    }
    LastPhaseRevision = Revision;

    FMobaPresentationArenaFact Fact;
    Fact.Identity.FactId = MakeArenaFactId(
        GameState->MatchIdPublic + TEXT(":Phase"),
        Revision,
        0x50484153u);
    Fact.Identity.Revision = Revision;
    Fact.Identity.WorldGeneration = WorldGeneration;
    Fact.MatchId = GameState->MatchIdPublic;
    Fact.ArenaModeId = GameState->ArenaModeId;

    if (GameState->MatchPhase == EGamePlatformArenaMatchPhase::InProgress)
    {
        Fact.Type = EMobaPresentationArenaFactType::MatchStart;
        AdaptArenaFact(Fact);
    }
    else if (
        GameState->MatchPhase == EGamePlatformArenaMatchPhase::Completed ||
        GameState->MatchPhase == EGamePlatformArenaMatchPhase::Aborted ||
        GameState->MatchPhase == EGamePlatformArenaMatchPhase::Failed)
    {
        Fact.Type = EMobaPresentationArenaFactType::MatchEnd;
        AdaptArenaFact(Fact);
    }
}

void UMobaPresentationClientSubsystem::HandleArenaTeamStatesChanged()
{
    AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get();
    if (!GameState)
    {
        return;
    }

    for (const FGamePlatformArenaTeamState& Team : GameState->TeamStates)
    {
        const int32 PreviousRevision = TeamRevisions.FindRef(Team.TeamId);
        if (Team.Revision <= PreviousRevision)
        {
            continue;
        }
        TeamRevisions.Add(Team.TeamId, Team.Revision);

        FMobaPresentationArenaFact Fact;
        Fact.Identity.FactId = MakeArenaFactId(
            GameState->MatchIdPublic + Team.TeamId.ToString(),
            Team.Revision,
            0x53434F52u);
        Fact.Identity.Revision = Team.Revision;
        Fact.Identity.WorldGeneration = WorldGeneration;
        Fact.Type = EMobaPresentationArenaFactType::ScoreChanged;
        Fact.MatchId = GameState->MatchIdPublic;
        Fact.ArenaModeId = GameState->ArenaModeId;
        Fact.TeamId = Team.TeamId;
        Fact.Value = Team.Score;
        AdaptArenaFact(Fact);
    }

    RefreshArenaPlayerBindings();
}

void UMobaPresentationClientSubsystem::HandleArenaObjectiveStatesChanged()
{
    AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get();
    if (!GameState)
    {
        return;
    }

    for (const FGamePlatformArenaObjectiveState& Objective : GameState->ObjectiveStates)
    {
        const int32 PreviousRevision = ObjectiveRevisions.FindRef(Objective.ObjectiveId);
        if (Objective.Revision <= PreviousRevision)
        {
            continue;
        }
        ObjectiveRevisions.Add(Objective.ObjectiveId, Objective.Revision);

        FMobaPresentationArenaFact Fact;
        Fact.Identity.FactId = MakeArenaFactId(
            GameState->MatchIdPublic + Objective.ObjectiveId.ToString(),
            Objective.Revision,
            0x4F424A45u);
        Fact.Identity.Revision = Objective.Revision;
        Fact.Identity.WorldGeneration = WorldGeneration;
        Fact.Type = EMobaPresentationArenaFactType::ObjectiveCompleted;
        Fact.MatchId = GameState->MatchIdPublic;
        Fact.ArenaModeId = GameState->ArenaModeId;
        Fact.TeamId = Objective.OwningTeamId;
        Fact.Value = Objective.Value;
        AdaptArenaFact(Fact);
    }
}

void UMobaPresentationClientSubsystem::HandleArenaResultChanged()
{
    // ResultSummary本身不再额外触发Match.End，避免与终态Phase产生双表现。
}

void UMobaPresentationClientSubsystem::HandleArenaPlayerStatsChanged(
    AGamePlatformArenaPlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    FPlayerSnapshot& Snapshot = ArenaPlayerSnapshots.FindOrAdd(PlayerState);
    if (PlayerState->StatsRevision <= Snapshot.StatsRevision)
    {
        return;
    }

    const bool bScoreChanged =
        PlayerState->ArenaScore != Snapshot.Score ||
        PlayerState->ObjectiveScore != Snapshot.ObjectiveScore;

    Snapshot.StatsRevision = PlayerState->StatsRevision;
    Snapshot.Score = PlayerState->ArenaScore;
    Snapshot.ObjectiveScore = PlayerState->ObjectiveScore;

    if (!bScoreChanged)
    {
        return;
    }

    AGamePlatformArenaGameState* GameState = BoundArenaGameState.Get();
    if (!GameState)
    {
        return;
    }

    FMobaPresentationArenaFact Fact;
    Fact.Identity.FactId = MakeArenaFactId(
        GameState->MatchIdPublic + PlayerState->PlayerIdPublic,
        PlayerState->StatsRevision,
        0x504C4159u);
    Fact.Identity.Revision = PlayerState->StatsRevision;
    Fact.Identity.WorldGeneration = WorldGeneration;
    Fact.Type = EMobaPresentationArenaFactType::ScoreChanged;
    Fact.MatchId = GameState->MatchIdPublic;
    Fact.ArenaModeId = GameState->ArenaModeId;
    Fact.TeamId = PlayerState->TeamId;
    Fact.Value = PlayerState->ArenaScore;
    FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromArenaFact(Fact);
    Adapted.Context.TargetEntityId = PlayerState->PlayerIdPublic;
    SubmitAdaptedFact(MoveTemp(Adapted));
}

void UMobaPresentationClientSubsystem::HandleCombatEvent(
    const FGamePlatformCombatEvent& Event)
{
    AdaptCombatEvent(Event);
}

void UMobaPresentationClientSubsystem::ConfigureHitFeedbackProfile(
    UGamePlatformHitFeedbackProfile* LoadedProfile,
    FName VFXDefinitionId,
    FName SFXDefinitionId)
{
    if (bClosing && LoadedProfile) return;
    ++HitFeedbackConfigurationGeneration;
    // 注入的Profile已由上层通过GamePlatformData完成软资产加载与合法性校验。
    // 命中热路径只Get()已经在内存中的CameraShake/Overlay资源，绝不LoadSynchronous。
    LoadedHitFeedbackProfile = LoadedProfile;
    HitVFXDefinitionId = LoadedProfile ? VFXDefinitionId : NAME_None;
    HitSFXDefinitionId = LoadedProfile ? SFXDefinitionId : NAME_None;
    HitFeedbackTuning = LoadedProfile
        ? LoadedProfile->Tuning
        : FGamePlatformHitFeedbackTuning{};
}

void UMobaPresentationClientSubsystem::AdaptCombatEvent(
    const FGamePlatformCombatEvent& Event)
{
    if (bClosing) return;
    RefreshBindings();
    const uint64 ExpectedBindingGeneration = BindingGeneration;
    const TWeakObjectPtr<UWorld> ExpectedWorld = BoundWorld;
    if (!ExpectedWorld.IsValid() || ExpectedWorld->bIsTearingDown ||
        (IsValid(Event.TargetActor) && Event.TargetActor->GetWorld() != ExpectedWorld.Get()) ||
        (IsValid(Event.SourceActor) && Event.SourceActor->GetWorld() != ExpectedWorld.Get())) return;
    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    for (FMobaPresentationAdaptedFact& Fact : Facts)
    {
        SubmitAdaptedFact(MoveTemp(Fact));
        if (bClosing || BindingGeneration != ExpectedBindingGeneration || BoundWorld != ExpectedWorld) return;
    }
    // 权威Spec携带真实技能ID时按技能反馈；其他命中默认轻击，不依据伤害数字推断重击。
    const EMobaHitFeedbackContact Contact = Event.SourceAbilityId.IsNone()
        ? EMobaHitFeedbackContact::Light : EMobaHitFeedbackContact::Skill;
    ApplyVisualFeedbackForConfirmedHit(Event, Contact, 1);
}

void UMobaPresentationClientSubsystem::ApplyVisualFeedbackForConfirmedHit(
    const FGamePlatformCombatEvent& Event,
    EMobaHitFeedbackContact Contact,
    int32 ComboStep)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (bClosing || !World || World != BoundWorld.Get() || World->bIsTearingDown || !Event.EventId.IsValid() ||
        Event.EventType != EGamePlatformCombatEventType::Damage ||
        !IsValid(Event.TargetActor) || Event.TargetActor->GetWorld() != World ||
        (IsValid(Event.SourceActor) && Event.SourceActor->GetWorld() != World) ||
        RenderedHitEvents.Contains(Event.EventId) || ResolvingHitEvents.Contains(Event.EventId))
    {
        return;
    }
    const TStrongObjectPtr<UMobaPresentationClientSubsystem> KeepService(this);
    const TStrongObjectPtr<ULocalPlayer> KeepPlayer(LocalPlayer);
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const TStrongObjectPtr<AActor> KeepTarget(Event.TargetActor.Get());
    const TStrongObjectPtr<AActor> KeepSource(Event.SourceActor.Get());
    const uint64 ExpectedBinding = BindingGeneration;
    const uint64 ExpectedConfiguration = HitFeedbackConfigurationGeneration;
    const auto IsCurrent = [this, World, LocalPlayer, ExpectedBinding, ExpectedConfiguration, &Event]
    {
        return !bClosing && BindingGeneration == ExpectedBinding &&
            HitFeedbackConfigurationGeneration == ExpectedConfiguration && BoundWorld.Get() == World &&
            LocalPlayer->GetWorld() == World && !World->bIsTearingDown && IsValid(Event.TargetActor);
    };
    ResolvingHitEvents.Add(Event.EventId);
    ON_SCOPE_EXIT
    {
        if (BindingGeneration == ExpectedBinding) ResolvingHitEvents.Remove(Event.EventId);
    };

    // 每次命中独立查询项目层已加载的技能反馈映射，不能把上一击的Profile复用到其他英雄。
    // 本地副本保活正在执行的闭包；回调若自行替换Resolver，不销毁自身栈内函数。
    const FMobaHitFeedbackResolver Resolver = HitFeedbackResolver;
    FMobaResolvedHitFeedbackConfiguration ResolvedConfiguration;
    const bool bHasSpecificProfile = Resolver &&
        Resolver(Event, ResolvedConfiguration) &&
        ResolvedConfiguration.LoadedProfile.IsValid();
    if (!IsCurrent()) return;
    // 若项目Resolver已经安装但没有匹配到已加载技能资源，不能复用上一击的英雄特化资源。
    // 仅保留平台通用参数反馈，不伪造VFX/SFX定义，更不能同步加载未命中的资源。
    const bool bProjectFallback = static_cast<bool>(Resolver) && !bHasSpecificProfile;
    UGamePlatformHitFeedbackProfile* CurrentProfile = bHasSpecificProfile
        ? ResolvedConfiguration.LoadedProfile.Get()
        : (bProjectFallback ? nullptr : LoadedHitFeedbackProfile.Get());
    const TStrongObjectPtr<UGamePlatformHitFeedbackProfile> KeepProfile(CurrentProfile);
    const FGamePlatformHitFeedbackTuning CurrentTuning = IsValid(CurrentProfile)
        ? CurrentProfile->Tuning : HitFeedbackTuning;
    const FName CurrentVFXDefinitionId = bHasSpecificProfile
        ? ResolvedConfiguration.VFXDefinitionId
        : (bProjectFallback ? NAME_None : HitVFXDefinitionId);
    const FName CurrentSFXDefinitionId = bHasSpecificProfile
        ? ResolvedConfiguration.SFXDefinitionId
        : (bProjectFallback ? NAME_None : HitSFXDefinitionId);

    FMobaHitFeedbackInput Input;
    Input.Contact = Contact;
    Input.ComboStep = ComboStep;
    APawn* LocalPawn = nullptr;
    if (APlayerController* Controller = LocalPlayer->GetPlayerController(World))
    {
        LocalPawn = Controller->GetPawn();
        Input.bLocalVictim = LocalPawn == Event.TargetActor;
    }

    const FMobaHitFeedbackDecision Decision =
        FMobaHitFeedbackPolicy::Evaluate(Input, CurrentTuning);
    if (!Decision.bHasContact)
    {
        return; // 挥空/闪避不触发接触层，但攻击挥击动画仍由原技能系统继续播放。
    }

    // 在发出任何层的表现之前登记EventId，防止本地Pawn与远端确认两条委托重复触发。
    RenderedHitEvents.Add(Event.EventId);
    RenderedHitOrder.Add(Event.EventId);
    if (RenderedHitOrder.Num() > MaxRenderedHitEvents)
    {
        RenderedHitEvents.Remove(RenderedHitOrder[0]);
        RenderedHitOrder.RemoveAt(0, 1, EAllowShrinking::No);
    }

    auto FindMesh = [](AActor* Actor) -> USkeletalMeshComponent*
    {
        if (!IsValid(Actor))
        {
            return nullptr;
        }
        if (ACharacter* Character = Cast<ACharacter>(Actor))
        {
            return Character->GetMesh();
        }
        return Actor->FindComponentByClass<USkeletalMeshComponent>();
    };
    USkeletalMeshComponent* SourceMesh = FindMesh(Event.SourceActor);
    USkeletalMeshComponent* TargetMesh = FindMesh(Event.TargetActor);
    // 外部反馈可同步移除组件并GC；当前调用持有其原Mesh，后续层仍须通过作用域/有效性检查。
    const TStrongObjectPtr<USkeletalMeshComponent> KeepSourceMesh(SourceMesh);
    const TStrongObjectPtr<USkeletalMeshComponent> KeepTargetMesh(TargetMesh);

    // 局部顿帧可以设置为0，同时继续触发其余层；不能把0帧当作整套反馈关闭。
    if (Decision.VisualHitstopFrames > 0)
    {
        if (UGamePlatformLocalHitstopSubsystem* Hitstop =
            LocalPlayer->GetSubsystem<UGamePlatformLocalHitstopSubsystem>())
        {
            Hitstop->ApplyVisualHitstop(
                Event.EventId, SourceMesh, TargetMesh, Decision.VisualHitstopFrames);
            if (!IsCurrent()) return;
        }
    }

    if (IsValid(CurrentProfile))
    {
        // 模型高亮仅替换受击Mesh的Overlay，最多参考2帧；缺失材质安全跳过。
        if (Decision.FlashSeconds > 0.0f && IsValid(TargetMesh))
        {
            UMaterialInterface* Overlay =
                CurrentProfile->HitFlashOverlayMaterial.Get();
            if (IsValid(Overlay))
            {
                if (UGamePlatformHitFlashWorldSubsystem* Flash =
                    World->GetSubsystem<UGamePlatformHitFlashWorldSubsystem>())
                {
                    Flash->PlayHitFlash(
                        Event.EventId, TargetMesh, Overlay, Decision.FlashSeconds);
                    if (!IsCurrent()) return;
                }
            }
        }

        // 仅命中与受击双方的本地玩家产生摄像机冲击，其余观战者不被迫震屏。
        if (LocalPawn && (LocalPawn == Event.TargetActor ||
            LocalPawn == Event.SourceActor))
        {
            if (UGamePlatformCameraHitFeedbackSubsystem* Camera =
                LocalPlayer->GetSubsystem<UGamePlatformCameraHitFeedbackSubsystem>())
            {
                UClass* PreloadedShakeClass =
                    CurrentProfile->CameraShakeClass.Get();
                if (PreloadedShakeClass)
                {
                    const float Emphasis = Input.bLocalVictim ? 0.8f : 0.25f;
                    Camera->PlayHitCameraShake(
                        Event.EventId,
                        TSubclassOf<UCameraShakeBase>(PreloadedShakeClass),
                        Decision.Strength *
                            CurrentTuning.CameraStrength * Emphasis);
                    if (!IsCurrent()) return;
                }
            }
        }
    }

    // 两个独立Provider请求复用唯一GamePlatformPresentation执行器，
    // 同一次Hit使用有区别的稳定Layer GUID，防止VFX与SFX相互吞掉或互相去重。
    UGamePlatformPresentationClientSubsystem* Presentation =
        LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    if (!Presentation)
    {
        return;
    }

    auto SubmitLayer = [&](FName Provider, FName DefinitionId, uint32 LayerSalt,
                           float LayerStrength)
    {
        if (!IsCurrent() || DefinitionId.IsNone() || LayerStrength <= 0.0f)
        {
            return;
        }
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid(
            Event.EventId.A ^ LayerSalt, Event.EventId.B,
            Event.EventId.C, Event.EventId.D);
        Request.SemanticTag = MobaPresentationTags::Combat_Hit;
        Request.ContextId = FName(TEXT("Combat.Hit"));
        Request.ProviderChannel = Provider;
        Request.DefinitionId = DefinitionId;
        Request.SourceId = IsValid(Event.SourceActor)
            ? Event.SourceActor->GetFName() : NAME_None;
        Request.TargetId = Event.TargetActor->GetFName();
        Request.SourceLocation = Event.ImpactPoint;
        Request.TargetLocation = Event.TargetActor->GetActorLocation();
        Request.ImpactLocation = Event.ImpactPoint;
        Request.ImpactNormal = Event.ImpactNormal;
        Request.Magnitude = FMath::Clamp(LayerStrength, 0.0f, 2.0f);
        Request.ContextTags = Event.ResultTags;
        // 由同次Submit刷新平台世界世代，不能读旅行前缓存值。
        Request.WorldGeneration = 0;
        Request.Context.WorldGeneration = 0;
        Request.RequestGeneration = ++RequestGeneration;
        Request.PredictionState = EGamePlatformPresentationPredictionState::Confirmed;
        Request.Priority = Contact == EMobaHitFeedbackContact::Heavy
            ? EGamePlatformPresentationPriority::High
            : EGamePlatformPresentationPriority::Normal;
        Presentation->Submit(Request); // 可选表现失败不改变GAS权威结算。
    };

    SubmitLayer(
        TEXT("VFX"), CurrentVFXDefinitionId, 0x56465831u,
        Decision.Strength * CurrentTuning.VFXStrength);
    SubmitLayer(
        TEXT("SFX"), CurrentSFXDefinitionId, 0x53465831u,
        Decision.Strength * CurrentTuning.AudioStrength);
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::AdaptAbilityFact(
    const FMobaPresentationAbilityFact& Fact)
{
    return SubmitAdaptedFact(
        FMobaPresentationFactAdapters::FromAbilityFact(Fact));
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::AdaptStatusFact(
    const FMobaPresentationStatusFact& Fact)
{
    return SubmitAdaptedFact(
        FMobaPresentationFactAdapters::FromStatusFact(Fact));
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::AdaptCharacterFact(
    const FMobaPresentationCharacterFact& Fact)
{
    return SubmitAdaptedFact(
        FMobaPresentationFactAdapters::FromCharacterFact(Fact));
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::AdaptArenaFact(
    const FMobaPresentationArenaFact& Fact)
{
    return SubmitAdaptedFact(
        FMobaPresentationFactAdapters::FromArenaFact(Fact));
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::RecoverPersistentFact(
    FMobaPresentationAdaptedFact Fact)
{
    if (Fact.bTransient || FMobaPresentationSemanticRegistry::IsTransient(Fact.Semantic))
    {
        ++StaleDropCount;
        return EGamePlatformPresentationSubmitResult::StaleWorld;
    }

    Fact.Lifetime = EGamePlatformPresentationLifetime::Persistent;
    return SubmitAdaptedFact(MoveTemp(Fact));
}

bool UMobaPresentationClientSubsystem::RememberFact(
    const FMobaPresentationFactIdentity& Identity)
{
    if (!Identity.FactId.IsValid())
    {
        return false;
    }

    if (Identity.bPredicted && !Identity.bConfirmed)
    {
        if (PredictedFacts.Contains(Identity.FactId) ||
            ConfirmedFacts.Contains(Identity.FactId))
        {
            ++DuplicateDropCount;
            return false;
        }
        PredictedFacts.Add(Identity.FactId);
    }
    else
    {
        if (ConfirmedFacts.Contains(Identity.FactId))
        {
            ++DuplicateDropCount;
            return false;
        }
        if (PredictedFacts.Remove(Identity.FactId) > 0)
        {
            ConfirmedFacts.Add(Identity.FactId);
            FactOrder.Add(Identity.FactId);
            ++PredictedConfirmationCount;
            // 已播放低风险预测表现；确认只升级内部状态，不二次播放。
            return false;
        }
        ConfirmedFacts.Add(Identity.FactId);
    }

    FactOrder.Add(Identity.FactId);
    while (FactOrder.Num() > MaxRememberedFacts)
    {
        const FGuid Evicted = FactOrder[0];
        FactOrder.RemoveAt(0, 1, EAllowShrinking::No);
        PredictedFacts.Remove(Evicted);
        ConfirmedFacts.Remove(Evicted);
    }
    return true;
}

void UMobaPresentationClientSubsystem::ApplyContextContributors(
    FMobaPresentationContext& Context) const
{
    TArray<FName> Ids;
    ContextContributors.GetKeys(Ids);
    Ids.Sort([](const FName& A, const FName& B)
    {
        return A.LexicalLess(B);
    });

    for (const FName& Id : Ids)
    {
        if (const TSharedPtr<IMobaPresentationContextContributor>* Contributor =
            ContextContributors.Find(Id))
        {
            if (Contributor->IsValid())
            {
                (*Contributor)->Contribute(Context);
            }
        }
    }
}

bool UMobaPresentationClientSubsystem::PrepareFact(
    FMobaPresentationAdaptedFact& Fact)
{
    if (bClosing) return false;
    UWorld* CurrentWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (CurrentWorld != BoundWorld.Get())
    {
        ResetWorldState(CurrentWorld);
    }

    if (!Fact.IsValid())
    {
        return false;
    }

    if (Fact.Context.WorldGeneration == 0)
    {
        Fact.Context.WorldGeneration = WorldGeneration;
        Fact.Identity.WorldGeneration = WorldGeneration;
    }
    else if (Fact.Context.WorldGeneration != WorldGeneration)
    {
        ++StaleDropCount;
        return false;
    }

    const FString AvatarKey = !Fact.Context.TargetEntityId.IsEmpty()
        ? Fact.Context.TargetEntityId
        : Fact.Context.TargetCharacterId;
    if (!AvatarKey.IsEmpty() && Fact.Context.AvatarGeneration > 0)
    {
        const int32 Latest = LatestAvatarGeneration.FindRef(AvatarKey);
        if (Latest > Fact.Context.AvatarGeneration)
        {
            ++StaleDropCount;
            return false;
        }
        LatestAvatarGeneration.Add(
            AvatarKey,
            FMath::Max(Latest, Fact.Context.AvatarGeneration));
    }

    ApplyContextContributors(Fact.Context);
    Fact.Context.RequestGeneration = ++RequestGeneration;
    return true;
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::SubmitAdaptedFact(
    FMobaPresentationAdaptedFact Fact)
{
    if (!PrepareFact(Fact))
    {
        return EGamePlatformPresentationSubmitResult::InvalidRequest;
    }
    if (!RememberFact(Fact.Identity))
    {
        return EGamePlatformPresentationSubmitResult::Submitted;
    }

    UGamePlatformPresentationClientSubsystem* Coordinator =
        GetLocalPlayer()
        ? GetLocalPlayer()->GetSubsystem<UGamePlatformPresentationClientSubsystem>()
        : nullptr;
    if (!Coordinator)
    {
        PredictedFacts.Remove(Fact.Identity.FactId); ConfirmedFacts.Remove(Fact.Identity.FactId);
        FactOrder.Remove(Fact.Identity.FactId);
        ++ProviderMissingCount;
        return EGamePlatformPresentationSubmitResult::ProviderMissing;
    }

    FGamePlatformPresentationRequest Request =
        FMobaPresentationRequestBuilder::Build(
            Fact,
            Fact.Context.RequestGeneration);
    // 已在PrepareFact校验MOBA事实所属World；平台世代由同一次Submit刷新后原子补齐，不读取旅行前缓存。
    Request.WorldGeneration = 0;
    Request.Context.WorldGeneration = 0;

    const EGamePlatformPresentationSubmitResult Result =
        Coordinator->Submit(Request);
    // Submitted仅表示实际受理；同步失败不得通过重复事实被升级成受理成功。
    if (Result != EGamePlatformPresentationSubmitResult::Submitted)
    {
        PredictedFacts.Remove(Fact.Identity.FactId); ConfirmedFacts.Remove(Fact.Identity.FactId);
        FactOrder.Remove(Fact.Identity.FactId);
    }
    if (Result == EGamePlatformPresentationSubmitResult::ProviderMissing)
    {
        ++ProviderMissingCount;
    }
    return Result;
}

FGuid UMobaPresentationClientSubsystem::MakeArenaFactId(
    const FString& Scope,
    int32 Revision,
    uint32 Salt) const
{
    return FMobaPresentationFactAdapters::MakeRevisionFactId(
        Scope,
        Revision,
        Salt);
}
