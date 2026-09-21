package tests

import (
	"path/filepath"
	"strings"
	"testing"
)

// TestProductionCompositionWiresInfrastructure（生产装配门禁测试）防止生产适配器退化为未接线骨架。
func TestProductionCompositionWiresInfrastructure(t *testing.T) {
	httpComposition := mustRead(t, filepath.Join("..", "internal", "app", "composition", "composition_production_http.go"))
	grpcComposition := mustRead(t, filepath.Join("..", "internal", "app", "composition", "composition_production_grpc.go"))
	combined := httpComposition + grpcComposition
	// 两种生产链必须各自接入持久身份与Online资料仓储，不能由另一种链补齐词串。
	for name, content := range map[string]string{"http": httpComposition, "grpc": grpcComposition} {
		for _, expected := range []string{"identity.NewPersistentService(postgres.NewOnlineIdentityRepository(pool)", "playerdata.NewService(postgres.NewOnlinePlayerRepository(pool))"} {
			if !strings.Contains(content, expected) {
				t.Errorf("%s生产Composition缺少Online持久化接线: %s", name, expected)
			}
		}
	}
	for _, expected := range []string{
		"postgres.Open",
		"postgres.NewOnlinePlayerRepository",
		"postgres.NewOnlineIdentityRepository",
		"identity.NewPersistentService",
		"postgres.NewMatchOutboxStore",
		"redisstore.Open",
		"redisstore.NewPartyRepository",
		"redisstore.NewTicketRepository",
		"redisstore.NewTransferReplayStore",
		"natsjs.Open",
		"natsjs.NewOutboxPublisher",
		"agones.NewClient",
		"agones.NewRegistryBackedAllocator",
		"outbox.NewDispatcherWithOptions",
	} {
		if !strings.Contains(combined, expected) {
			t.Fatalf("生产Composition缺少适配器接线: %s", expected)
		}
	}
}

// TestRedisTransferReplayUsesAtomicSetNX（Redis防重放原子性测试）锁定TransferTicket消费必须使用SET NX + TTL语义。
func TestRedisTransferReplayUsesAtomicSetNX(t *testing.T) {
	content := mustRead(t, filepath.Join("..", "internal", "platform", "cache", "redisstore", "repositories_production.go"))
	for _, expected := range []string{"type TransferReplayStore", "SetNX", "transferReplayKeyPrefix", "servertransfer.ReplayStore"} {
		if !strings.Contains(content, expected) {
			t.Fatalf("Redis TransferTicket防重放实现缺少关键语义: %s", expected)
		}
	}
}

// TestOutboxProductionUsesLeaseAndSkipLocked（Outbox生产并发安全测试）锁定多副本Dispatcher的数据库领取语义。
func TestOutboxProductionUsesLeaseAndSkipLocked(t *testing.T) {
	content := mustRead(t, filepath.Join("..", "internal", "platform", "database", "postgres", "repositories_production.go"))
	for _, expected := range []string{"FOR UPDATE SKIP LOCKED", "locked_until", "lock_owner", "InsertWithEvent", "tx.Commit"} {
		if !strings.Contains(content, expected) {
			t.Fatalf("Transactional Outbox生产实现缺少关键语义: %s", expected)
		}
	}
}
