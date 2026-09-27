#include "Framework/GamePlatformArenaGameMode.h"

#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
/** 单场比赛可信事件幂等记录上限；正常1v1~5v5比赛远低于该值。达到上限时Fail Closed，避免异常事件源无界占用内存。 */
constexpr int32 MaxProcessedTrustedEvents = 65536;
constexpr int32 MaxTrustedEventIdLength = 128;
}

AGamePlatformArenaGameMode::AGamePlatformArenaGameMode()
{
    GameStateClass = AGamePlatformArenaGameState::StaticClass();
    PlayerControllerClass = AGamePlatformArenaPlayerController::StaticClass();
    PlayerStateClass = AGamePlatformArenaPlayerState::StaticClass();
}

void AGamePlatformArenaGameMode::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    Transition(EGamePlatformArenaMatchPhase::WaitingAssignment, Error);
}

void AGamePlatformArenaGameMode::Logout(AController* Exiting)
{
    if (HasAuthority() && Exiting != nullptr)
    {
        if (const AGamePlatformArenaPlayerState* State = Exiting->GetPlayerState<AGamePlatformArenaPlayerState>())
        {
            if (!State->PlayerIdPublic.IsEmpty())
            {
                MarkPlayerDisconnected(State->PlayerIdPublic);
            }
        }
    }
    Super::Logout(Exiting);
}

bool AGamePlatformArenaGameMode::Transition(
    EGamePlatformArenaMatchPhase NewPhase,
    FString& OutError,
    double DurationSeconds)
{
    if (!HasAuthority()) { OutError = TEXT("竞技阶段只能由服务器权威推进。"); return false; }
    if (!PhaseMachine.TryTransition(NewPhase, OutError)) { return false; }
    if (AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>())
    {
        ArenaState->AuthoritySetPhase(NewPhase, PhaseMachine.GetRevision(), DurationSeconds);
    }
    return true;
}

