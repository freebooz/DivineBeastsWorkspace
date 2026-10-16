//go:build grpcdeps

// 真实内部RPC方法先校验服务身份，再允许本人状态查询；空/错/重复元数据失败关闭。
package grpcadapter

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	mv "divinebeasts/backend/internal/generated/match/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"testing"
)

func TestMatchRPCInternalAuthentication(t *testing.T) {
	s := &MatchAPIServer{service: matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), func(s string) string { return s + "-test" }), runtimeBearer: "test-only-match"}
	for _, tokens := range [][]string{nil, {"Bearer wrong"}, {"Bearer test-only-match", "Bearer test-only-match"}} {
		ctx := metadata.NewIncomingContext(context.Background(), metadata.MD{"authorization": tokens})
		if _, err := s.GetMatchmakingMigration(ctx, &mv.MatchmakingCommandRequest{}); status.Code(err) != codes.Unauthenticated {
			t.Fatal("无可信主体不得解析迁移请求", err)
		}
	}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "Bearer test-only-match"))
	value, err := s.ReadMatchmakingState(ctx, &mv.PartyReadRequest{PlayerId: "test-player"})
	if err != nil || value.GetErrorCode() != "" || value.GetPayloadJson() == "" {
		t.Fatal("授权查询应返回真实快照", err)
	}
}
