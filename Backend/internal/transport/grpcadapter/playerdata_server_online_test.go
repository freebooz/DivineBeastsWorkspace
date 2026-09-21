//go:build grpcdeps

package grpcadapter

import (
	"context"
	playerdatav1 "divinebeasts/backend/internal/generated/playerdata/v1"
	"divinebeasts/backend/internal/modules/playerdata"
	"errors"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/test/bufconn"
	"net"
	"testing"
	"time"
)

// 内存仓储仅注入传输故障；测试真实gRPC序列化、处理器和领域用例，不宣称PG验收。
type playerTransportRepository struct {
	*playerdata.MemoryRepository
	probeErr error
	lastKey  string
}

func (r *playerTransportRepository) Probe(context.Context) error { return r.probeErr }
func (r *playerTransportRepository) EnsureProfile(_ context.Context, id, game string) error {
	r.Seed(playerdata.Profile{PlayerID: id, GameID: game, DisplayName: "初始", DataVersion: 1})
	return nil
}
func (r *playerTransportRepository) UpdateDisplayNameIdempotent(ctx context.Context, id, name string, rev int64, key string) (playerdata.Profile, error) {
	r.lastKey = key
	return playerdata.NewService(r.MemoryRepository).UpdateDisplayName(ctx, id, name, rev)
}
func TestOnlineGRPCProfileMethodsAndProbe(t *testing.T) {
	repo := &playerTransportRepository{MemoryRepository: playerdata.NewMemoryRepository()}
	listener := bufconn.Listen(1 << 20)
	server := grpc.NewServer()
	RegisterPlayerDataServer(server, playerdata.NewService(repo))
	go server.Serve(listener)
	defer server.Stop()
	conn, err := grpc.NewClient("passthrough:///bufnet", grpc.WithTransportCredentials(insecure.NewCredentials()), grpc.WithContextDialer(func(context.Context, string) (net.Conn, error) { return listener.Dial() }))
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	client := playerdatav1.NewPlayerDataServiceClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	probe, err := client.Probe(ctx, &playerdatav1.ProbeRequest{})
	if err != nil || !probe.GetReady() {
		t.Fatalf("probe=%v err=%v", probe, err)
	}
	ensured, err := client.EnsureProfile(ctx, &playerdatav1.EnsureProfileRequest{PlayerId: "player-a", GameId: "game"})
	if err != nil || ensured.GetErrorCode() != "" {
		t.Fatalf("ensure=%v err=%v", ensured, err)
	}
	rev := int64(0)
	profile, err := client.UpdateProfile(ctx, &playerdatav1.UpdateProfileRequest{PlayerId: "player-a", DisplayName: "新名字", ExpectedRevision: &rev, IdempotencyKey: "key-a"})
	if err != nil || profile.GetRevision() != 1 || repo.lastKey != "key-a" {
		t.Fatalf("update=%v err=%v", profile, err)
	}
	missing, err := client.UpdateProfile(ctx, &playerdatav1.UpdateProfileRequest{PlayerId: "player-a", DisplayName: "x", IdempotencyKey: "key-b"})
	if err != nil || missing.GetErrorCode() != "INVALID_REQUEST" {
		t.Fatalf("missing revision=%v err=%v", missing, err)
	}
	repo.probeErr = errors.New("private PG address")
	failed, err := client.Probe(ctx, &playerdatav1.ProbeRequest{})
	if err != nil || failed.GetReady() || failed.GetErrorCode() != "SERVICE_UNAVAILABLE" {
		t.Fatalf("failed probe=%v err=%v", failed, err)
	}
}