bool AGamePlatformArenaGameMode::ValidateAssignment(
    const FGamePlatformArenaAssignment& Assignment,
    const FGamePlatformArenaModeSpec& Mode,
    FString& OutError) const
{
    if (Assignment.MatchId.IsEmpty()) { OutError = TEXT("Assignment.MatchId不能为空。"); return false; }
    if (Assignment.ServerRole != FGamePlatformArenaBuiltinModes::MainArenaServerRole)
    {
        OutError = TEXT("Assignment必须绑定GameServer.Role.MainArena。");
        return false;
    }
    if (Assignment.ExperienceId != FName(TEXT("Experience.MainArena.Main")))
    {
        OutError = TEXT("Assignment必须绑定Experience.MainArena.Main。");
        return false;
    }
    if (Assignment.ArenaModeId != Mode.ArenaModeId || Assignment.MapId != Mode.MapId)
    {
        OutError = TEXT("Assignment ArenaModeId/MapId与ModeSpec不一致。");
        return false;
    }
    if (Assignment.TeamSize != Mode.TeamSize || Assignment.TotalPlayers != Mode.TotalPlayers)
    {
        OutError = TEXT("Assignment队伍规模与ArenaModeDefinition不一致。");
        return false;
    }
    if (Assignment.Roster.Num() != Mode.TotalPlayers)
    {
        OutError = TEXT("Assignment Roster人数与竞技模式总人数不一致。");
        return false;
    }

    TSet<FString> Players;
    TSet<FString> Characters;
    TMap<FName, int32> TeamCounts;
    TSet<int32> SlotIndexes;
    for (const FGamePlatformArenaRosterSlot& Slot : Assignment.Roster)
    {
        if (!Slot.IsValid()) { OutError = TEXT("Roster存在无效槽位。"); return false; }
        if (Players.Contains(Slot.PlayerId)) { OutError = TEXT("Roster存在重复PlayerId。"); return false; }
        if (Characters.Contains(Slot.CharacterId)) { OutError = TEXT("Roster存在重复CharacterId。"); return false; }
        if (SlotIndexes.Contains(Slot.SlotIndex)) { OutError = TEXT("Roster存在重复SlotIndex。"); return false; }
        Players.Add(Slot.PlayerId);
        Characters.Add(Slot.CharacterId);
        SlotIndexes.Add(Slot.SlotIndex);
        TeamCounts.FindOrAdd(Slot.TeamId)++;
    }
    if (TeamCounts.Num() != Mode.TeamCount)
    {
        OutError = TEXT("Roster队伍数量与ArenaModeDefinition不一致。");
        return false;
    }
    for (const TPair<FName, int32>& Pair : TeamCounts)
    {
        if (Pair.Value != Mode.TeamSize)
        {
            OutError = TEXT("Roster每队人数与ArenaModeDefinition不一致。");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool AGamePlatformArenaGameMode::ApplyAssignment(const FGamePlatformArenaAssignment& Assignment, FString& OutError)
{
    if (!HasAuthority()) { OutError = TEXT("Assignment只能由服务器应用。"); return false; }
    if (bAssignmentApplied) { OutError = TEXT("Assignment已应用，禁止覆盖当前比赛。"); return false; }

    const FGamePlatformArenaModeSpec* Mode = FGamePlatformArenaBuiltinModes::Find(Assignment.ArenaModeId);
    if (Mode == nullptr) { OutError = TEXT("未知ArenaModeId。"); return false; }
    return ApplyAssignmentWithModeSpec(Assignment, *Mode, OutError);
}

bool AGamePlatformArenaGameMode::ApplyAssignmentWithModeSpec(
    const FGamePlatformArenaAssignment& Assignment,
    const FGamePlatformArenaModeSpec& ModeSpec,
    FString& OutError)
{
    if (!HasAuthority()) { OutError = TEXT("Assignment只能由服务器应用。"); return false; }
    if (bAssignmentApplied) { OutError = TEXT("Assignment已应用，禁止覆盖当前比赛。"); return false; }
    if (ModeSpec.ArenaModeId != Assignment.ArenaModeId)
    {
        OutError = TEXT("项目ModeSpec与Assignment.ArenaModeId不一致。");
        return false;
    }
    if (!ValidateAssignment(Assignment, ModeSpec, OutError)) { return false; }

    CurrentAssignment = Assignment;
    ActiveModeSpec = ModeSpec;
    bAssignmentApplied = true;
    if (AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>())
    {
        ArenaState->AuthorityInitializeMatch(CurrentAssignment);
    }
    if (!Transition(EGamePlatformArenaMatchPhase::Preparing, OutError)) { return false; }
    return Transition(EGamePlatformArenaMatchPhase::WaitingPlayers, OutError);
}

bool AGamePlatformArenaGameMode::AdmitPlayer(
    AGamePlatformArenaPlayerState* PlayerState,
    const FGamePlatformArenaTransferTicketClaims& VerifiedTicket,
    FString& OutError)
{
    if (!HasAuthority() || PlayerState == nullptr) { OutError = TEXT("无效的服务器PlayerState。"); return false; }
    if (!bAssignmentApplied) { OutError = TEXT("Assignment尚未就绪。"); return false; }
    if (!VerifiedTicket.bConsumed) { OutError = TEXT("TransferTicket必须由后端验证并原子消费后才能准入。"); return false; }
    if (VerifiedTicket.MatchId != CurrentAssignment.MatchId || VerifiedTicket.DestinationServerId != CurrentAssignment.GameServerId)
    {
        OutError = TEXT("TransferTicket目标比赛或服务器不匹配。");
        return false;
    }
    if (VerifiedTicket.CharacterId.IsEmpty())
    {
        OutError = TEXT("TransferTicket缺少CharacterId。");
        return false;
    }
    if (VerifiedTicket.ExpiresAtUtc <= FDateTime::UtcNow()) { OutError = TEXT("TransferTicket已过期。"); return false; }

    const FGamePlatformArenaRosterSlot* Slot = CurrentAssignment.Roster.FindByPredicate(
        [&VerifiedTicket](const FGamePlatformArenaRosterSlot& Candidate)
        {
            return Candidate.PlayerId == VerifiedTicket.PlayerId;
        });
    if (Slot == nullptr) { OutError = TEXT("玩家不在Assignment Roster中。"); return false; }
    if (Slot->CharacterId != VerifiedTicket.CharacterId)
    {
        OutError = TEXT("TransferTicket CharacterId与Assignment Roster不一致。");
        return false;
    }
    if (PlayerStatesById.Contains(Slot->PlayerId))
    {
        OutError = TEXT("该Roster玩家已经准入；重连必须走Reconnect流程。");
        return false;
    }

    PlayerState->AuthorityApplyRosterIdentity(Slot->PlayerId, Slot->CharacterId, Slot->TeamId);
    PlayerStatesById.Add(Slot->PlayerId, PlayerState);
    OutError.Reset();
    BeginHeroSelectionIfReady();
    return true;
}

bool AGamePlatformArenaGameMode::RequestHeroSelection(
    AGamePlatformArenaPlayerState* PlayerState,
    const FString& HeroDefinitionId,
    const IGamePlatformArenaHeroEligibilityProvider* EligibilityProvider,
    FString& OutError)
{
    if (!HasAuthority() || PlayerState == nullptr) { OutError = TEXT("选人请求缺少服务器PlayerState。"); return false; }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::HeroSelection) { OutError = TEXT("当前阶段不允许选人。"); return false; }
    if (HeroDefinitionId.IsEmpty()) { OutError = TEXT("HeroDefinitionId不能为空。"); return false; }
    if (!PlayerStatesById.Contains(PlayerState->PlayerIdPublic)) { OutError = TEXT("只能为当前Roster中的自己选人。"); return false; }

    if (EligibilityProvider == nullptr)
    {
        OutError = TEXT("英雄资格Provider尚未配置，服务器按Fail Closed拒绝选人。");
        return false;
    }
    FString EligibilityReason;
    if (!EligibilityProvider->IsHeroEligible(PlayerState->PlayerIdPublic, HeroDefinitionId, ActiveModeSpec.ArenaModeId, EligibilityReason))
    {
        OutError = EligibilityReason.IsEmpty() ? TEXT("英雄竞技资格校验失败。") : EligibilityReason;
        return false;
    }

    if (!PlayerState->AuthoritySelectHero(HeroDefinitionId))
    {
        OutError = TEXT("英雄选择已锁定或请求无效。");
        return false;
    }
    OutError.Reset();
    BeginReadyCheckIfReady();
    return true;
}

bool AGamePlatformArenaGameMode::RequestReady(AGamePlatformArenaPlayerState* PlayerState, FString& OutError)
{
    if (!HasAuthority() || PlayerState == nullptr) { OutError = TEXT("Ready请求缺少服务器PlayerState。"); return false; }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::ReadyCheck) { OutError = TEXT("当前阶段不允许Ready。"); return false; }
    if (!PlayerStatesById.Contains(PlayerState->PlayerIdPublic)) { OutError = TEXT("只能Ready当前Roster中的自己。"); return false; }
    if (!PlayerState->AuthoritySetReady(true)) { OutError = TEXT("未选择英雄，不能Ready。"); return false; }
    OutError.Reset();
    BeginCountdownIfReady();
    return true;
}

bool AGamePlatformArenaGameMode::AreAllPlayersAdmitted() const
{
    return bAssignmentApplied && PlayerStatesById.Num() == ActiveModeSpec.TotalPlayers;
}

bool AGamePlatformArenaGameMode::AreAllPlayersSelected() const
{
    if (!AreAllPlayersAdmitted()) { return false; }
    for (const TPair<FString, TWeakObjectPtr<AGamePlatformArenaPlayerState>>& Pair : PlayerStatesById)
    {
        const AGamePlatformArenaPlayerState* State = Pair.Value.Get();
        if (State == nullptr || State->HeroDefinitionId.IsEmpty()) { return false; }
    }
    return true;
}

bool AGamePlatformArenaGameMode::AreAllPlayersReady() const
{
    if (!AreAllPlayersSelected()) { return false; }
    for (const TPair<FString, TWeakObjectPtr<AGamePlatformArenaPlayerState>>& Pair : PlayerStatesById)
    {
        const AGamePlatformArenaPlayerState* State = Pair.Value.Get();
        if (State == nullptr || !State->bReady) { return false; }
    }
    return true;
}

void AGamePlatformArenaGameMode::BeginHeroSelectionIfReady()
{
    if (!AreAllPlayersAdmitted() || PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::WaitingPlayers) { return; }
    FString Error;
    if (Transition(EGamePlatformArenaMatchPhase::HeroSelection, Error))
    {
        GetWorldTimerManager().SetTimer(
            PreMatchTimeoutTimer,
            this,
            &AGamePlatformArenaGameMode::HandlePreMatchTimeout,
            HeroSelectionTimeoutSeconds,
            false);
    }
}

void AGamePlatformArenaGameMode::BeginReadyCheckIfReady()
{
    if (!AreAllPlayersSelected() || PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::HeroSelection) { return; }
    GetWorldTimerManager().ClearTimer(PreMatchTimeoutTimer);
    FString Error;
    if (Transition(EGamePlatformArenaMatchPhase::ReadyCheck, Error))
    {
        GetWorldTimerManager().SetTimer(
            PreMatchTimeoutTimer,
            this,
            &AGamePlatformArenaGameMode::HandlePreMatchTimeout,
            ReadyCheckTimeoutSeconds,
            false);
    }
}

void AGamePlatformArenaGameMode::BeginCountdownIfReady()
{
    if (!AreAllPlayersReady() || PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::ReadyCheck) { return; }
    GetWorldTimerManager().ClearTimer(PreMatchTimeoutTimer);
    FString Error;
    if (!Transition(EGamePlatformArenaMatchPhase::Countdown, Error, CountdownSeconds)) { return; }
    GetWorldTimerManager().SetTimer(
        CountdownTimer,
        this,
        &AGamePlatformArenaGameMode::StartMatchFromCountdown,
        CountdownSeconds,
        false);
}

void AGamePlatformArenaGameMode::HandlePreMatchTimeout()
{
    const EGamePlatformArenaMatchPhase Phase = PhaseMachine.GetPhase();
    if (Phase != EGamePlatformArenaMatchPhase::HeroSelection &&
        Phase != EGamePlatformArenaMatchPhase::ReadyCheck)
    {
        return;
    }
    FString Error;
    Transition(EGamePlatformArenaMatchPhase::Aborted, Error);
    UE_LOG(LogTemp, Warning, TEXT("Arena pre-match timeout; first-version policy is abort."));
}

void AGamePlatformArenaGameMode::StartMatchFromCountdown()
{
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::Countdown) { return; }
    FString Error;
    if (GameplayLifecycleAdapter == nullptr)
    {
        Transition(EGamePlatformArenaMatchPhase::Failed, Error);
        UE_LOG(LogTemp, Error, TEXT("Arena start rejected: GameplayLifecycleAdapter is not configured."));
        return;
    }
    if (Transition(EGamePlatformArenaMatchPhase::InProgress, Error, ActiveModeSpec.TimeLimitSeconds))
    {
        MatchStartedAtUtc = FDateTime::UtcNow();
        for (const FGamePlatformArenaRosterSlot& Slot : CurrentAssignment.Roster)
        {
            FString SpawnError;
            if (!GameplayLifecycleAdapter->SpawnPlayer(
                Slot.PlayerId,
                Slot.TeamId,
                ActiveModeSpec.SpawnPolicyId,
                SpawnError))
            {
                FString FailureError;
                Transition(EGamePlatformArenaMatchPhase::Failed, FailureError);
                UE_LOG(LogTemp, Error, TEXT("Arena spawn failed for player %s: %s"), *Slot.PlayerId, *SpawnError);
                return;
            }
        }
        GetWorldTimerManager().SetTimer(
            MatchDeadlineTimer,
            this,
            &AGamePlatformArenaGameMode::HandleMatchTimeLimit,
            static_cast<float>(ActiveModeSpec.TimeLimitSeconds),
            false);
    }
}

