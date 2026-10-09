// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// MOBA本地玩家事实接线：事件驱动处理迟到Controller/Pawn/GameState与重生；不持有玩法权威或资产租约。
#include "MobaPresentationClientSubsystem.h"

#include "Adapters/MobaPresentationFactAdapters.h"
#include "Components/GamePlatformCombatComponent.h"
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
        BoundWorld.Reset();
        ++WorldGeneration;
        PredictedFacts.Reset();
        ConfirmedFacts.Reset();
        FactOrder.Reset();
        LatestAvatarGeneration.Reset();
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

void UMobaPresentationClientSubsystem::AdaptCombatEvent(
    const FGamePlatformCombatEvent& Event)
{
    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    for (FMobaPresentationAdaptedFact& Fact : Facts)
    {
        SubmitAdaptedFact(MoveTemp(Fact));
    }
}

EGamePlatformPresentationSubmitResult UMobaPresentationClientSubsystem::AdaptCriticalFact(
    const FMobaPresentationCriticalFact& Fact)
{
    return SubmitAdaptedFact(
        FMobaPresentationFactAdapters::FromCriticalFact(Fact));
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
