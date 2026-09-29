package servertransfer

import (
	"context"
	"errors"
	"testing"
	"time"
)

func TestTicketIsOneTimeAndDestinationBound(t *testing.T) {
	now := time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)
	clock := func() time.Time { return now }
	service := NewService([]byte("01234567890123456789012345678901"), clock)
	ticket, err := service.Issue(IssueRequest{
		TicketID: "transfer-1", GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1",
		DestinationGameServerID: "arena-1", DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.MainArena", MatchID: "match-1", TTL: 30 * time.Second,
		DestinationServerBootID: "boot-arena-1", DestinationProtocolVersion: 2,
	})
	if err != nil {
		t.Fatalf("签发失败: %v", err)
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-2", DestinationServerBootID: "boot-arena-1", DestinationProtocolVersion: 2}); err == nil {
		t.Fatal("错误目标服务器必须被拒绝")
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-1", DestinationServerBootID: "boot-arena-1", DestinationProtocolVersion: 2}); err != nil {
		t.Fatalf("第一次正确验证应该成功: %v", err)
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-1", DestinationServerBootID: "boot-arena-1", DestinationProtocolVersion: 2}); err == nil {
		t.Fatal("迁移票据只能使用一次")
	}
}

func TestExpiredTicketIsRejected(t *testing.T) {
	now := time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)
	current := now
	service := NewService([]byte("01234567890123456789012345678901"), func() time.Time { return current })
	ticket, _ := service.Issue(IssueRequest{TicketID: "t1", GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1", DestinationGameServerID: "village-1", DestinationEndpoint: "127.0.0.1:7778", DestinationWorldID: "World.Village", TTL: time.Second, DestinationServerBootID: "boot-village-1", DestinationProtocolVersion: 1})
	current = now.Add(2 * time.Second)
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "village-1", DestinationServerBootID: "boot-village-1", DestinationProtocolVersion: 1}); err == nil {
		t.Fatal("过期票据必须被拒绝")
	}
}

// TestAuthoritativeBindingEpochAndFences（权威绑定代次与防旧测试）验证Epoch单调递增，且Boot/协议错配不会提前消费票据。
func TestAuthoritativeBindingEpochAndFences(t *testing.T) {
	now := time.Date(2026, 9, 29, 8, 0, 0, 0, time.UTC)
	replay := &recordingReplayStore{}
	service := NewServiceWithStores(
		[]byte("01234567890123456789012345678901"),
		func() time.Time { return now },
		replay,
		NewMemorySessionEpochStore())

	issue := func(id string) Ticket {
		ticket, err := service.Issue(IssueRequest{
			TicketID: id, AssignmentID: "world:ow-1:main", GameID: "divine-beasts",
			PlayerID: "p1", SessionID: "session-epoch", DestinationGameServerID: "ow-1",
			DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.OpenWorld.Main",
			DestinationExperienceID: "Experience.OpenWorld.Main", TTL: 30 * time.Second,
			DestinationServerBootID: "boot-current", DestinationProtocolVersion: 7,
		})
		if err != nil {
			t.Fatal(err)
		}
		return ticket
	}

	first := issue("epoch-1")
	second := issue("epoch-2")
	if first.SessionEpoch != 1 || second.SessionEpoch != 2 ||
		first.GameSessionID == second.GameSessionID {
		t.Fatalf("SessionEpoch或GameSessionID不符合单调绑定语义: first=%+v second=%+v", first, second)
	}

	if _, err := service.Validate(ValidateRequest{
		Ticket: first, DestinationGameServerID: "ow-1",
		DestinationServerBootID: "boot-old", DestinationProtocolVersion: 7,
	}); err == nil {
		t.Fatal("旧Boot必须拒绝")
	}
	if replay.calls != 0 {
		t.Fatal("Boot错配必须在ReplayStore消费前拒绝")
	}
	if _, err := service.Validate(ValidateRequest{
		Ticket: first, DestinationGameServerID: "ow-1",
		DestinationServerBootID: "boot-current", DestinationProtocolVersion: 8,
	}); err == nil {
		t.Fatal("协议错配必须拒绝")
	}
	if replay.calls != 0 {
		t.Fatal("协议错配必须在ReplayStore消费前拒绝")
	}
	// 更高Epoch先成功准入后，旧Epoch必须在ReplayStore消费之前被服务端拒绝。
	validated, err := service.Validate(ValidateRequest{
		Ticket: second, DestinationGameServerID: "ow-1",
		DestinationServerBootID: "boot-current", DestinationProtocolVersion: 7,
	})
	if err != nil {
		t.Fatal(err)
	}
	if validated.GameSessionID != second.GameSessionID ||
		validated.SessionEpoch != second.SessionEpoch ||
		validated.DestinationServerBootID != "boot-current" ||
		validated.DestinationProtocolVersion != 7 {
		t.Fatalf("验证结果未保留完整Binding: %+v", validated)
	}
	if replay.calls != 1 {
		t.Fatalf("更高Epoch首次准入应只消费一次ReplayStore，实际=%d", replay.calls)
	}
	if _, err := service.Validate(ValidateRequest{
		Ticket: first, DestinationGameServerID: "ow-1",
		DestinationServerBootID: "boot-current", DestinationProtocolVersion: 7,
	}); err == nil {
		t.Fatal("更高Epoch已经准入后，旧Epoch票据必须被服务端拒绝")
	}
	if replay.calls != 1 {
		t.Fatalf("旧Epoch必须在ReplayStore消费前拒绝，实际调用=%d", replay.calls)
	}
}