void AGamePlatformArenaGameMode::HandleMatchTimeLimit()
{
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::InProgress) { return; }
    FName WinningTeam = NAME_None;
    if (const AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>())
    {
        if (ArenaState->TeamStates.Num() == 2 && ArenaState->TeamStates[0].Score != ArenaState->TeamStates[1].Score)
        {
            WinningTeam = ArenaState->TeamStates[0].Score > ArenaState->TeamStates[1].Score
                ? ArenaState->TeamStates[0].TeamId
                : ArenaState->TeamStates[1].TeamId;
        }
    }
    FString Error;
    EndMatch(WinningTeam, EGamePlatformArenaMatchEndReason::TimeLimit, Error);
}

bool AGamePlatformArenaGameMode::HandleTrustedEvent(const FGamePlatformArenaTrustedEvent& Event, FString& OutError)
{
    if (!HasAuthority()) { OutError = TEXT("竞技事件只能由服务器处理。"); return false; }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::InProgress) { OutError = TEXT("比赛未处于InProgress。"); return false; }
    if (Event.EventId.IsEmpty()) { OutError = TEXT("可信事件必须包含EventId用于幂等。"); return false; }
    if (Event.EventId.Len() > MaxTrustedEventIdLength) { OutError = TEXT("可信事件EventId长度超过平台安全上限。"); return false; }
    if (ProcessedEventIds.Contains(Event.EventId)) { OutError.Reset(); return true; }
    if (ProcessedEventIds.Num() >= MaxProcessedTrustedEvents)
    {
        OutError = TEXT("可信事件幂等集合达到安全上限，拒绝继续接收新事件。");
        return false;
    }
    ProcessedEventIds.Add(Event.EventId);

    AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>();
    if (Event.EventType == EGamePlatformArenaTrustedEventType::Death)
    {
        if (TWeakObjectPtr<AGamePlatformArenaPlayerState>* VictimPtr = PlayerStatesById.Find(Event.PlayerId))
        {
            if (AGamePlatformArenaPlayerState* Victim = VictimPtr->Get())
            {
                Victim->AuthorityRecordDeath();
                if (GameplayLifecycleAdapter != nullptr)
                {
                    FString RespawnError;
                    GameplayLifecycleAdapter->RequestRespawn(
                        Victim->PlayerIdPublic,
                        Victim->TeamId,
                        ActiveModeSpec.RespawnPolicyId,
                        StandardRespawnDelaySeconds,
                        RespawnError);
                }
            }
        }
        if (TWeakObjectPtr<AGamePlatformArenaPlayerState>* KillerPtr = PlayerStatesById.Find(Event.RelatedPlayerId))
        {
            if (AGamePlatformArenaPlayerState* Killer = KillerPtr->Get())
            {
                Killer->AuthorityRecordKill();
                Killer->AuthorityAddScore(StandardKillScore);
                if (ArenaState != nullptr) { ArenaState->AuthorityAddTeamScore(Killer->TeamId, StandardKillScore); }
                if (ArenaState != nullptr)
                {
                    const FGamePlatformArenaTeamState* Team = ArenaState->FindTeam(Killer->TeamId);
                    if (Team != nullptr && Team->Score >= StandardWinScore)
                    {
                        return EndMatch(Killer->TeamId, EGamePlatformArenaMatchEndReason::ScoreLimit, OutError);
                    }
                }
            }
        }
    }
    else if (Event.EventType == EGamePlatformArenaTrustedEventType::Assist)
    {
        if (TWeakObjectPtr<AGamePlatformArenaPlayerState>* AssistPtr = PlayerStatesById.Find(Event.PlayerId))
        {
            if (AGamePlatformArenaPlayerState* Assistant = AssistPtr->Get()) { Assistant->AuthorityRecordAssist(); }
        }
    }
    else if (Event.EventType == EGamePlatformArenaTrustedEventType::Objective)
    {
        if (ArenaState != nullptr) { ArenaState->AuthorityAddTeamScore(Event.TeamId, Event.Value, Event.Value); }
    }
    OutError.Reset();
    return true;
}

