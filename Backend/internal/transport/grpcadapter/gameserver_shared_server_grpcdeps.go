//go:build grpcdeps

package grpcadapter

import (
	"context"
	"fmt"
	"net"
	"strconv"

	gameserverv1 "divinebeasts/backend/generated/proto/shared/gameplatform/gameserver/v1"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/modules/gameserver"
	"google.golang.org/grpc"
)

// GameServerSharedServer（GameServer跨技术gRPC服务端）供UE Dedicated Server执行注册、心跳、Ready、Drain和Assignment查询。
type GameServerSharedServer struct {
	gameserverv1.UnimplementedGameServerControlServiceServer
	service *gameservercontrol.Service
}

// RegisterGameServerSharedServer（注册GameServer跨技术RPC）把Shared Contract服务挂载到gRPC Server。
func RegisterGameServerSharedServer(registrar grpc.ServiceRegistrar, service *gameservercontrol.Service) {
	gameserverv1.RegisterGameServerControlServiceServer(registrar, &GameServerSharedServer{service: service})
}

// RegisterGameServer（注册游戏服务器）把UE Dedicated Server注册请求写入Backend快速注册表。
func (s *GameServerSharedServer) RegisterGameServer(ctx context.Context, req *gameserverv1.RegisterGameServerRequest) (*gameserverv1.RegisterGameServerResponse, error) {
	_ = ctx
	endpoint := ""
	if req.GetPublicEndpoint() != nil {
		endpoint = net.JoinHostPort(req.GetPublicEndpoint().GetHost(), strconv.Itoa(int(req.GetPublicEndpoint().GetPort())))
	}
	err := s.service.Register(gameservercontrol.RegisterInput{
		GameID: req.GetGameId(), GameServerID: req.GetGameServerId(), ServerRoleID: req.GetServerRoleId(), ExperienceID: req.GetExperienceId(), RegionID: req.GetRegionId(),
		ClusterID: req.GetClusterId(), NodeID: req.GetNodeId(), WorldID: req.GetWorldId(), PublicEndpoint: endpoint,
		BuildVersion: req.GetBuildVersion(), ProtocolVersion: req.GetProtocolVersion(), Capacity: int(req.GetCapacity()),
	})
	if err != nil {
		return &gameserverv1.RegisterGameServerResponse{Accepted: false, ErrorCode: "GAME_SERVER_REGISTER_FAILED"}, nil
	}
	return &gameserverv1.RegisterGameServerResponse{Accepted: true, RegistrationId: "registration:" + req.GetGameServerId(), HeartbeatIntervalSeconds: 5}, nil
}

// UpdateHeartbeat（更新服务器心跳）刷新GameServer玩家数量和生命周期状态。
func (s *GameServerSharedServer) UpdateHeartbeat(ctx context.Context, req *gameserverv1.GameServerHeartbeatRequest) (*gameserverv1.GameServerHeartbeatResponse, error) {
	_ = ctx
	if err := s.service.Heartbeat(req.GetGameServerId(), int(req.GetCurrentPlayers()), gameserver.Status(req.GetStatus())); err != nil {
		return &gameserverv1.GameServerHeartbeatResponse{Accepted: false, ErrorCode: "GAME_SERVER_NOT_FOUND"}, nil
	}
	return &gameserverv1.GameServerHeartbeatResponse{Accepted: true}, nil
}

// SetGameServerReady（标记服务器就绪）允许GameServer进入分配池。
func (s *GameServerSharedServer) SetGameServerReady(ctx context.Context, req *gameserverv1.SetGameServerReadyRequest) (*gameserverv1.SetGameServerReadyResponse, error) {
	_ = ctx
	if err := s.service.SetReady(req.GetGameServerId()); err != nil {
		return &gameserverv1.SetGameServerReadyResponse{Accepted: false, ErrorCode: "GAME_SERVER_NOT_FOUND"}, nil
	}
	return &gameserverv1.SetGameServerReadyResponse{Accepted: true}, nil
}

// SetGameServerDraining（标记服务器排空）阻止GameServer继续接受新分配。
func (s *GameServerSharedServer) SetGameServerDraining(ctx context.Context, req *gameserverv1.SetGameServerDrainingRequest) (*gameserverv1.SetGameServerDrainingResponse, error) {
	_ = ctx
	if err := s.service.Drain(req.GetGameServerId()); err != nil {
		return &gameserverv1.SetGameServerDrainingResponse{Accepted: false, ErrorCode: "GAME_SERVER_NOT_FOUND"}, nil
	}
	return &gameserverv1.SetGameServerDrainingResponse{Accepted: true}, nil
}

// GetGameServerAssignment（获取统一服务器任务）供OpenWorld、Village和MainArena查询权威运行上下文。
func (s *GameServerSharedServer) GetGameServerAssignment(ctx context.Context, req *gameserverv1.GetGameServerAssignmentRequest) (*gameserverv1.GetGameServerAssignmentResponse, error) {
	_ = ctx
	assignment, found := s.service.GetServerAssignment(req.GetGameServerId())
	if !found {
		return &gameserverv1.GetGameServerAssignmentResponse{Found: false, ErrorCode: "GAME_SERVER_ASSIGNMENT_NOT_FOUND"}, nil
	}
	response := &gameserverv1.GetGameServerAssignmentResponse{Found: true, AssignmentId: assignment.AssignmentID, ServerRoleId: assignment.ServerRoleID, ExperienceId: assignment.ExperienceID, WorldId: assignment.WorldID, RegionId: assignment.RegionID}
	if assignment.World != nil {
		response.AssignmentType = "world"
		return response, nil
	}
	response.AssignmentType = "arena"
	response.MatchId = assignment.Arena.MatchID
	response.ArenaModeId = assignment.Arena.ArenaModeID
	response.MapId = assignment.Arena.MapID
	response.TeamSize = uint32(assignment.Arena.TeamSize)
	response.TotalPlayers = uint32(assignment.Arena.TotalPlayers)
	for _, player := range assignment.Arena.Roster {
		response.Roster = append(response.Roster, &gameserverv1.AssignedPlayer{PlayerId: player.PlayerID, TeamId: player.TeamID, CharacterId: player.CharacterID})
	}
	if assignment.Arena.TeamSize <= 0 || assignment.Arena.TotalPlayers <= 0 {
		return nil, fmt.Errorf("Backend内部Assignment人数无效")
	}
	return response, nil
}