// TestEpochFenceAllowsRetryAfterReplayStoreFailure（Epoch栅栏与Replay失败重试测试）
// 验证Epoch确认先执行但同Ticket同Epoch可幂等重试，避免ReplayStore瞬时故障永久烧掉本次准入。
func TestEpochFenceAllowsRetryAfterReplayStoreFailure(t *testing.T) {
	now := time.Date(2026, 9, 29, 9, 0, 0, 0, time.UTC)
	replay := &flakyReplayStore{}
	service := NewServiceWithStores(
		[]byte("01234567890123456789012345678901"),
		func() time.Time { return now },
		replay,
		NewMemorySessionEpochStore(),
	)
	ticket, err := service.Issue(IssueRequest{
		TicketID:                   "retry-after-replay-error",
		AssignmentID:               "world:ow-1:main",
		GameID:                     "divine-beasts",
		PlayerID:                   "p1",
		SessionID:                  "session-retry",
		DestinationGameServerID:    "ow-1",
		DestinationEndpoint:        "127.0.0.1:7777",
		DestinationWorldID:         "World.OpenWorld.Main",
		DestinationExperienceID:    "Experience.OpenWorld.Main",
		TTL:                        30 * time.Second,
		DestinationServerBootID:    "boot-current",
		DestinationProtocolVersion: 7,
	})
	if err != nil {
		t.Fatal(err)
	}

	request := ValidateRequest{
		Ticket:                     ticket,
		DestinationGameServerID:    "ow-1",
		DestinationServerBootID:    "boot-current",
		DestinationProtocolVersion: 7,
	}
	if _, err := service.Validate(request); err == nil {
		t.Fatal("第一次ReplayStore瞬时失败必须向上传递错误")
	}
	validated, err := service.Validate(request)
	if err != nil {
		t.Fatalf("同Ticket同Epoch重试应成功: %v", err)
	}
	if validated.SessionEpoch != ticket.SessionEpoch ||
		validated.GameSessionID != ticket.GameSessionID {
		t.Fatalf("重试返回Binding错误: %+v", validated)
	}
	if replay.calls != 2 {
		t.Fatalf("ReplayStore应调用两次，实际=%d", replay.calls)
	}
}

type flakyReplayStore struct {
	calls int
}

func (s *flakyReplayStore) Consume(_ context.Context, _ string, _ time.Duration) (bool, error) {
	s.calls++
	if s.calls == 1 {
		return false, errors.New("temporary replay store failure")
	}
	return true, nil
}

type recordingReplayStore struct {
	consumed map[string]bool
	calls    int
}

func (s *recordingReplayStore) Consume(_ context.Context, ticketID string, ttl time.Duration) (bool, error) {
	s.calls++
	if ttl <= 0 {
		return false, errors.New("ttl invalid")
	}
	if s.consumed == nil {
		s.consumed = map[string]bool{}
	}
	if s.consumed[ticketID] {
		return false, nil
	}
	s.consumed[ticketID] = true
	return true, nil
}

// TestReplayStateDelegatesToReplayStore（防重放仓储委托测试）确保Service不再依赖自身进程内消费状态。
func TestReplayStateDelegatesToReplayStore(t *testing.T) {
	now := time.Date(2026, 9, 21, 5, 0, 0, 0, time.UTC)
	store := &recordingReplayStore{}
	service := NewServiceWithReplayStore([]byte("01234567890123456789012345678901"), func() time.Time { return now }, store)
	ticket, err := service.Issue(IssueRequest{TicketID: "redis-ticket", AssignmentID: "world:ow-1:hub", GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1", DestinationGameServerID: "ow-1", DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.OpenWorld.Hub", DestinationExperienceID: "Experience.OpenWorld.Hub", TTL: 30 * time.Second, DestinationServerBootID: "boot-ow-1", DestinationProtocolVersion: 1})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateContext(context.Background(), ValidateRequest{Ticket: ticket, DestinationGameServerID: "ow-1", DestinationServerBootID: "boot-ow-1", DestinationProtocolVersion: 1}); err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateContext(context.Background(), ValidateRequest{Ticket: ticket, DestinationGameServerID: "ow-1", DestinationServerBootID: "boot-ow-1", DestinationProtocolVersion: 1}); err == nil {
		t.Fatal("ReplayStore应拒绝重复消费")
	}
	if store.calls != 2 {
		t.Fatalf("ReplayStore调用次数错误: %d", store.calls)
	}
}
