package outbox

import (
	"context"
	"errors"
	"testing"
	"time"
)

type recordingPublisher struct {
	published []Message
	fail      bool
}

func (p *recordingPublisher) Publish(_ context.Context, message Message) error {
	if p.fail {
		return errors.New("模拟消息系统不可用")
	}
	p.published = append(p.published, message)
	return nil
}

// TestOutboxEnqueueIsIdempotent（Outbox入队幂等测试）验证同一MessageID重复写入不会制造重复事件。
func TestOutboxEnqueueIsIdempotent(t *testing.T) {
	store := NewMemoryStore()
	message := Message{ID: "msg-1", Topic: "Match.Completed", AggregateID: "match-1", Payload: []byte(`{"matchId":"match-1"}`), OccurredAt: time.Now().UTC()}
	if err := store.Enqueue(context.Background(), message); err != nil {
		t.Fatal(err)
	}
	if err := store.Enqueue(context.Background(), message); err != nil {
		t.Fatal(err)
	}
	if got := store.PendingCount(); got != 1 {
		t.Fatalf("重复MessageID应只有一条Outbox记录，实际=%d", got)
	}

	conflicting := message
	conflicting.Payload = []byte(`{"matchId":"other"}`)
	if err := store.Enqueue(context.Background(), conflicting); err == nil {
		t.Fatal("同一MessageID不同Payload必须拒绝")
	}
}

// TestDispatcherMarksPublishedOnlyAfterSuccess（Outbox发布状态测试）验证消息成功发布后才标记Published，失败时保留Pending以便重试。
func TestDispatcherMarksPublishedOnlyAfterSuccess(t *testing.T) {
	store := NewMemoryStore()
	now := time.Date(2026, 9, 17, 2, 0, 0, 0, time.UTC)
	store.Enqueue(context.Background(), Message{ID: "msg-1", Topic: "Match.Completed", AggregateID: "match-1", Payload: []byte(`{}`), OccurredAt: now})

	failing := &recordingPublisher{fail: true}
	dispatcher := NewDispatcher(store, failing, func() time.Time { return now })
	if _, err := dispatcher.Dispatch(context.Background(), 10); err == nil {
		t.Fatal("发布失败应返回错误")
	}
	if got := store.PendingCount(); got != 1 {
		t.Fatalf("发布失败后消息必须保持Pending，实际=%d", got)
	}

	success := &recordingPublisher{}
	dispatcher = NewDispatcher(store, success, func() time.Time { return now.Add(time.Second) })
	count, err := dispatcher.Dispatch(context.Background(), 10)
	if err != nil {
		t.Fatal(err)
	}
	if count != 1 || len(success.published) != 1 {
		t.Fatalf("成功发布数量错误: count=%d published=%d", count, len(success.published))
	}
	if got := store.PendingCount(); got != 0 {
		t.Fatalf("成功发布后Pending应为0，实际=%d", got)
	}
}

// TestDispatcherLeasePreventsDoublePublish（多副本租约测试）验证同一消息不会被两个Dispatcher同时领取。
func TestDispatcherLeasePreventsDoublePublish(t *testing.T) {
	store := NewMemoryStore()
	now := time.Now().UTC()
	if err := store.Enqueue(context.Background(), Message{ID: "lease-1", Topic: "Match.Completed", AggregateID: "match-lease", Payload: []byte(`{"matchId":"match-lease"}`), OccurredAt: now}); err != nil {
		t.Fatal(err)
	}
	claimed, err := store.ClaimPending(context.Background(), 1, "worker-a", now.Add(time.Minute))
	if err != nil || len(claimed) != 1 {
		t.Fatalf("worker-a领取失败: len=%d err=%v", len(claimed), err)
	}
	claimedByB, err := store.ClaimPending(context.Background(), 1, "worker-b", now.Add(time.Minute))
	if err != nil {
		t.Fatal(err)
	}
	if len(claimedByB) != 0 {
		t.Fatalf("租约有效期内worker-b不得重复领取: %+v", claimedByB)
	}
	if err := store.MarkFailed(context.Background(), "lease-1", "worker-a"); err != nil {
		t.Fatal(err)
	}
	claimedByB, err = store.ClaimPending(context.Background(), 1, "worker-b", now.Add(time.Minute))
	if err != nil || len(claimedByB) != 1 {
		t.Fatalf("worker-a释放租约后worker-b应能重试: len=%d err=%v", len(claimedByB), err)
	}
}

// TestDispatcherRunRetriesAfterPublishFailure（持续分发重试测试）验证短暂消息系统故障不会终止Worker。
func TestDispatcherRunRetriesAfterPublishFailure(t *testing.T) {
	store := NewMemoryStore()
	if err := store.Enqueue(context.Background(), Message{ID: "retry-1", Topic: "Match.Completed", AggregateID: "match-retry", Payload: []byte(`{}`), OccurredAt: time.Now().UTC()}); err != nil {
		t.Fatal(err)
	}
	publisher := &flakyPublisher{failuresRemaining: 1}
	dispatcher := NewDispatcherWithOptions(store, publisher, func() time.Time { return time.Now().UTC() }, "worker-retry", time.Second, 10)
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	if err := dispatcher.Run(ctx, 20*time.Millisecond, func(error) {}); err != nil {
		t.Fatal(err)
	}
	if publisher.successes != 1 {
		t.Fatalf("Worker应在失败后继续重试直至成功，successes=%d", publisher.successes)
	}
	if store.PendingCount() != 0 {
		t.Fatalf("成功重试后不应存在待发布消息，pending=%d", store.PendingCount())
	}
}

type flakyPublisher struct {
	failuresRemaining int
	successes         int
}

func (p *flakyPublisher) Publish(_ context.Context, _ Message) error {
	if p.failuresRemaining > 0 {
		p.failuresRemaining--
		return errors.New("temporary failure")
	}
	p.successes++
	return nil
}
