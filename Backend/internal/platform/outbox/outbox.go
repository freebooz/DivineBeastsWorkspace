// Package outbox（事务发件箱）提供跨服务集成事件可靠发布所需的通用模型和Dispatcher Worker。
// 业务事务把事件与业务数据写入同一PostgreSQL事务，Dispatcher再异步投递到NATS JetStream。
package outbox

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"sort"
	"sync"
	"time"
)

const (
	StatePending    = "pending"    // StatePending（待发布）。
	StatePublishing = "publishing" // StatePublishing（已领取，正在发布）。
	StatePublished  = "published"  // StatePublished（已发布）。
)

// Message（Outbox消息）保存可靠发布所需的事件元数据和Payload。
type Message struct {
	ID          string    // ID（消息唯一ID，同时作为NATS JetStream去重键）。
	Topic       string    // Topic（消息主题，例如Match.Completed）。
	AggregateID string    // AggregateID（关联业务聚合ID，例如MatchID）。
	Payload     []byte    // Payload（JSON或Protobuf序列化后的事件正文）。
	OccurredAt  time.Time // OccurredAt（业务事件实际发生时间）。
	State       string    // State（pending/publishing/published）。
	Attempts    int       // Attempts（已经尝试发布的次数）。
	PublishedAt time.Time // PublishedAt（成功发布确认时间）。
	LockOwner   string    // LockOwner（领取该消息的Dispatcher实例ID）。
	LockedUntil time.Time // LockedUntil（发布租约过期时间）。
}

// Store（Outbox仓储接口）隔离PostgreSQL实现。
type Store interface {
	Enqueue(ctx context.Context, message Message) error
	ClaimPending(ctx context.Context, limit int, workerID string, leaseUntil time.Time) ([]Message, error)
	MarkPublished(ctx context.Context, messageID, workerID string, publishedAt time.Time) error
	MarkFailed(ctx context.Context, messageID, workerID string) error
}

// Publisher（事件发布端口）隔离NATS JetStream等具体消息系统。
type Publisher interface {
	Publish(ctx context.Context, message Message) error
}

// Dispatcher（Outbox发布器）领取Pending事件并在消息系统确认后更新Published状态。
type Dispatcher struct {
	store     Store
	publisher Publisher
	now       func() time.Time
	workerID  string
	lease     time.Duration
	batchSize int
}

// NewDispatcher（创建Outbox发布器）创建默认30秒租约、100条批量的Dispatcher。
func NewDispatcher(store Store, publisher Publisher, now func() time.Time) *Dispatcher {
	return NewDispatcherWithOptions(store, publisher, now, "outbox-worker", 30*time.Second, 100)
}

// NewDispatcherWithOptions（创建可配置Dispatcher）用于生产多副本安全领取。
func NewDispatcherWithOptions(store Store, publisher Publisher, now func() time.Time, workerID string, lease time.Duration, batchSize int) *Dispatcher {
	if store == nil || publisher == nil || now == nil || workerID == "" || lease <= 0 || batchSize <= 0 {
		panic("Outbox Dispatcher依赖和参数不能为空")
	}
	return &Dispatcher{store: store, publisher: publisher, now: now, workerID: workerID, lease: lease, batchSize: batchSize}
}

// Dispatch（发布一批Outbox事件）只在Publish成功后标记Published；失败时释放租约并增加Attempts。
func (d *Dispatcher) Dispatch(ctx context.Context, limit int) (int, error) {
	if limit <= 0 {
		limit = d.batchSize
	}
	now := d.now().UTC()
	messages, err := d.store.ClaimPending(ctx, limit, d.workerID, now.Add(d.lease))
	if err != nil {
		return 0, err
	}
	published := 0
	for _, message := range messages {
		if err := d.publisher.Publish(ctx, message); err != nil {
			_ = d.store.MarkFailed(ctx, message.ID, d.workerID)
			return published, err
		}
		if err := d.store.MarkPublished(ctx, message.ID, d.workerID, d.now().UTC()); err != nil {
			return published, err
		}
		published++
	}
	return published, nil
}

// Run（持续运行Dispatcher Worker）按固定间隔轮询；Context取消时立即退出。
// 发布错误不会终止Worker，而是在下一轮继续处理，以避免短暂NATS故障导致服务永久停止。
func (d *Dispatcher) Run(ctx context.Context, interval time.Duration, onError func(error)) error {
	if interval <= 0 {
		return errors.New("Outbox Worker interval必须大于0")
	}
	if onError == nil {
		onError = func(error) {}
	}
	// 启动时先立即处理一次，降低重启后的消息延迟。
	if _, err := d.Dispatch(ctx, d.batchSize); err != nil && !errors.Is(err, context.Canceled) {
		onError(err)
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return nil
		case <-ticker.C:
			if _, err := d.Dispatch(ctx, d.batchSize); err != nil && !errors.Is(err, context.Canceled) {
				onError(err)
			}
		}
	}
}

