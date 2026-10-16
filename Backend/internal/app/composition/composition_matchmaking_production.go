//go:build productiondeps

// 本文件装配比赛步骤事务Outbox发布；事件不含票据秘密，失败保留PG待发布记录，由同一正式Dispatcher重试。
package composition

import (
	"context"
	"divinebeasts/backend/internal/platform/config"
	"divinebeasts/backend/internal/platform/database/postgres"
	"divinebeasts/backend/internal/platform/messaging/natsjs"
	"divinebeasts/backend/internal/platform/outbox"
	"errors"
	"log/slog"
	"time"
)

// startMatchOperationOutbox要求启用匹配时同时具备真实NATS与PG；返回关闭函数用于宿主退出时取消并等待Dispatcher。
func startMatchOperationOutbox(ctx context.Context, pool *postgres.Pool, name string) (func(), error) {
	url := config.Getenv("NATS_URL", "")
	if url == "" {
		return nil, errors.New("MATCH_OUTBOX_NOT_CONFIGURED: 启用匹配必须配置NATS")
	}
	publisher, err := natsjs.Open(natsjs.Config{URL: url, ClientName: name + "-match-operations", ConnectTimeout: 5 * time.Second, ReconnectWait: 2 * time.Second})
	if err != nil {
		return nil, err
	}
	owned, cancel := context.WithCancel(ctx)
	done := make(chan struct{})
	dispatcher := outbox.NewDispatcherWithOptions(postgres.NewMatchOutboxStore(pool), natsjs.NewOutboxPublisher(publisher), func() time.Time { return time.Now().UTC() }, name+":"+config.Getenv("POD_NAME", "local")+":match", 30*time.Second, 100)
	go func() {
		defer close(done)
		_ = dispatcher.Run(owned, 500*time.Millisecond, func(error) { slog.Warn("比赛步骤事件等待重发") })
	}()
	return func() { cancel(); <-done; publisher.Close() }, nil
}
