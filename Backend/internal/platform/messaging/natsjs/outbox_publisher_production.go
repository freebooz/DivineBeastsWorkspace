//go:build productiondeps

package natsjs

import (
	"context"

	"divinebeasts/backend/internal/platform/outbox"
)

// OutboxPublisher（事务发件箱NATS发布适配器）把Outbox Message映射到JetStream Subject和MsgID。
type OutboxPublisher struct{ publisher *Publisher }

// NewOutboxPublisher（创建Outbox NATS发布器）绑定共享JetStream连接。
func NewOutboxPublisher(publisher *Publisher) *OutboxPublisher {
	if publisher == nil {
		panic("NATS Publisher不能为空")
	}
	return &OutboxPublisher{publisher: publisher}
}

// Publish（发布Outbox消息）使用Message.ID作为JetStream去重MsgID。
func (p *OutboxPublisher) Publish(ctx context.Context, message outbox.Message) error {
	return p.publisher.Publish(ctx, message.Topic, message.ID, message.Payload)
}

var _ outbox.Publisher = (*OutboxPublisher)(nil)
