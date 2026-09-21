//go:build grpcdeps

package grpcadapter

import (
	"context"

	playerdatav1 "divinebeasts/backend/internal/generated/playerdata/v1"
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
		return &playerdatav1.GetProfileResponse{Found: false, ErrorCode: onlineDomainCode(err)}, nil
	}
	return profileResponse(profile),nil
}

func profileResponse(profile playerdata.Profile)*playerdatav1.GetProfileResponse{return &playerdatav1.GetProfileResponse{
		Found: true, PlayerId: profile.PlayerID, GameId: profile.GameID, DisplayName: profile.DisplayName,
		DataVersion: int32(profile.DataVersion), Revision: profile.Revision, TutorialCompleted: profile.TutorialCompleted,
		DefaultWorldId: profile.DefaultWorldID, OwnedCharacterIds: append([]string(nil), profile.OwnedCharacterIDs...),
	}
}

// UpdateProfile 要求proto optional presence，不能将漏填修订号当作0。
func(s *PlayerDataServer)UpdateProfile(ctx context.Context,req *playerdatav1.UpdateProfileRequest)(*playerdatav1.GetProfileResponse,error){
 if req.ExpectedRevision==nil{return &playerdatav1.GetProfileResponse{ErrorCode:"INVALID_REQUEST"},nil}
 p,err:=s.service.UpdateDisplayNameIdempotent(ctx,req.GetPlayerId(),req.GetDisplayName(),req.GetExpectedRevision(),req.GetIdempotencyKey());if err!=nil{return &playerdatav1.GetProfileResponse{ErrorCode:onlineDomainCode(err)},nil};return profileResponse(p),nil
}
func(s *PlayerDataServer)EnsureProfile(ctx context.Context,req *playerdatav1.EnsureProfileRequest)(*playerdatav1.EnsureProfileResponse,error){return &playerdatav1.EnsureProfileResponse{ErrorCode:onlineDomainCode(s.service.EnsureProfile(ctx,req.GetPlayerId(),req.GetGameId()))},nil}
func(s *PlayerDataServer)Probe(ctx context.Context,_ *playerdatav1.ProbeRequest)(*playerdatav1.ProbeResponse,error){err:=s.service.Probe(ctx);return &playerdatav1.ProbeResponse{Ready:err==nil,ErrorCode:onlineDomainCode(err)},nil}
