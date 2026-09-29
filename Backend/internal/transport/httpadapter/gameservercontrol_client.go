package httpadapter

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"
	"time"

	"divinebeasts/backend/internal/app/gateway"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// GameServerControlClientConfig（游戏服务器控制内部客户端配置）用于Gateway调用受保护的控制面。
// BearerToken仅进入Authorization请求头，不写日志、不进入URL或业务DTO。
type GameServerControlClientConfig struct {
	ClientConfig
	BearerToken   string
	DefaultRegion string
}

// GameServerControlClient（游戏服务器控制内部HTTP客户端）实现Gateway WorldEntryPort。
// 它只负责服务间协议适配，不自行选择角色、验证玩家所有权或生成TransferTicket。
type GameServerControlClient struct {
	internalClient
	bearerToken   string
	defaultRegion string
}

// NewGameServerControlClient（创建游戏服务器控制客户端）要求非空内部Bearer和默认区域。
func NewGameServerControlClient(cfg GameServerControlClientConfig) *GameServerControlClient {
	token := strings.TrimSpace(cfg.BearerToken)
	region := strings.TrimSpace(cfg.DefaultRegion)
	if token == "" || region == "" {
		panic("GameServerControl内部客户端BearerToken和DefaultRegion不能为空")
	}
	return &GameServerControlClient{
		internalClient: newInternalClient(cfg.ClientConfig),
		bearerToken:    token,
		defaultRegion:  region,
	}
}

// AllocateWorldEntry（分配世界并签发迁移票据）调用受保护的GameServerControl内部接口。
// MapID当前使用WorldID作为逻辑世界定义身份；真实磁盘地图仍由客户端WorldDefinition解析，绝不把它当包路径。
func (c *GameServerControlClient) AllocateWorldEntry(
	ctx context.Context,
	req gateway.WorldEntryAllocationRequest,
) (gateway.WorldEntryResponse, error) {
	worldID, ok := worldIDForExperience(req.DesiredExperienceID)
	if !ok || strings.TrimSpace(req.RequestID) == "" || strings.TrimSpace(req.GameID) == "" ||
		strings.TrimSpace(req.PlayerID) == "" || strings.TrimSpace(req.SessionID) == "" ||
		strings.TrimSpace(req.CharacterID) == "" {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("INVALID_REQUEST")
	}
	region := strings.TrimSpace(req.PreferredRegion)
	if region == "" {
		region = c.defaultRegion
	}

	request := struct {
		World struct {
			ExperienceID string `json:"ExperienceID"`
			WorldID      string `json:"WorldID"`
			RegionID     string `json:"RegionID"`
			PlayerSlots  int    `json:"PlayerSlots"`
		} `json:"world"`
		TicketID           string `json:"ticketId"`
		GameID             string `json:"gameId"`
		PlayerID           string `json:"playerId"`
		SessionID          string `json:"sessionId"`
		SourceGameServerID string `json:"sourceGameServerId,omitempty"`
		TTLMilliseconds    int64  `json:"ttlMilliseconds"`
	}{
		TicketID:        req.RequestID,
		GameID:          req.GameID,
		PlayerID:        req.PlayerID,
		SessionID:       req.SessionID,
		TTLMilliseconds: (30 * time.Second).Milliseconds(),
	}
	request.World.ExperienceID = req.DesiredExperienceID
	request.World.WorldID = worldID
	request.World.RegionID = region
	request.World.PlayerSlots = 1

	var response struct {
		Assignment struct {
			AssignmentID string
			GameServerID string
			ServerRoleID string
			ExperienceID string
			WorldID      string
			RegionID     string
			Endpoint     string
		}
		Ticket servertransfer.Ticket
	}
	if err := c.postAuthorized(
		ctx,
		"/internal/v1/gameservers/allocate-world-transfer",
		request,
		&response,
	); err != nil {
		return gateway.WorldEntryResponse{}, err
	}
	if response.Assignment.AssignmentID == "" || response.Assignment.GameServerID == "" ||
		response.Assignment.ServerRoleID == "" || response.Assignment.ExperienceID != req.DesiredExperienceID ||
		response.Assignment.WorldID != worldID || response.Assignment.RegionID == "" ||
		response.Assignment.Endpoint == "" || response.Ticket.TicketID == "" ||
		response.Ticket.AssignmentID != response.Assignment.AssignmentID ||
		response.Ticket.PlayerID != req.PlayerID || response.Ticket.SessionID != req.SessionID ||
		response.Ticket.DestinationGameServerID != response.Assignment.GameServerID ||
		response.Ticket.DestinationEndpoint != response.Assignment.Endpoint ||
		response.Ticket.DestinationWorldID != response.Assignment.WorldID ||
		response.Ticket.DestinationExperienceID != response.Assignment.ExperienceID ||
		response.Ticket.GameSessionID == "" ||
		response.Ticket.DestinationServerBootID == "" ||
		response.Ticket.DestinationProtocolVersion == 0 ||
		response.Ticket.SessionEpoch == 0 {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	ticketBytes, err := json.Marshal(response.Ticket)
	if err != nil {
		return gateway.WorldEntryResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return gateway.WorldEntryResponse{
		AssignmentID:    response.Assignment.AssignmentID,
		GameServerID:    response.Assignment.GameServerID,
		ServerRoleID:    response.Assignment.ServerRoleID,
		ExperienceID:    response.Assignment.ExperienceID,
		WorldID:         response.Assignment.WorldID,
		MapID:           response.Assignment.WorldID,
		RegionID:        response.Assignment.RegionID,
		TicketID:        response.Ticket.TicketID,
		CharacterID:     req.CharacterID,
		SessionID:       req.SessionID,
		GameSessionID:   response.Ticket.GameSessionID,
		ServerBootID:    response.Ticket.DestinationServerBootID,
		ProtocolVersion: response.Ticket.DestinationProtocolVersion,
		SessionEpoch:    response.Ticket.SessionEpoch,
		Endpoint:        response.Assignment.Endpoint,
		TransferTicket:  string(ticketBytes),
	}, nil
}

func (c *GameServerControlClient) postAuthorized(
	ctx context.Context,
	path string,
	request any,
	response any,
) error {
	body, err := json.Marshal(request)
	if err != nil {
		return err
	}
	httpRequest, err := http.NewRequestWithContext(
		ctx,
		http.MethodPost,
		c.baseURL+path,
		bytes.NewReader(body),
	)
	if err != nil {
		return err
	}
	httpRequest.Header.Set("Content-Type", "application/json")
	httpRequest.Header.Set("Accept", "application/json")
	httpRequest.Header.Set("Authorization", "Bearer "+c.bearerToken)
	if err := c.do(httpRequest, response); err != nil {
		var serviceError gateway.ServiceError
		if errors.As(err, &serviceError) {
			return err
		}
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return nil
}

func worldIDForExperience(experienceID string) (string, bool) {
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
