//go:build grpcdeps

package grpcclient

import (
	"context"
	"time"

	gameservercontrolv1 "divinebeasts/backend/internal/generated/gameservercontrol/v1"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/servertransfer"
	"google.golang.org/grpc"
)

// ArenaControlClient（竞技场控制gRPC客户端）实现MatchService的ArenaControl端口。
type ArenaControlClient struct {
	client gameservercontrolv1.GameServerControlInternalServiceClient
}

// NewArenaControlClient（创建竞技场控制gRPC客户端）基于GameServerControlService连接创建Stub。
func NewArenaControlClient(conn grpc.ClientConnInterface) *ArenaControlClient {
	return &ArenaControlClient{client: gameservercontrolv1.NewGameServerControlInternalServiceClient(conn)}
}

// AllocateMainArena（分配主竞技场）通过内部gRPC请求一个独占MainArena实例。
func (c *ArenaControlClient) AllocateMainArena(req matchservice.ArenaAllocationRequest) (gameservercontract.Assignment, error) {
	roster := make([]*gameservercontrolv1.RosterPlayer, 0, len(req.Roster))
	for _, player := range req.Roster {
		roster = append(roster, &gameservercontrolv1.RosterPlayer{PlayerId: player.PlayerID, TeamId: player.TeamID, CharacterId: player.CharacterID})
	}
	response, err := c.client.AllocateMainArena(context.Background(), &gameservercontrolv1.AllocateMainArenaRequest{MatchId: req.MatchID, ArenaModeId: req.ArenaModeID, MapId: req.MapID, RegionId: req.RegionID, TeamSize: uint32(req.TeamSize), TotalPlayers: uint32(req.TotalPlayers), Roster: roster})
	if err != nil {
		return gameservercontract.Assignment{}, err
	}
	if !response.GetAllocated() {
		return gameservercontract.Assignment{}, grpcError(response.GetErrorCode())
	}
	resultRoster := make([]gameservercontract.RosterPlayer, 0, len(response.GetRoster()))
	for _, player := range response.GetRoster() {
		resultRoster = append(resultRoster, gameservercontract.RosterPlayer{PlayerID: player.GetPlayerId(), TeamID: player.GetTeamId(), CharacterID: player.GetCharacterId()})
	}
	return gameservercontract.Assignment{MatchID: response.GetMatchId(), ArenaModeID: response.GetArenaModeId(), MapID: response.GetMapId(), TeamSize: int(response.GetTeamSize()), TotalPlayers: int(response.GetTotalPlayers()), GameServerID: response.GetGameServerId(), Roster: resultRoster}, nil
}

// IssuePlayerTransfer（签发玩家迁移票据）通过内部gRPC获取完整TransferTicket。
func (c *ArenaControlClient) IssuePlayerTransfer(req matchservice.PlayerTransferRequest) (servertransfer.Ticket, error) {
	response, err := c.client.IssuePlayerTransfer(context.Background(), &gameservercontrolv1.IssuePlayerTransferRequest{TicketId: req.TicketID, AssignmentId: req.AssignmentID, GameId: req.GameID, PlayerId: req.PlayerID, SessionId: req.SessionID, SourceGameServerId: req.SourceGameServerID, DestinationGameServerId: req.DestinationGameServerID, DestinationWorldId: req.DestinationWorldID, DestinationExperienceId: req.DestinationExperienceID, MatchId: req.MatchID, TtlMilliseconds: req.TTL.Milliseconds()})
	if err != nil {
		return servertransfer.Ticket{}, err
	}
	if !response.GetIssued() {
		return servertransfer.Ticket{}, grpcError(response.GetErrorCode())
	}
	return servertransfer.Ticket{TicketID: response.GetTicketId(), AssignmentID: response.GetAssignmentId(), GameID: req.GameID, PlayerID: response.GetPlayerId(), SessionID: response.GetSessionId(), SourceGameServerID: req.SourceGameServerID, DestinationGameServerID: response.GetDestinationGameServerId(), DestinationEndpoint: response.GetDestinationEndpoint(), DestinationWorldID: response.GetDestinationWorldId(), DestinationExperienceID: response.GetDestinationExperienceId(), MatchID: response.GetMatchId(), IssuedAt: time.UnixMilli(response.GetIssuedAtUnixMs()).UTC(), ExpiresAt: time.UnixMilli(response.GetExpiresAtUnixMs()).UTC(), Nonce: response.GetNonce(), Signature: response.GetSignature()}, nil
}

func grpcError(code string) error {
	if code == "" {
		code = "GRPC_APPLICATION_ERROR"
	}
	return &applicationError{code: code}
}

type applicationError struct{ code string }

// Error（错误文本）返回Backend内部gRPC应用错误码。
func (e *applicationError) Error() string { return e.code }
