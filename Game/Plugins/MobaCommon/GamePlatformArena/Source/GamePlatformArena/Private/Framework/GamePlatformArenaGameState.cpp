#include "Framework/GamePlatformArenaGameState.h"

#include "Net/UnrealNetwork.h"

AGamePlatformArenaGameState::AGamePlatformArenaGameState()
{
    bReplicates = true;
}

void AGamePlatformArenaGameState::AuthorityInitializeMatch(const FGamePlatformArenaAssignment& Assignment)
{
    if (!HasAuthority()) { return; }
    MatchIdPublic = Assignment.MatchId;
    ArenaModeId = Assignment.ArenaModeId;
    TeamStates.Reset();

    TSet<FName> SeenTeams;
    for (const FGamePlatformArenaRosterSlot& Slot : Assignment.Roster)
    {
        if (!Slot.TeamId.IsNone() && !SeenTeams.Contains(Slot.TeamId))
        {
            FGamePlatformArenaTeamState Team;
            Team.TeamId = Slot.TeamId;
            TeamStates.Add(Team);
            SeenTeams.Add(Slot.TeamId);
        }
    }
}

void AGamePlatformArenaGameState::AuthoritySetPhase(
    EGamePlatformArenaMatchPhase NewPhase,
    int32 NewRevision,
    double DurationSeconds)
{
    if (!HasAuthority()) { return; }
    MatchPhase = NewPhase;
    PhaseRevision = NewRevision;
    PhaseStartServerTime = GetServerWorldTimeSeconds();
    PhaseDeadlineServerTime = DurationSeconds > 0.0 ? PhaseStartServerTime + DurationSeconds : 0.0;
    if (NewPhase == EGamePlatformArenaMatchPhase::InProgress) { MatchStartServerTime = PhaseStartServerTime; }
    if (NewPhase == EGamePlatformArenaMatchPhase::Completed || NewPhase == EGamePlatformArenaMatchPhase::Aborted || NewPhase == EGamePlatformArenaMatchPhase::Failed)
    {
        MatchEndServerTime = PhaseStartServerTime;
    }
    ForceNetUpdate();
    OnArenaPhaseChanged.Broadcast(MatchPhase, PhaseRevision);
}

bool AGamePlatformArenaGameState::AuthorityAddTeamScore(FName TeamId, int32 ScoreDelta, int32 ObjectiveDelta)
{
    if (!HasAuthority()) { return false; }
    for (FGamePlatformArenaTeamState& Team : TeamStates)
    {
        if (Team.TeamId == TeamId)
        {
            Team.Score += ScoreDelta;
            Team.ObjectiveScore += ObjectiveDelta;
            ++Team.Revision;
            ForceNetUpdate();
            OnArenaTeamStatesChanged.Broadcast(TeamStates);
            return true;
        }
    }
    return false;
}

void AGamePlatformArenaGameState::AuthoritySetResult(const FGamePlatformArenaResultSummary& Summary)
{
    if (!HasAuthority()) { return; }
    ResultSummary = Summary;
    ForceNetUpdate();
    OnArenaResultChanged.Broadcast(ResultSummary);
}

double AGamePlatformArenaGameState::GetPhaseRemainingSeconds() const
{
    if (PhaseDeadlineServerTime <= 0.0) { return 0.0; }
    return FMath::Max(0.0, PhaseDeadlineServerTime - GetServerWorldTimeSeconds());
}

const FGamePlatformArenaTeamState* AGamePlatformArenaGameState::FindTeam(FName TeamId) const
{
    return TeamStates.FindByPredicate([TeamId](const FGamePlatformArenaTeamState& Team)
    {
        return Team.TeamId == TeamId;
    });
}

void AGamePlatformArenaGameState::OnRep_MatchPhase()
{
    OnArenaPhaseChanged.Broadcast(MatchPhase, PhaseRevision);
}

void AGamePlatformArenaGameState::OnRep_TeamStates()
{
    OnArenaTeamStatesChanged.Broadcast(TeamStates);
}

void AGamePlatformArenaGameState::OnRep_ObjectiveStates()
{
    OnArenaObjectiveStatesChanged.Broadcast(ObjectiveStates);
}

void AGamePlatformArenaGameState::OnRep_ResultSummary()
{
    OnArenaResultChanged.Broadcast(ResultSummary);
}

void AGamePlatformArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGamePlatformArenaGameState, MatchIdPublic);
    DOREPLIFETIME(AGamePlatformArenaGameState, ArenaModeId);
    DOREPLIFETIME(AGamePlatformArenaGameState, MatchPhase);
    DOREPLIFETIME(AGamePlatformArenaGameState, PhaseRevision);
    DOREPLIFETIME(AGamePlatformArenaGameState, PhaseStartServerTime);
    DOREPLIFETIME(AGamePlatformArenaGameState, PhaseDeadlineServerTime);
    DOREPLIFETIME(AGamePlatformArenaGameState, MatchStartServerTime);
    DOREPLIFETIME(AGamePlatformArenaGameState, MatchEndServerTime);
    DOREPLIFETIME(AGamePlatformArenaGameState, TeamStates);
    DOREPLIFETIME(AGamePlatformArenaGameState, ObjectiveStates);
    DOREPLIFETIME(AGamePlatformArenaGameState, ResultSummary);
}
