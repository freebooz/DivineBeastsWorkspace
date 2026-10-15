//go:build grpcdeps

// 本文件只为经过内部Bearer认证的Match/Gateway提供生命周期RPC；错误和事件不能泄漏原始秘密正文。
package grpcadapter

import (
	"context"
	"crypto/subtle"
	gameservercontrolv1 "divinebeasts/backend/internal/generated/gameservercontrol/v1"
	"encoding/json"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"strings"
)

// ArenaLifecycle使用部署内部凭据，不把服务名或客户端自报身份当作授权；未配置凭据失败关闭。
func (s *GameServerControlInternalServer) ArenaLifecycle(ctx context.Context, input *gameservercontrolv1.ArenaLifecycleRequest) (*gameservercontrolv1.ArenaLifecycleResponse, error) {
	values, _ := metadata.FromIncomingContext(ctx)
	tokens := values.Get("authorization")
	if s.runtimeBearer == "" || len(tokens) != 1 || subtle.ConstantTimeCompare([]byte(tokens[0]), []byte("Bearer "+s.runtimeBearer)) != 1 {
		return nil, status.Error(codes.Unauthenticated, "内部控制认证失败")
	}
	value, err := s.service.ExecuteRuntimeCommand(ctx, input.GetAction(), input.GetInputJson())
	if err != nil {
		code := strings.SplitN(err.Error(), ":", 2)[0]
		if !strings.HasPrefix(code, "MATCH_") && !strings.HasPrefix(code, "ARENA_") && !strings.HasPrefix(code, "PLAYER_") && !strings.HasPrefix(code, "TRANSFER_") && code != "INVALID_REQUEST" {
			code = "SERVICE_UNAVAILABLE"
		}
		return &gameservercontrolv1.ArenaLifecycleResponse{ErrorCode: code}, nil
	}
	payload, err := json.Marshal(value)
	if err != nil {
		return nil, status.Error(codes.Internal, "内部结果编码失败")
	}
	return &gameservercontrolv1.ArenaLifecycleResponse{Success: true, ResultJson: string(payload)}, nil
}
