// MOBA客户端只读竞技投影：本地玩家持有，复制事件更新；世界退出清空旧身份和路由，不修改服务器事实。
#include "ViewModels/GamePlatformArenaViewModel.h"

#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"

void UGamePlatformArenaViewModel::RefreshFromReplicatedState(
    const AGamePlatformArenaGameState* GameState,
    const TArray<AGamePlatformArenaPlayerState*>& PlayerStates)
{
    if (GameState == nullptr)
    {
        return;
    }

    MatchId = GameState->MatchIdPublic;
    ArenaModeId = GameState->ArenaModeId;
    MatchPhase = GameState->MatchPhase;
    RemainingPhaseSeconds = GameState->GetPhaseRemainingSeconds();
    Teams = GameState->TeamStates;
    ResultSummary = GameState->ResultSummary;

    // 每次从复制事实重建视图状态前先恢复稳定默认值，避免未知阶段沿用旧页面状态。
    FlowState = EGamePlatformArenaClientFlowState::Idle;

    switch (MatchPhase)
    {
    case EGamePlatformArenaMatchPhase::HeroSelection:
        FlowState = EGamePlatformArenaClientFlowState::HeroSelection;
        break;
    case EGamePlatformArenaMatchPhase::ReadyCheck:
    case EGamePlatformArenaMatchPhase::Countdown:
        FlowState = EGamePlatformArenaClientFlowState::ReadyCheck;
        break;
    case EGamePlatformArenaMatchPhase::InProgress:
    case EGamePlatformArenaMatchPhase::Ending:
    case EGamePlatformArenaMatchPhase::ResultPending:
        FlowState = EGamePlatformArenaClientFlowState::InMatch;
        break;
    case EGamePlatformArenaMatchPhase::Completed:
        FlowState = EGamePlatformArenaClientFlowState::Result;
        break;
    case EGamePlatformArenaMatchPhase::Failed:
    case EGamePlatformArenaMatchPhase::Aborted:
        FlowState = EGamePlatformArenaClientFlowState::Failed;
        break;
    default:
        break;
    }

    Teams.Sort([](
        const FGamePlatformArenaTeamState& A,
        const FGamePlatformArenaTeamState& B)
    {
        return A.TeamId.LexicalLess(B.TeamId);
    });

    Scoreboard.Reset();
    Scoreboard.Reserve(PlayerStates.Num());
    for (const AGamePlatformArenaPlayerState* PlayerState : PlayerStates)
    {
        if (PlayerState == nullptr)
        {
            continue;
        }

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

    Scoreboard.Sort([](
        const FGamePlatformArenaScoreboardRow& A,
        const FGamePlatformArenaScoreboardRow& B)
    {
        if (A.TeamId != B.TeamId)
        {
            return A.TeamId.LexicalLess(B.TeamId);
        }
        return A.PlayerId < B.PlayerId;
    });

    // RefreshFromReplicatedState 只应由复制状态变化或显式刷新触发。
    // 通过平台ViewModel事件统一通知HUD/Screen局部刷新，而不是由Widget逐帧扫描GameState。
    MarkStateChanged();
    OnArenaViewStateChanged.Broadcast();
}

void UGamePlatformArenaViewModel::ResetReplicatedArenaState()
{
    MatchId.Reset(); ArenaModeId = NAME_None; MatchPhase = EGamePlatformArenaMatchPhase::Uninitialized;
    RemainingPhaseSeconds = 0.0; Teams.Reset(); Scoreboard.Reset(); ResultSummary = {};
    FlowState = EGamePlatformArenaClientFlowState::Idle;
    MarkStateChanged(); OnArenaViewStateChanged.Broadcast();
}

void UGamePlatformArenaViewModel::SetObservedClientFlowState(
    EGamePlatformArenaClientFlowState InFlowState)
{
    if (FlowState == InFlowState)
    {
        return;
    }

    FlowState = InFlowState;
    MarkStateChanged();
    OnArenaViewStateChanged.Broadcast();
}
