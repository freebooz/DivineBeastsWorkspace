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
	})
	if err != nil {
		t.Fatalf("签发失败: %v", err)
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-2"}); err == nil {
		t.Fatal("错误目标服务器必须被拒绝")
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-1"}); err != nil {
		t.Fatalf("第一次正确验证应该成功: %v", err)
	}
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "arena-1"}); err == nil {
		t.Fatal("迁移票据只能使用一次")
	}
}

func TestExpiredTicketIsRejected(t *testing.T) {
	now := time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)
	current := now
	service := NewService([]byte("01234567890123456789012345678901"), func() time.Time { return current })
	ticket, _ := service.Issue(IssueRequest{TicketID: "t1", GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1", DestinationGameServerID: "village-1", DestinationEndpoint: "127.0.0.1:7778", DestinationWorldID: "World.Village", TTL: time.Second})
	current = now.Add(2 * time.Second)
	if _, err := service.Validate(ValidateRequest{Ticket: ticket, DestinationGameServerID: "village-1"}); err == nil {
		t.Fatal("过期票据必须被拒绝")
	}
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
	ticket, err := service.Issue(IssueRequest{TicketID: "redis-ticket", AssignmentID: "world:ow-1:hub", GameID: "divine-beasts", PlayerID: "p1", SessionID: "s1", DestinationGameServerID: "ow-1", DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.OpenWorld.Hub", DestinationExperienceID: "Experience.OpenWorld.Hub", TTL: 30 * time.Second})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateContext(context.Background(), ValidateRequest{Ticket: ticket, DestinationGameServerID: "ow-1"}); err != nil {
		t.Fatal(err)
	}
	if _, err := service.ValidateContext(context.Background(), ValidateRequest{Ticket: ticket, DestinationGameServerID: "ow-1"}); err == nil {
		t.Fatal("ReplayStore应拒绝重复消费")
	}
	if store.calls != 2 {
		t.Fatalf("ReplayStore调用次数错误: %d", store.calls)
	}
}
