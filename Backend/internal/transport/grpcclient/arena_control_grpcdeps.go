//go:build grpcdeps

package grpcclient

import (
	"context"
	"encoding/json"
	"time"

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
	defaultRegion string
}

// NewArenaControlClient（创建竞技场控制gRPC客户端）基于GameServerControlService连接创建Stub。
func NewArenaControlClient(conn grpc.ClientConnInterface) *ArenaControlClient {
	return &ArenaControlClient{client: gameservercontrolv1.NewGameServerControlInternalServiceClient(conn)}
}

// NewWorldEntryClient（创建世界进入gRPC客户端）为Gateway设置服务端默认区域，客户端未声明首选区域时使用。
func NewWorldEntryClient(conn grpc.ClientConnInterface, defaultRegion string) *ArenaControlClient {
	return &ArenaControlClient{
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
	return servertransfer.Ticket{TicketID: response.GetTicketId(), AssignmentID: response.GetAssignmentId(), GameID: req.GameID, PlayerID: response.GetPlayerId(), SessionID: response.GetSessionId(), SourceGameServerID: req.SourceGameServerID, DestinationGameServerID: response.GetDestinationGameServerId(), DestinationEndpoint: response.GetDestinationEndpoint(), DestinationWorldID: response.GetDestinationWorldId(), DestinationExperienceID: response.GetDestinationExperienceId(), MatchID: response.GetMatchId(), IssuedAt: time.UnixMilli(response.GetIssuedAtUnixMs()).UTC(), ExpiresAt: time.UnixMilli(response.GetExpiresAtUnixMs()).UTC(), Nonce: response.GetNonce(), Signature: response.GetSignature()}, nil
}

// AllocateWorldEntry（Gateway世界进入gRPC适配）先分配常驻世界，再为同一Assignment签发玩家迁移票据。
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
	allocated, err := c.client.AllocateWorld(ctx, &gameservercontrolv1.AllocateWorldRequest{
		ExperienceId: req.DesiredExperienceID,
		WorldId:      worldID,
		RegionId:     region,
		PlayerSlots:  1,
	})
	if err != nil {
		return gateway.WorldEntryResponse{}, err
	}
	if !allocated.GetAllocated() {
		return gateway.WorldEntryResponse{}, grpcError(allocated.GetErrorCode())
	}
	issued, err := c.client.IssuePlayerTransfer(ctx, &gameservercontrolv1.IssuePlayerTransferRequest{
		TicketId:                req.RequestID,
		AssignmentId:            allocated.GetAssignmentId(),
		GameId:                  req.GameID,
		PlayerId:                req.PlayerID,
		SessionId:               req.SessionID,
		DestinationGameServerId: allocated.GetGameServerId(),
		DestinationWorldId:      allocated.GetWorldId(),
		DestinationExperienceId: allocated.GetExperienceId(),
		TtlMilliseconds:         (30 * time.Second).Milliseconds(),
	})
	if err != nil {
		return gateway.WorldEntryResponse{}, err
	}
	if !issued.GetIssued() {
		return gateway.WorldEntryResponse{}, grpcError(issued.GetErrorCode())
	}
	ticket := servertransfer.Ticket{
		TicketID: issued.GetTicketId(), AssignmentID: issued.GetAssignmentId(),
		GameID: req.GameID, PlayerID: issued.GetPlayerId(), SessionID: issued.GetSessionId(),
		DestinationGameServerID: issued.GetDestinationGameServerId(),
		DestinationEndpoint:     issued.GetDestinationEndpoint(),
		DestinationWorldID:      issued.GetDestinationWorldId(),
		DestinationExperienceID: issued.GetDestinationExperienceId(),
		MatchID:                 issued.GetMatchId(),
		IssuedAt:                time.UnixMilli(issued.GetIssuedAtUnixMs()).UTC(),
		ExpiresAt:               time.UnixMilli(issued.GetExpiresAtUnixMs()).UTC(),
		Nonce:                   issued.GetNonce(), Signature: issued.GetSignature(),
	}
	if ticket.AssignmentID != allocated.GetAssignmentId() ||
		ticket.PlayerID != req.PlayerID || ticket.SessionID != req.SessionID ||
		ticket.DestinationGameServerID != allocated.GetGameServerId() ||
		ticket.DestinationEndpoint != allocated.GetDestinationEndpoint() ||
		ticket.DestinationWorldID != allocated.GetWorldId() ||
		ticket.DestinationExperienceID != allocated.GetExperienceId() {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	ticketBytes, err := json.Marshal(ticket)
	if err != nil {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return gateway.WorldEntryResponse{
		AssignmentID: allocated.GetAssignmentId(), GameServerID: allocated.GetGameServerId(),
		ServerRoleID: allocated.GetServerRoleId(), ExperienceID: allocated.GetExperienceId(),
		WorldID: allocated.GetWorldId(), MapID: allocated.GetWorldId(), RegionID: allocated.GetRegionId(),
		TicketID: ticket.TicketID, CharacterID: req.CharacterID, SessionID: req.SessionID,
		Endpoint: allocated.GetDestinationEndpoint(), TransferTicket: string(ticketBytes),
	}, nil
}

func grpcWorldIDForExperience(experienceID string) (string, bool) {
	switch experienceID {
	case gameservercontract.ExperienceOpenWorldHub, gameservercontract.ExperienceLobbyMain:
		return "World.OpenWorld.Hub", true
	case gameservercontract.ExperienceOpenWorldMain:
		return "World.OpenWorld.Main", true
	case gameservercontract.ExperienceVillageMain:
		return "World.Village.Main", true
	case gameservercontract.ExperienceVillageTutorial:
		return "World.Village.Tutorial", true
	case gameservercontract.ExperienceVillageTraining:
		return "World.Village.Training", true
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