FName AGamePlatformArenaGameMode::GetOpponentTeam(FName TeamId) const
{
    if (const AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>())
    {
        for (const FGamePlatformArenaTeamState& Team : ArenaState->TeamStates)
        {
            if (Team.TeamId != TeamId) { return Team.TeamId; }
        }
    }
    return NAME_None;
}

bool AGamePlatformArenaGameMode::RequestForfeit(AGamePlatformArenaPlayerState* PlayerState, FString& OutError)
{
    if (!HasAuthority() || PlayerState == nullptr) { OutError = TEXT("无效弃权请求。"); return false; }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::InProgress) { OutError = TEXT("只有进行中的比赛可以弃权。"); return false; }
    if (!PlayerStatesById.Contains(PlayerState->PlayerIdPublic)) { OutError = TEXT("非Roster玩家不能弃权。"); return false; }
    PlayerState->AuthoritySetForfeitState(EGamePlatformArenaForfeitState::Accepted);
    return EndMatch(GetOpponentTeam(PlayerState->TeamId), EGamePlatformArenaMatchEndReason::PlayerForfeit, OutError);
}

void AGamePlatformArenaGameMode::MarkPlayerDisconnected(const FString& PlayerId)
{
    if (!HasAuthority()) { return; }
    TWeakObjectPtr<AGamePlatformArenaPlayerState>* StatePtr = PlayerStatesById.Find(PlayerId);
    if (StatePtr == nullptr || !StatePtr->IsValid()) { return; }
    AGamePlatformArenaPlayerState* State = StatePtr->Get();
    FPlayerRecoveryState& Recovery = RecoveryStatesById.FindOrAdd(PlayerId);
    Recovery.HeroDefinitionId = State->HeroDefinitionId;
    Recovery.bReady = State->bReady;
    Recovery.Kills = State->Kills;
    Recovery.Deaths = State->Deaths;
    Recovery.Assists = State->Assists;
    Recovery.Score = State->ArenaScore;
    Recovery.ObjectiveScore = State->ObjectiveScore;
    Recovery.ForfeitState = State->ForfeitState;
    State->AuthoritySetConnectionState(EGamePlatformArenaConnectionState::Disconnected);
    FTimerHandle& Handle = ReconnectTimers.FindOrAdd(PlayerId);
    GetWorldTimerManager().SetTimer(
        Handle,
        FTimerDelegate::CreateUObject(this, &AGamePlatformArenaGameMode::HandleReconnectTimeout, PlayerId),
        ReconnectGracePeriodSeconds,
        false);
}