// MemoryStore（内存Outbox仓储）用于单元测试和本地开发。
type MemoryStore struct {
	mu    sync.RWMutex
	items map[string]Message
}

// NewMemoryStore（创建内存Outbox仓储）创建线程安全消息存储。
func NewMemoryStore() *MemoryStore { return &MemoryStore{items: map[string]Message{}} }

// Enqueue（写入待发布消息）以MessageID保证幂等；同ID不同内容会被拒绝。
func (s *MemoryStore) Enqueue(_ context.Context, message Message) error {
	if message.ID == "" || message.Topic == "" || len(message.Payload) == 0 {
		return errors.New("Outbox Message的ID、Topic和Payload不能为空")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if current, exists := s.items[message.ID]; exists {
		if current.Topic != message.Topic || current.AggregateID != message.AggregateID || !bytes.Equal(current.Payload, message.Payload) {
			return errors.New("OUTBOX_MESSAGE_CONFLICT: 同一MessageID对应不同事件内容")
		}
		return nil
	}
	message.Payload = append([]byte(nil), message.Payload...)
	message.State = StatePending
	s.items[message.ID] = message
	return nil
}

// ClaimPending（领取待发布消息）模拟PostgreSQL租约领取，过期publishing消息会自动重新进入可领取集合。
func (s *MemoryStore) ClaimPending(_ context.Context, limit int, workerID string, leaseUntil time.Time) ([]Message, error) {
	if limit <= 0 || workerID == "" || leaseUntil.IsZero() {
		return nil, errors.New("ClaimPending参数无效")
	}
	now := time.Now().UTC()
	s.mu.Lock()
	defer s.mu.Unlock()
	candidates := make([]Message, 0)
	for id, item := range s.items {
		if item.State == StatePublishing && !item.LockedUntil.IsZero() && !now.Before(item.LockedUntil) {
			item.State = StatePending
			item.LockOwner = ""
			item.LockedUntil = time.Time{}
			s.items[id] = item
		}
		if item.State == StatePending {
			candidates = append(candidates, item)
		}
	}
	sort.Slice(candidates, func(i, j int) bool {
		if candidates[i].OccurredAt.Equal(candidates[j].OccurredAt) {
			return candidates[i].ID < candidates[j].ID
		}
		return candidates[i].OccurredAt.Before(candidates[j].OccurredAt)
	})
	if len(candidates) > limit {
		candidates = candidates[:limit]
	}
	for i := range candidates {
		item := s.items[candidates[i].ID]
		item.State = StatePublishing
		item.LockOwner = workerID
		item.LockedUntil = leaseUntil.UTC()
		s.items[item.ID] = item
		candidates[i] = item
		candidates[i].Payload = append([]byte(nil), item.Payload...)
	}
	return candidates, nil
}

// MarkPublished（标记已发布）要求消息仍由当前Worker持有租约。
func (s *MemoryStore) MarkPublished(_ context.Context, messageID, workerID string, publishedAt time.Time) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	item, exists := s.items[messageID]
	if !exists {
		return errors.New("Outbox消息不存在")
	}
	if item.State == StatePublished {
		return nil
	}
	if item.State != StatePublishing || item.LockOwner != workerID {
		return fmt.Errorf("OUTBOX_LEASE_LOST: message=%s", messageID)
	}
	item.State = StatePublished
	item.PublishedAt = publishedAt.UTC()
	item.LockOwner = ""
	item.LockedUntil = time.Time{}
	s.items[messageID] = item
	return nil
}

// MarkFailed（记录发布失败）增加Attempts并立即释放租约，使后续轮次可以重试。
func (s *MemoryStore) MarkFailed(_ context.Context, messageID, workerID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	item, exists := s.items[messageID]
	if !exists {
		return errors.New("Outbox消息不存在")
	}
	if item.State == StatePublishing && item.LockOwner != workerID {
		return errors.New("OUTBOX_LEASE_LOST: Outbox消息由其他Worker持有")
	}
	item.Attempts++
	item.State = StatePending
	item.LockOwner = ""
	item.LockedUntil = time.Time{}
	s.items[messageID] = item
	return nil
}

// PendingCount（待发布数量）仅用于测试和本地诊断。
func (s *MemoryStore) PendingCount() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	count := 0
	for _, item := range s.items {
		if item.State == StatePending || item.State == StatePublishing {
			count++
		}
	}
	return count
}
