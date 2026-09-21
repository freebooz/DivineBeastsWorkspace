//go:build grpcdeps

package grpcadapter

import (
	"context"
	"time"

	matchv1 "divinebeasts/backend/generated/proto/shared/gameplatform/match/v1"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/modules/match"
	"google.golang.org/grpc"
)

// MatchResultServer（比赛结果gRPC服务端）只接受可信MainArena Dedicated Server提交的权威结果。
type MatchResultServer struct {
	matchv1.UnimplementedMatchResultServiceServer
	service *gameservercontrol.Service
}

// RegisterMatchResultServer（注册比赛结果RPC）把Shared MatchResult契约挂载到gRPC Server。
func RegisterMatchResultServer(registrar grpc.ServiceRegistrar, service *gameservercontrol.Service) {
	matchv1.RegisterMatchResultServiceServer(registrar, &MatchResultServer{service: service})
}

// SubmitMatchResult（提交比赛结果）完整映射Team与Player统计，并由应用服务执行幂等结算与GameServer释放。
func (s *MatchResultServer) SubmitMatchResult(ctx context.Context, req *matchv1.SubmitMatchResultRequest) (*matchv1.SubmitMatchResultResponse, error) {
	teams := make([]match.TeamResult, 0, len(req.GetTeams()))
	for _, team := range req.GetTeams() {
		teams = append(teams, match.TeamResult{TeamID: team.GetTeamId(), Won: team.GetWon(), Score: team.GetScore()})
	}
	players := make([]match.PlayerResult, 0, len(req.GetPlayers()))
	for _, player := range req.GetPlayers() {
		players = append(players, match.PlayerResult{PlayerID: player.GetPlayerId(), TeamID: player.GetTeamId(), CharacterID: player.GetCharacterId(), Kills: player.GetKills(), Deaths: player.GetDeaths(), Assists: player.GetAssists(), Score: player.GetScore()})
	}
	stored, err := s.service.SubmitMatchResult(ctx, match.Result{
		MatchID: req.GetMatchId(), ArenaModeID: req.GetArenaModeId(), GameServerID: req.GetGameServerId(),
		StartedAt: time.UnixMilli(req.GetStartedAtUnixMs()).UTC(), EndedAt: time.UnixMilli(req.GetEndedAtUnixMs()).UTC(),
		WinningTeamID: req.GetWinningTeamId(), Teams: teams, Players: players,
	})
	if err != nil {
		return &matchv1.SubmitMatchResultResponse{Accepted: false, ErrorCode: "MATCH_RESULT_REJECTED"}, nil
	}
	return &matchv1.SubmitMatchResultResponse{Accepted: true, ResultId: stored.ResultID}, nil
}