bool AGamePlatformArenaGameMode::IsPlayerAwaitingReconnect(const FString& PlayerId) const
{
    return ReconnectTimers.Contains(PlayerId);
}

bool AGamePlatformArenaGameMode::TryReconnectPlayer(
    AGamePlatformArenaPlayerState* NewPlayerState,
    const FString& PlayerId,
    FString& OutError)
{
    if (!HasAuthority() || NewPlayerState == nullptr) { OutError = TEXT("无效重连PlayerState。"); return false; }
    const FGamePlatformArenaRosterSlot* Slot = CurrentAssignment.Roster.FindByPredicate([&PlayerId](const FGamePlatformArenaRosterSlot& Candidate)
    {
        return Candidate.PlayerId == PlayerId;
    });
    if (Slot == nullptr) { OutError = TEXT("重连玩家不在原Roster中。"); return false; }
    FTimerHandle* Handle = ReconnectTimers.Find(PlayerId);
    if (Handle == nullptr || !GetWorldTimerManager().IsTimerActive(*Handle)) { OutError = TEXT("重连宽限期已结束。"); return false; }

    GetWorldTimerManager().ClearTimer(*Handle);
    ReconnectTimers.Remove(PlayerId);
    NewPlayerState->AuthorityApplyRosterIdentity(PlayerId, Slot->CharacterId, Slot->TeamId);
    if (const FPlayerRecoveryState* Recovery = RecoveryStatesById.Find(PlayerId))
    {
        NewPlayerState->AuthorityRestoreCompetitiveState(
            Recovery->HeroDefinitionId,
            Recovery->bReady,
            Recovery->Kills,
            Recovery->Deaths,
            Recovery->Assists,
            Recovery->Score,
            Recovery->ObjectiveScore,
            Recovery->ForfeitState);
    }
    PlayerStatesById.Add(PlayerId, NewPlayerState);
    OutError.Reset();
    return true;
}

