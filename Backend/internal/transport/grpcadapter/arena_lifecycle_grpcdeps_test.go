//go:build grpcdeps

// 本回归验证真实gRPC元数据认证和严格正文边界；使用内存领域仓储，不将其当成生产数据库证据。
package grpcadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	controlv1 "divinebeasts/backend/internal/generated/gameservercontrol/v1"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"testing"
	"time"
)

// TestArenaLifecycleAuthentication拒绝无/重复/错误凭据、未知字段及重复JSON键；合法取消必须真实持久收敛。
func TestArenaLifecycleAuthentication(t *testing.T) {
	service := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now), match.NewResultService(match.NewMemoryResultStore()), time.Now)
	if err := service.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
		t.Fatal(err)
	}
	server := &GameServerControlInternalServer{service: service, runtimeBearer: "test-only-internal"}
	request := &controlv1.ArenaLifecycleRequest{Action: "cancel", InputJson: `{"MatchID":"test-cancel"}`}
	for _, tokens := range [][]string{nil, {"Bearer wrong"}, {"Bearer test-only-internal", "Bearer test-only-internal"}} {
		values := metadata.MD{}
		values.Set("authorization", tokens...)
		if _, err := server.ArenaLifecycle(metadata.NewIncomingContext(context.Background(), values), request); status.Code(err) != codes.Unauthenticated {
			t.Fatal("必须拒绝未授权元数据", err)
		}
	}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("authorization", "Bearer test-only-internal"))
	for _, body := range []string{`{"MatchID":"a","MatchID":"b"}`, `{"MatchID":"a","endpoint":"forged"}`, `{"MatchID":"a"} {}`} {
		response, err := server.ArenaLifecycle(ctx, &controlv1.ArenaLifecycleRequest{Action: "cancel", InputJson: body})
		if err != nil || response.Success || response.ErrorCode != "INVALID_REQUEST" {
			t.Fatal("不合法正文不能进入领域操作", err)
		}
	}
	response, err := server.ArenaLifecycle(ctx, request)
	if err != nil || !response.Success {
		t.Fatal("合法可信取消应真实成功", err)
	}
	value, err := service.GetArenaStatus(ctx, "test-cancel")
	if err != nil || !value.Record.UnallocatedCancellation || value.Record.State != "cancelled" {
		t.Fatal("必须存在取消事实", err)
	}
}
