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
    BoundWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;

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
    UnbindCombatFeedbackWorld();
    UnbindCombat();
    UnbindArena();

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
    ConfigureHitFeedbackProfile(nullptr, NAME_None, NAME_None);
}

bool UMobaPresentationClientSubsystem::RegisterContextContributor(
    FName ContributorId,
    TSharedRef<IMobaPresentationContextContributor> Contributor)
{
    if (ContributorId.IsNone() || ContextContributors.Contains(ContributorId))
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
    UnbindCombatFeedbackWorld();
    UnbindCombat();
    UnbindArena();

    BoundWorld = NewWorld;
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

    RefreshBindings();    RenderedHitEvents.Reset();
    RenderedHitOrder.Reset();
    ConfigureHitFeedbackProfile(nullptr, NAME_None, NAME_None);

}

void UMobaPresentationClientSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
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
        UnbindCombat();
        UnbindArena();
        UnbindCombatFeedbackWorld();
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
    if (!World)
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

void UMobaPresentationClientSubsystem::BindCombatFeedbackWorld(UWorld& World)
{
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
        CombatWorldFeedbackHandle = Bus->OnConfirmedFeedback().AddUObject(
            this, &UMobaPresentationClientSubsystem::HandleConfirmedNetworkCombatEvent);
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
    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    for (FMobaPresentationAdaptedFact& Fact : Facts)
    {
        SubmitAdaptedFact(MoveTemp(Fact));
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
    if (!World || !Event.EventId.IsValid() ||
        Event.EventType != EGamePlatformCombatEventType::Damage ||
        !IsValid(Event.TargetActor) || Event.TargetActor->GetWorld() != World ||
        (IsValid(Event.SourceActor) && Event.SourceActor->GetWorld() != World) ||
        RenderedHitEvents.Contains(Event.EventId))
    {
        return;
    }

    // 每次命中独立查询项目层已加载的技能反馈映射，不能把上一击的Profile复用到其他英雄。
    FMobaResolvedHitFeedbackConfiguration ResolvedConfiguration;
    const bool bHasSpecificProfile = HitFeedbackResolver &&
        HitFeedbackResolver(Event, ResolvedConfiguration) &&
        ResolvedConfiguration.LoadedProfile.IsValid();
    UGamePlatformHitFeedbackProfile* CurrentProfile = bHasSpecificProfile
        ? ResolvedConfiguration.LoadedProfile.Get()
        : LoadedHitFeedbackProfile.Get();
    const FGamePlatformHitFeedbackTuning& CurrentTuning = IsValid(CurrentProfile)
        ? CurrentProfile->Tuning : HitFeedbackTuning;
    const FName CurrentVFXDefinitionId = bHasSpecificProfile
        ? ResolvedConfiguration.VFXDefinitionId : HitVFXDefinitionId;
    const FName CurrentSFXDefinitionId = bHasSpecificProfile
        ? ResolvedConfiguration.SFXDefinitionId : HitSFXDefinitionId;

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

    // 局部顿帧可以设置为0，同时继续触发其余层；不能把0帧当作整套反馈关闭。
    if (Decision.VisualHitstopFrames > 0)
    {
        if (UGamePlatformLocalHitstopSubsystem* Hitstop =
            LocalPlayer->GetSubsystem<UGamePlatformLocalHitstopSubsystem>())
        {
            Hitstop->ApplyVisualHitstop(
                Event.EventId, SourceMesh, TargetMesh, Decision.VisualHitstopFrames);
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
        if (DefinitionId.IsNone() || LayerStrength <= 0.0f)
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
        Request.WorldGeneration = Presentation->GetWorldGeneration();
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
        ++ProviderMissingCount;
        return EGamePlatformPresentationSubmitResult::ProviderMissing;
    }

    FGamePlatformPresentationRequest Request =
        FMobaPresentationRequestBuilder::Build(
            Fact,
            Fact.Context.RequestGeneration);
    Request.WorldGeneration = Coordinator->GetWorldGeneration();

    const EGamePlatformPresentationSubmitResult Result =
        Coordinator->Submit(Request);
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