void AGamePlatformArenaGameMode::HandleReconnectTimeout(const FString PlayerId)
{
    FName TimedOutTeam = NAME_None;
    if (TWeakObjectPtr<AGamePlatformArenaPlayerState>* StatePtr = PlayerStatesById.Find(PlayerId))
    {
        if (AGamePlatformArenaPlayerState* State = StatePtr->Get())
        {
            TimedOutTeam = State->TeamId;
            State->AuthoritySetConnectionState(EGamePlatformArenaConnectionState::TimedOut);
        }
    }
    if (TimedOutTeam.IsNone())
    {
        if (const FGamePlatformArenaRosterSlot* Slot = CurrentAssignment.Roster.FindByPredicate(
            [&PlayerId](const FGamePlatformArenaRosterSlot& Candidate)
            {
                return Candidate.PlayerId == PlayerId;
            }))
        {
            TimedOutTeam = Slot->TeamId;
        }
    }
    ReconnectTimers.Remove(PlayerId);
    if (!TimedOutTeam.IsNone() && PhaseMachine.GetPhase() == EGamePlatformArenaMatchPhase::InProgress)
    {
        FString Error;
        EndMatch(GetOpponentTeam(TimedOutTeam), EGamePlatformArenaMatchEndReason::ReconnectTimeout, Error);
    }
}

