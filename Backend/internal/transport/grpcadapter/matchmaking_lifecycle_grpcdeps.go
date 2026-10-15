//go:build grpcdeps

// 本文件让正式内部gRPC与HTTP使用同一匹配应用、严格解码和安全错误；不维护第二套准备/取消状态。
package grpcadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gateway"
	matchv1 "divinebeasts/backend/internal/generated/match/v1"
	"encoding/json"
	"time"
)

// matchmakingRPCResponse只编码完整授权成功值；失败不夹带JSON或底层异常。
func matchmakingRPCResponse(value any, err error) *matchv1.PartyDataResponse {
	if err != nil {
		return &matchv1.PartyDataResponse{ErrorCode: gateway.MatchmakingErrorCode(err)}
	}
	payload, err := json.Marshal(value)
	if err != nil {
		return &matchv1.PartyDataResponse{ErrorCode: "SERVICE_UNAVAILABLE"}
	}
	return &matchv1.PartyDataResponse{PayloadJson: string(payload)}
}

// ReadMatchmakingState只接受可信Gateway生成的本人主体。
func (s *MatchAPIServer) ReadMatchmakingState(ctx context.Context, request *matchv1.PartyReadRequest) (*matchv1.PartyDataResponse, error) {
	if err := s.authorizeInternalMatch(ctx); err != nil {
		return nil, err
	}
	value, err := s.service.ReadMatchmakingState(ctx, request.GetPlayerId())
	return matchmakingRPCResponse(value, err), nil
}

// ObserveMatchmaking严格限制毫秒范围后转换，避免Duration溢出或无限阻塞。
func (s *MatchAPIServer) ObserveMatchmaking(ctx context.Context, request *matchv1.PartyObservationRequest) (*matchv1.PartyDataResponse, error) {
	if err := s.authorizeInternalMatch(ctx); err != nil {
		return nil, err
	}
	if request.GetWaitMilliseconds() < 0 || request.GetWaitMilliseconds() > 20000 {
		return &matchv1.PartyDataResponse{ErrorCode: "INVALID_REQUEST"}, nil
	}
	value, err := s.service.ObserveMatchmaking(ctx, request.GetPlayerId(), request.GetCursor(), time.Duration(request.GetWaitMilliseconds())*time.Millisecond)
	return matchmakingRPCResponse(value, err), nil
}

// CommandMatchmaking对actor与公开命令分别严格解析，不允许嵌套正文逃过重复键检查。
func (s *MatchAPIServer) CommandMatchmaking(ctx context.Context, request *matchv1.MatchmakingCommandRequest) (*matchv1.PartyDataResponse, error) {
	if err := s.authorizeInternalMatch(ctx); err != nil {
		return nil, err
	}
	actor, err := gateway.DecodeMatchmakingActorJSON(request.GetActorJson())
	if err != nil {
		return matchmakingRPCResponse(nil, err), nil
	}
	command, err := gateway.DecodeMatchmakingCommandJSON(request.GetCommandJson())
	if err != nil {
		return matchmakingRPCResponse(nil, err), nil
	}
	value, err := s.service.CommandMatchmaking(ctx, actor, command)
	return matchmakingRPCResponse(value, err), nil
}

// GetMatchmakingMigration仅返回当前会话本人的秘密准入，不能广播到普通队伍快照。
func (s *MatchAPIServer) GetMatchmakingMigration(ctx context.Context, request *matchv1.MatchmakingCommandRequest) (*matchv1.PartyDataResponse, error) {
	if err := s.authorizeInternalMatch(ctx); err != nil {
		return nil, err
	}
	actor, err := gateway.DecodeMatchmakingActorJSON(request.GetActorJson())
	if err != nil {
		return matchmakingRPCResponse(nil, err), nil
	}
	command, err := gateway.DecodeMatchmakingMigrationJSON(request.GetCommandJson())
	if err != nil {
		return matchmakingRPCResponse(nil, err), nil
	}
	value, err := s.service.GetMatchmakingMigration(ctx, actor, command)
	return matchmakingRPCResponse(value, err), nil
}
