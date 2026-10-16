//go:build grpcdeps

// Match内部身份校验仅信任单个Bearer元数据，不把ActorJSON或自报服务名当凭据。
package grpcadapter

import (
	"context"
	"crypto/subtle"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

func (s *MatchAPIServer) authorizeInternalMatch(ctx context.Context) error {
	md, _ := metadata.FromIncomingContext(ctx)
	tokens := md.Get("authorization")
	if s.runtimeBearer == "" || len(tokens) != 1 || subtle.ConstantTimeCompare([]byte(tokens[0]), []byte("Bearer "+s.runtimeBearer)) != 1 {
		return status.Error(codes.Unauthenticated, "内部匹配认证失败")
	}
	return nil
}