void AGamePlatformArenaGameMode::BuildPendingResult(FName WinningTeamId, EGamePlatformArenaMatchEndReason Reason)
{
    PendingResult = FGamePlatformArenaMatchResult{};
    PendingResult.MatchId = CurrentAssignment.MatchId;
    PendingResult.ArenaModeId = CurrentAssignment.ArenaModeId;
    PendingResult.GameServerId = CurrentAssignment.GameServerId;
    PendingResult.StartedAtUtc = MatchStartedAtUtc;
    PendingResult.EndedAtUtc = FDateTime::UtcNow();
    PendingResult.WinningTeamId = WinningTeamId;
    PendingResult.EndReason = Reason;
    PendingResult.EndRevision = 1;

    const AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>();
    if (ArenaState == nullptr) { return; }
    for (const FGamePlatformArenaTeamState& TeamState : ArenaState->TeamStates)
    {
        FGamePlatformArenaTeamResult TeamResult;
        TeamResult.TeamId = TeamState.TeamId;
        TeamResult.Score = TeamState.Score;
        for (const TPair<FString, TWeakObjectPtr<AGamePlatformArenaPlayerState>>& Pair : PlayerStatesById)
        {
            const AGamePlatformArenaPlayerState* State = Pair.Value.Get();
            if (State == nullptr)
            {
                const FGamePlatformArenaRosterSlot* Slot = CurrentAssignment.Roster.FindByPredicate(
                    [&Pair](const FGamePlatformArenaRosterSlot& Candidate)
                    {
                        return Candidate.PlayerId == Pair.Key;
                    });
                const FPlayerRecoveryState* Recovery = RecoveryStatesById.Find(Pair.Key);
                if (Slot == nullptr || Recovery == nullptr || Slot->TeamId != TeamState.TeamId) { continue; }
                FGamePlatformArenaPlayerResult PlayerResult;
                PlayerResult.PlayerId = Pair.Key;
                PlayerResult.TeamId = Slot->TeamId;
                PlayerResult.CharacterId = Slot->CharacterId;
                PlayerResult.Kills = Recovery->Kills;
                PlayerResult.Deaths = Recovery->Deaths;
                PlayerResult.Assists = Recovery->Assists;
                PlayerResult.Score = Recovery->Score;
                TeamResult.Players.Add(MoveTemp(PlayerResult));
                continue;
            }
            if (State->TeamId != TeamState.TeamId) { continue; }
            FGamePlatformArenaPlayerResult PlayerResult;
            PlayerResult.PlayerId = State->PlayerIdPublic;
            PlayerResult.TeamId = State->TeamId;
            PlayerResult.CharacterId = State->CharacterId;
            PlayerResult.Kills = State->Kills;
            PlayerResult.Deaths = State->Deaths;
            PlayerResult.Assists = State->Assists;
            PlayerResult.Score = State->ArenaScore;
            TeamResult.Players.Add(MoveTemp(PlayerResult));
        }
        PendingResult.Teams.Add(MoveTemp(TeamResult));
    }
}

