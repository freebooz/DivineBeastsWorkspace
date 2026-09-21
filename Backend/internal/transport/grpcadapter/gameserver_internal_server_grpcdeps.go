//go:build grpcdeps

package grpcadapter

import (
	"context"
	"time"

	gameservercontrolv1 "divinebeasts/backend/generated/proto/internal/gameservercontrol/v1"
	"divinebeasts/backend/internal/app/gameservercontrol"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"google.golang.org/grpc"
)

// GameServerControlInternalServer（游戏服务器控制内部gRPC服务端）供MatchService分配MainArena并签发迁移票据。
type GameServerControlInternalServer struct {
	gameservercontrolv1.UnimplementedGameServerControlInternalServiceServer
	service *gameservercontrol.Service
}

// RegisterGameServerControlInternalServer（注册内部控制gRPC服务）挂载Backend内部RPC接口。
func RegisterGameServerControlInternalServer(registrar grpc.ServiceRegistrar, service *gameservercontrol.Service) {
	gameservercontrolv1.RegisterGameServerControlInternalServiceServer(registrar, &GameServerControlInternalServer{service: service})
}

// AllocateWorld（分配常驻世界）支持OpenWorld.Hub/OpenWorld.Main/Village体验。
func (s *GameServerControlInternalServer) AllocateWorld(ctx context.Context, req *gameservercontrolv1.AllocateWorldRequest) (*gameservercontrolv1.AllocateWorldResponse, error) {
	assignment, err := s.service.AllocateWorld(ctx, gameservercontrol.AllocateWorldInput{ExperienceID: req.GetExperienceId(), WorldID: req.GetWorldId(), RegionID: req.GetRegionId(), PlayerSlots: int(req.GetPlayerSlots())})
	if err != nil {
		return &gameservercontrolv1.AllocateWorldResponse{Allocated: false, ErrorCode: "WORLD_ALLOCATION_FAILED"}, nil
	}
	return &gameservercontrolv1.AllocateWorldResponse{Allocated: true, AssignmentId: assignment.AssignmentID, GameServerId: assignment.GameServerID, ServerRoleId: assignment.ServerRoleID, ExperienceId: assignment.ExperienceID, WorldId: assignment.WorldID, RegionId: assignment.RegionID, DestinationEndpoint: assignment.Endpoint}, nil
}

// AllocateMainArena（分配主竞技场）把内部RPC Roster映射到权威Assignment。
func (s *GameServerControlInternalServer) AllocateMainArena(ctx context.Context, req *gameservercontrolv1.AllocateMainArenaRequest) (*gameservercontrolv1.AllocateMainArenaResponse, error) {
	roster := make([]gameservercontract.RosterPlayer, 0, len(req.GetRoster()))
	for _, player := range req.GetRoster() {
		roster = append(roster, gameservercontract.RosterPlayer{PlayerID: player.GetPlayerId(), TeamID: player.GetTeamId(), CharacterID: player.GetCharacterId()})
	}
	assignment, err := s.service.AllocateMainArenaContext(ctx, gameservercontrol.AllocateMainArenaInput{MatchID: req.GetMatchId(), ArenaModeID: req.GetArenaModeId(), MapID: req.GetMapId(), RegionID: req.GetRegionId(), Roster: roster})
	if err != nil {
		return &gameservercontrolv1.AllocateMainArenaResponse{Allocated: false, ErrorCode: "GAME_SERVER_NO_CAPACITY"}, nil
	}
	responseRoster := make([]*gameservercontrolv1.RosterPlayer, 0, len(assignment.Roster))
	for _, player := range assignment.Roster {
		responseRoster = append(responseRoster, &gameservercontrolv1.RosterPlayer{PlayerId: player.PlayerID, TeamId: player.TeamID, CharacterId: player.CharacterID})
	}
	return &gameservercontrolv1.AllocateMainArenaResponse{Allocated: true, GameServerId: assignment.GameServerID, MatchId: assignment.MatchID, ArenaModeId: assignment.ArenaModeID, MapId: assignment.MapID, TeamSize: uint32(assignment.TeamSize), TotalPlayers: uint32(assignment.TotalPlayers), Roster: responseRoster}, nil
}

// IssuePlayerTransfer（签发玩家迁移票据）返回完整票据，供MatchService下发给Game Client。
func (s *GameServerControlInternalServer) IssuePlayerTransfer(ctx context.Context, req *gameservercontrolv1.IssuePlayerTransferRequest) (*gameservercontrolv1.IssuePlayerTransferResponse, error) {
	_ = ctx
	ticket, err := s.service.IssueTransfer(gameservercontrol.IssueTransferInput{
		TicketID: req.GetTicketId(), AssignmentID: req.GetAssignmentId(), GameID: req.GetGameId(), PlayerID: req.GetPlayerId(), SessionID: req.GetSessionId(),
		SourceGameServerID: req.GetSourceGameServerId(), DestinationGameServerID: req.GetDestinationGameServerId(),
		DestinationWorldID: req.GetDestinationWorldId(), DestinationExperienceID: req.GetDestinationExperienceId(), MatchID: req.GetMatchId(), TTL: time.Duration(req.GetTtlMilliseconds()) * time.Millisecond,
	})
	if err != nil {
		return &gameservercontrolv1.IssuePlayerTransferResponse{Issued: false, ErrorCode: "TRANSFER_TICKET_ISSUE_FAILED"}, nil
	}
	return &gameservercontrolv1.IssuePlayerTransferResponse{
		Issued: true, TicketId: ticket.TicketID, PlayerId: ticket.PlayerID, SessionId: ticket.SessionID,
		DestinationGameServerId: ticket.DestinationGameServerID, DestinationEndpoint: ticket.DestinationEndpoint,
		DestinationWorldId: ticket.DestinationWorldID, MatchId: ticket.MatchID, IssuedAtUnixMs: ticket.IssuedAt.UnixMilli(),
		ExpiresAtUnixMs: ticket.ExpiresAt.UnixMilli(), Nonce: ticket.Nonce, Signature: ticket.Signature, AssignmentId: ticket.AssignmentID, DestinationExperienceId: ticket.DestinationExperienceID,
	}, nil
}
