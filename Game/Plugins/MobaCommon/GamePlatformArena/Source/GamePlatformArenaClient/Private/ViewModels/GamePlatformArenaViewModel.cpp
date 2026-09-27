#include "ViewModels/GamePlatformArenaViewModel.h"

#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"

void UGamePlatformArenaViewModel::RefreshFromReplicatedState(
    const AGamePlatformArenaGameState* GameState,
    const TArray<AGamePlatformArenaPlayerState*>& PlayerStates)
{
    if (GameState == nullptr) { return; }

    MatchId = GameState->MatchIdPublic;
    ArenaModeId = GameState->ArenaModeId;
    MatchPhase = GameState->MatchPhase;
    RemainingPhaseSeconds = GameState->GetPhaseRemainingSeconds();
    Teams = GameState->TeamStates;
    ResultSummary = GameState->ResultSummary;

    switch (MatchPhase)
    {
    case EGamePlatformArenaMatchPhase::HeroSelection: FlowState = EGamePlatformArenaClientFlowState::HeroSelection; break;
    case EGamePlatformArenaMatchPhase::ReadyCheck:
    case EGamePlatformArenaMatchPhase::Countdown: FlowState = EGamePlatformArenaClientFlowState::ReadyCheck; break;
    case EGamePlatformArenaMatchPhase::InProgress:
    case EGamePlatformArenaMatchPhase::Ending:
    case EGamePlatformArenaMatchPhase::ResultPending: FlowState = EGamePlatformArenaClientFlowState::InMatch; break;
    case EGamePlatformArenaMatchPhase::Completed: FlowState = EGamePlatformArenaClientFlowState::Result; break;
    case EGamePlatformArenaMatchPhase::Failed:
    case EGamePlatformArenaMatchPhase::Aborted: FlowState = EGamePlatformArenaClientFlowState::Failed; break;
    default: break;
    }

    Teams.Sort([](const FGamePlatformArenaTeamState& A, const FGamePlatformArenaTeamState& B)
    {
        return A.TeamId.LexicalLess(B.TeamId);
    });

    Scoreboard.Reset();
    for (const AGamePlatformArenaPlayerState* PlayerState : PlayerStates)
    {
        if (PlayerState == nullptr) { continue; }
        FGamePlatformArenaScoreboardRow Row;
        Row.PlayerId = PlayerState->PlayerIdPublic;
        Row.TeamId = PlayerState->TeamId;
        Row.HeroDefinitionId = PlayerState->HeroDefinitionId;
        Row.Kills = PlayerState->Kills;
        Row.Deaths = PlayerState->Deaths;
        Row.Assists = PlayerState->Assists;
        Row.Score = PlayerState->ArenaScore;
        Row.bReady = PlayerState->bReady;
        Scoreboard.Add(MoveTemp(Row));
    }
    Scoreboard.Sort([](const FGamePlatformArenaScoreboardRow& A, const FGamePlatformArenaScoreboardRow& B)
    {
        if (A.TeamId != B.TeamId) { return A.TeamId.LexicalLess(B.TeamId); }
        return A.PlayerId < B.PlayerId;
    });
}