bool AGamePlatformArenaGameMode::EndMatch(FName WinningTeamId, EGamePlatformArenaMatchEndReason Reason, FString& OutError)
{
    if (!HasAuthority()) { OutError = TEXT("比赛结束只能由服务器决定。"); return false; }
    if (bEndStarted)
    {
        OutError = TEXT("比赛已处于Ending/ResultPending/Completed，重复结束请求被忽略。");
        return false;
    }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::InProgress) { OutError = TEXT("只有InProgress比赛可正常结束。"); return false; }
    bEndStarted = true;
    if (!Transition(EGamePlatformArenaMatchPhase::Ending, OutError)) { bEndStarted = false; return false; }
    GetWorldTimerManager().ClearTimer(MatchDeadlineTimer);
    BuildPendingResult(WinningTeamId, Reason);
    if (AGamePlatformArenaGameState* ArenaState = GetGameState<AGamePlatformArenaGameState>())
    {
        FGamePlatformArenaResultSummary Summary;
        Summary.WinningTeamId = WinningTeamId;
        Summary.EndReason = Reason;
        Summary.EndRevision = PendingResult.EndRevision;
        ArenaState->AuthoritySetResult(Summary);
    }
    if (!Transition(EGamePlatformArenaMatchPhase::ResultPending, OutError))
    {
        return false;
    }
    ResultPendingDelegate.Broadcast(PendingResult);
    return true;
}

bool AGamePlatformArenaGameMode::MarkResultCommitted(FString& OutError)
{
    if (!HasAuthority()) { OutError = TEXT("结果提交确认只能由服务器处理。"); return false; }
    if (PhaseMachine.GetPhase() != EGamePlatformArenaMatchPhase::ResultPending) { OutError = TEXT("当前没有待提交结果。"); return false; }
    return Transition(EGamePlatformArenaMatchPhase::Completed, OutError);
}
