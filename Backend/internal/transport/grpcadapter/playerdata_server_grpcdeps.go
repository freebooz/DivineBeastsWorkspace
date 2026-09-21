//go:build grpcdeps

package grpcadapter

import (
	"context"

	playerdatav1 "divinebeasts/backend/generated/proto/internal/playerdata/v1"
	"divinebeasts/backend/internal/modules/playerdata"
	"google.golang.org/grpc"
)

// PlayerDataServer（玩家数据gRPC服务端）把GetProfile RPC映射到PlayerData领域服务。
type PlayerDataServer struct {
	playerdatav1.UnimplementedPlayerDataServiceServer
	service *playerdata.Service
}

// RegisterPlayerDataServer（注册玩家数据gRPC服务）把服务挂载到共享gRPC Server。
func RegisterPlayerDataServer(registrar grpc.ServiceRegistrar, service *playerdata.Service) {
	playerdatav1.RegisterPlayerDataServiceServer(registrar, &PlayerDataServer{service: service})
}

// GetProfile（获取玩家资料）返回跨局长期数据，不包含实时Gameplay状态。
func (s *PlayerDataServer) GetProfile(ctx context.Context, req *playerdatav1.GetProfileRequest) (*playerdatav1.GetProfileResponse, error) {
	profile, err := s.service.GetProfile(ctx, req.GetPlayerId())
	if err != nil {
		return &playerdatav1.GetProfileResponse{Found: false, ErrorCode: "PLAYER_PROFILE_NOT_FOUND"}, nil
	}
	return &playerdatav1.GetProfileResponse{
		Found: true, PlayerId: profile.PlayerID, GameId: profile.GameID, DisplayName: profile.DisplayName,
		DataVersion: int32(profile.DataVersion), Revision: profile.Revision, TutorialCompleted: profile.TutorialCompleted,
		DefaultWorldId: profile.DefaultWorldID, OwnedCharacterIds: append([]string(nil), profile.OwnedCharacterIDs...),
	}, nil
}
