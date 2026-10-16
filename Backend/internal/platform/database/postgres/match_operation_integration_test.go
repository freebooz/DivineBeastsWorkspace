//go:build productiondeps

// 本文件只对显式隔离的party_arena_test_数据库schema做真实PG事务回归，不自动迁移或清理其他业务数据。
package postgres

import (
	"context"
	"divinebeasts/backend/internal/app/matchservice"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"errors"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"
)

// TestMatchOperationPGRecovery验证两个仓储实例共享持久租约、事件失败回滚和读取副本隔离。
func TestMatchOperationPGRecovery(t *testing.T) {
	dsn := os.Getenv("SSLM_PARTY_TEST_POSTGRES_DSN")
	if dsn == "" {
		t.Skip("未提供隔离PG，持久比赛步骤事务尚未执行")
	}
	if os.Getenv("SSLM_PARTY_TEST_POSTGRES_ALLOW_WRITES") != "1" {
		t.Fatal("必须显式授权隔离PG写入")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	pool, err := Open(ctx, Config{DSN: dsn, MaxConns: 4, PingTimeout: 5 * time.Second})
	if err != nil {
		t.Fatal("隔离PG连接失败，不打印连接凭据")
	}
	defer pool.Close()
	var schema string
	if err = pool.inner.QueryRow(ctx, `SELECT current_schema()`).Scan(&schema); err != nil || !strings.HasPrefix(schema, "party_arena_test_") {
		t.Fatal("只允许party_arena_test_隔离schema")
	}
	var count int
	if err = pool.inner.QueryRow(ctx, `SELECT count(*) FROM pg_class c JOIN pg_namespace n ON c.relnamespace=n.oid WHERE c.oid IN(to_regclass('match_operations'),to_regclass('outbox_messages')) AND n.nspname=$1`, schema).Scan(&count); err != nil || count != 2 {
		t.Fatal("两张真实表必须位于隔离schema，不能回退public")
	}
	id := "pg-operation-" + time.Now().UTC().Format("20060102150405.000000000")
	now := time.Now().UTC()
	value := matchservice.Operation{ID: id, MatchID: "match-" + id, MapID: "test-only-map", RuleRevision: 1, HeroCatalogRevision: 1, ContentRevision: "test-only", Stage: "allocating", CreatedAtUTC: now, DeadlineUTC: now.Add(time.Minute), Contexts: map[string]matchmaking.ActorContext{}, TicketIDs: map[string]string{}, Tickets: map[string]servertransfer.Ticket{}, Returns: map[string]matchservice.ReturnIntent{}}
	value.Candidate = matchmaking.Candidate{ID: id, GameID: "test-only", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "test", Roster: []matchmaking.CandidateSlot{{PlayerID: "a", TeamID: "team-a", Accepted: true}, {PlayerID: "b", TeamID: "team-b", Accepted: true}}}
	for _, slot := range value.Candidate.Roster {
		value.Contexts[slot.PlayerID] = matchmaking.ActorContext{PlayerID: slot.PlayerID, SessionID: "session-" + slot.PlayerID, CharacterID: "character-" + slot.PlayerID, HeroDefinitionID: "hero-test", CharacterRevision: 1}
		value.TicketIDs[slot.PlayerID] = "ticket-" + slot.PlayerID
	}
	value.InputFingerprint, _ = matchservice.OperationInputFingerprint(value)
	t.Cleanup(func() {
		cleanup, stop := context.WithTimeout(context.Background(), 5*time.Second)
		defer stop()
		_, _ = pool.inner.Exec(cleanup, `DELETE FROM outbox_messages WHERE aggregate_id=$1 AND topic='match.operation.changed'`, id)
		_, _ = pool.inner.Exec(cleanup, `DELETE FROM match_operations WHERE operation_id=$1`, id)
	})
	firstStore := NewMatchOperationStore(pool)
	secondStore := NewMatchOperationStore(pool)
	first, err := firstStore.ClaimOperation(ctx, value, "worker-a", now, 150*time.Millisecond)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = secondStore.ClaimOperation(ctx, value, "worker-b", now.Add(24*time.Hour), time.Second); !errors.Is(err, matchservice.ErrOperationBusy) {
		t.Fatal("必须由PG时钟决定租约，不能接受调用方未来时间", err)
	}
	time.Sleep(180 * time.Millisecond)
	second, err := secondStore.ClaimOperation(ctx, value, "worker-b", now, time.Second)
	if err != nil || second.Fence <= first.Fence {
		t.Fatal("持久接管必须递增Fence", err)
	}
	first.Stage = "signing"
	if _, err = firstStore.SaveOperation(ctx, first, now, time.Second); !errors.Is(err, matchservice.ErrOperationLeaseLost) {
		t.Fatal("旧工作者不能覆盖重启后的拥有者", err)
	}
	// 故意碰撞本测试下一版本事件ID，证明步骤更新不会在Outbox失败时独立提交。
	eventID := "match-operation:" + id + ":" + formatVersion(second.Version+1)
	if _, err = pool.inner.Exec(ctx, `INSERT INTO outbox_messages(message_id,topic,aggregate_id,payload,occurred_at) VALUES($1,'match.operation.changed',$2,'{}',NOW())`, eventID, id); err != nil {
		t.Fatal(err)
	}
	second.Stage = "signing"
	if _, err = secondStore.SaveOperation(ctx, second, now, time.Second); err == nil {
		t.Fatal("事件失败必须使整个步骤失败")
	}
	persisted, found, err := firstStore.GetOperation(ctx, id)
	if err != nil || !found || persisted.Stage != "allocating" || persisted.Version != second.Version {
		t.Fatal("必须回滚步骤版本及阶段", err)
	}
	if _, err = pool.inner.Exec(ctx, `DELETE FROM outbox_messages WHERE message_id=$1 AND aggregate_id=$2`, eventID, id); err != nil {
		t.Fatal(err)
	}
	second.Tickets["a"] = servertransfer.Ticket{Signature: "test-only-secret-must-not-appear-in-events"}
	saved, err := secondStore.SaveOperation(ctx, second, now, time.Second)
	if err != nil {
		t.Fatal(err)
	}
	var leaked bool
	if err = pool.inner.QueryRow(ctx, `SELECT EXISTS(SELECT 1 FROM outbox_messages WHERE aggregate_id=$1 AND payload::text LIKE '%test-only-secret%')`, id).Scan(&leaked); err != nil || leaked {
		t.Fatal("票据秘密不得进入事件", err)
	}
	if err = firstStore.ReleaseOperation(ctx, id, first.LeaseOwner, first.Fence); !errors.Is(err, matchservice.ErrOperationLeaseLost) {
		t.Fatal("旧释放必须拒绝", err)
	}
	if err = secondStore.ReleaseOperation(ctx, id, saved.LeaseOwner, saved.Fence); err != nil {
		t.Fatal(err)
	}
	recovered, err := firstStore.ClaimOperation(ctx, value, "worker-c", now, time.Second)
	if err != nil || recovered.Stage != "signing" || recovered.Tickets["a"].Signature == "" {
		t.Fatal("释放/重新领取必须恢复真实回执", err)
	}
}

func formatVersion(value int64) string { return strconv.FormatInt(value, 10) }
