//go:build grpcdeps

package grpcclient

import (
	"context"
	"encoding/json"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	gameservercontrolv1 "divinebeasts/backend/internal/generated/gameservercontrol/v1"
	"divinebeasts/backend/internal/modules/servertransfer"
	"google.golang.org/grpc"
)

// ArenaControlClient（竞技场控制gRPC客户端）实现MatchService的ArenaControl端口。
type ArenaControlClient struct {
	client        gameservercontrolv1.GameServerControlInternalServiceClient
	runtimeBearer string // 生命周期RPC专用内部凭据，只进入metadata。
	defaultRegion string
}

// NewArenaControlClient（创建竞技场控制gRPC客户端）基于GameServerControlService连接创建Stub。
func NewArenaControlClient(conn grpc.ClientConnInterface) *ArenaControlClient {
	return &ArenaControlClient{client: gameservercontrolv1.NewGameServerControlInternalServiceClient(conn)}
}

// NewWorldEntryClient（创建世界进入gRPC客户端）为Gateway设置服务端默认区域，客户端未声明首选区域时使用。
func NewWorldEntryClient(conn grpc.ClientConnInterface, defaultRegion string, runtimeBearer ...string) *ArenaControlClient {
	token := ""
	if len(runtimeBearer) > 0 {
		token = runtimeBearer[0]
	}
	return &ArenaControlClient{runtimeBearer: token,
		client:        gameservercontrolv1.NewGameServerControlInternalServiceClient(conn),
		defaultRegion: defaultRegion,
	}
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
	return servertransfer.Ticket{
		TicketID: response.GetTicketId(), AssignmentID: response.GetAssignmentId(),
		GameID: req.GameID, PlayerID: response.GetPlayerId(), SessionID: response.GetSessionId(),
		SourceGameServerID:         req.SourceGameServerID,
		DestinationGameServerID:    response.GetDestinationGameServerId(),
		DestinationEndpoint:        response.GetDestinationEndpoint(),
		DestinationWorldID:         response.GetDestinationWorldId(),
		DestinationExperienceID:    response.GetDestinationExperienceId(),
		MatchID:                    response.GetMatchId(),
		GameSessionID:              response.GetGameSessionId(),
		DestinationServerBootID:    response.GetDestinationServerBootId(),
		DestinationProtocolVersion: response.GetDestinationProtocolVersion(),
		SessionEpoch:               response.GetSessionEpoch(),
		IssuedAt:                   time.UnixMilli(response.GetIssuedAtUnixMs()).UTC(),
		ExpiresAt:                  time.UnixMilli(response.GetExpiresAtUnixMs()).UTC(),
		Nonce:                      response.GetNonce(), Signature: response.GetSignature(),
	}, nil
}

// AllocateWorldEntry和HTTP共用控制面原子分配/签票入口，保留完整已签票据。
func (c *ArenaControlClient) AllocateWorldEntry(
	ctx context.Context,
	req gateway.WorldEntryAllocationRequest,
) (gateway.WorldEntryResponse, error) {
	worldID, ok := grpcWorldIDForExperience(req.DesiredExperienceID)
	if !ok || req.RequestID == "" || req.GameID == "" || req.PlayerID == "" ||
		req.SessionID == "" || req.CharacterID == "" {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("INVALID_REQUEST")
	}
	region := req.PreferredRegion
	if region == "" {
		region = c.defaultRegion
	}
	if region == "" {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	result, err := c.AllocateWorldTransfer(ctx, gameservercontrol.AllocateWorldTransferInput{
		World:    gameservercontrol.AllocateWorldInput{ExperienceID: req.DesiredExperienceID, WorldID: worldID, RegionID: region, PlayerSlots: 1},
		Transfer: gameservercontrol.IssueTransferInput{TicketID: req.RequestID, GameID: req.GameID, PlayerID: req.PlayerID, SessionID: req.SessionID, TTL: 30 * time.Second},
	})
	if err != nil {
		return gateway.WorldEntryResponse{}, err
	}
	allocated, ticket := result.Assignment, result.Ticket
	if allocated.AssignmentID == "" || ticket.AssignmentID != allocated.AssignmentID ||
		ticket.PlayerID != req.PlayerID || ticket.SessionID != req.SessionID ||
		ticket.DestinationGameServerID != allocated.GameServerID ||
		ticket.DestinationEndpoint != allocated.Endpoint ||
		ticket.DestinationWorldID != allocated.WorldID ||
		ticket.DestinationExperienceID != allocated.ExperienceID ||
		ticket.GameSessionID == "" || ticket.DestinationServerBootID == "" ||
		ticket.DestinationProtocolVersion == 0 || ticket.SessionEpoch == 0 {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	ticketBytes, err := json.Marshal(ticket)
	if err != nil {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return gateway.WorldEntryResponse{
		AssignmentID: allocated.AssignmentID, GameServerID: allocated.GameServerID,
		ServerRoleID: allocated.ServerRoleID, ExperienceID: allocated.ExperienceID,
		WorldID: allocated.WorldID, MapID: allocated.WorldID, RegionID: allocated.RegionID,
		TicketID: ticket.TicketID, CharacterID: req.CharacterID, SessionID: req.SessionID,
		GameSessionID: ticket.GameSessionID, ServerBootID: ticket.DestinationServerBootID,
		ProtocolVersion: ticket.DestinationProtocolVersion, SessionEpoch: ticket.SessionEpoch,
		Endpoint: allocated.Endpoint, TransferTicket: string(ticketBytes),
	}, nil
}

func grpcWorldIDForExperience(experienceID string) (string, bool) {
	switch experienceID {
	case gameservercontract.ExperienceOpenWorldHub, gameservercontract.ExperienceLobbyMain:
		return "World.OpenWorld.Hub", true
	case gameservercontract.ExperienceOpenWorldMain:
		return "World.OpenWorld.Main", true
	case gameservercontract.ExperienceVillageMain:
		return "divinebeasts.world.village_main@1", true
	case gameservercontract.ExperienceVillageTutorial:
		return "divinebeasts.world.village_tutorial@1", true
	case gameservercontract.ExperienceVillageTraining:
		return "divinebeasts.world.village_training@1", true
	default:
		return "", false
	}
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
