//go:build productiondeps

// 本文件只对显式隔离的party_arena_test_数据库schema做真实PG事务回归，不自动迁移或清理其他业务数据。
package postgres

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/app/matchservice"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"errors"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

func openIsolatedMatchOperationPG(t *testing.T, ctx context.Context) *Pool {
	t.Helper()
	dsn := os.Getenv("SSLM_PARTY_TEST_POSTGRES_DSN")
	if dsn == "" {
		t.Skip("未提供隔离PG，持久比赛步骤事务尚未执行")
	}
	if os.Getenv("SSLM_PARTY_TEST_POSTGRES_ALLOW_WRITES") != "1" {
		t.Fatal("必须显式授权隔离PG写入")
	}
	pool, err := Open(ctx, Config{DSN: dsn, MaxConns: 4, PingTimeout: 5 * time.Second})
	if err != nil {
		t.Fatal("隔离PG连接失败，不打印连接凭据")
	}
	t.Cleanup(pool.Close)
	var schema string
	if err = pool.inner.QueryRow(ctx, `SELECT current_schema()`).Scan(&schema); err != nil || !strings.HasPrefix(schema, "party_arena_test_") {
		t.Fatal("只允许party_arena_test_隔离schema")
	}
	var count int
	if err = pool.inner.QueryRow(ctx, `SELECT count(*) FROM pg_class c JOIN pg_namespace n ON c.relnamespace=n.oid WHERE c.oid IN(to_regclass('match_operations'),to_regclass('outbox_messages')) AND n.nspname=$1`, schema).Scan(&count); err != nil || count != 2 {
		t.Fatal("两张真实表必须位于隔离schema，不能回退public")
	}
	return pool
}

func cleanupMatchOperationPG(t *testing.T, pool *Pool, id string) {
	t.Helper()
	t.Cleanup(func() {
		cleanup, stop := context.WithTimeout(context.Background(), 5*time.Second)
		defer stop()
		_, _ = pool.inner.Exec(cleanup, `DELETE FROM outbox_messages WHERE aggregate_id=$1 AND topic='match.operation.changed'`, id)
		_, _ = pool.inner.Exec(cleanup, `DELETE FROM match_operations WHERE operation_id=$1`, id)
	})
}

// TestMatchOperationPGRecovery验证两个仓储实例共享持久租约、事件失败回滚和读取副本隔离。
func TestMatchOperationPGRecovery(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	pool := openIsolatedMatchOperationPG(t, ctx)
	id := "pg-operation-" + time.Now().UTC().Format("20060102150405.000000000")
	now := time.Now().UTC()
	value := matchservice.Operation{ID: id, MatchID: "match-" + id, MapID: "test-only-map", RuleRevision: 1, HeroCatalogRevision: 1, ContentRevision: "test-only", Stage: "allocating", CreatedAtUTC: now, DeadlineUTC: now.Add(time.Minute), Contexts: map[string]matchmaking.ActorContext{}, TicketIDs: map[string]string{}, Tickets: map[string]servertransfer.Ticket{}, Returns: map[string]matchservice.ReturnIntent{}}
	value.Candidate = matchmaking.Candidate{ID: id, GameID: "test-only", ArenaModeID: "Arena.Mode.Duel1v1", RegionID: "test", Roster: []matchmaking.CandidateSlot{{PlayerID: "a", TeamID: "team-a", Accepted: true}, {PlayerID: "b", TeamID: "team-b", Accepted: true}}}
	for _, slot := range value.Candidate.Roster {
		value.Contexts[slot.PlayerID] = matchmaking.ActorContext{PlayerID: slot.PlayerID, SessionID: "session-" + slot.PlayerID, CharacterID: "character-" + slot.PlayerID, HeroDefinitionID: "hero-test", CharacterRevision: 1}
		value.TicketIDs[slot.PlayerID] = "ticket-" + slot.PlayerID
	}
	value.InputFingerprint, _ = matchservice.OperationInputFingerprint(value)
	cleanupMatchOperationPG(t, pool, id)
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

// TestPGConcurrentInitialArenaMigration使另一个真实仓储持续持有写租约，再同时读取2人或10人的已提交票据。
func TestPGConcurrentInitialArenaMigration(t *testing.T) {
	for _, size := range []int{1, 5} {
		t.Run(fmt.Sprintf("%dv%d", size, size), func(t *testing.T) {
			ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
			defer cancel()
			pool := openIsolatedMatchOperationPG(t, ctx)
			control := gameservercontrol.NewService(gameserver.NewRegistry(), servertransfer.NewService([]byte("01234567890123456789012345678901"), time.Now), match.NewResultService(match.NewMemoryResultStore()), time.Now)
			if err := control.ConfigureArenaLifecycle(gameservercontrol.NewMemoryArenaLifecycleStore()); err != nil {
				t.Fatal(err)
			}
			if err := control.Register(gameservercontrol.RegisterInput{GameID: "test-game", GameServerID: "arena", ServerBootID: "test-boot", ServerRoleID: gc.RoleMainArena, ExperienceID: gc.ExperienceMainArenaMain, WorldID: "test-map", RegionID: "test", PublicEndpoint: "127.0.0.1:17782", ProtocolVersion: 1, Capacity: 10}); err != nil {
				t.Fatal(err)
			}
			if err := control.SetReady("arena"); err != nil {
				t.Fatal(err)
			}
			id := fmt.Sprintf("pg-arena-migration-%d", time.Now().UnixNano())
			cleanupMatchOperationPG(t, pool, id)
			now := time.Now().UTC()
			mode := gc.ArenaMode1v1
			if size == 5 {
				mode = gc.ArenaMode5v5
			}
			value := matchservice.Operation{ID: id, MatchID: "match-" + id, MapID: "test-map", RuleRevision: 1, HeroCatalogRevision: 1, ContentRevision: "test", Stage: "committed", CreatedAtUTC: now, DeadlineUTC: now.Add(time.Minute), Contexts: map[string]matchmaking.ActorContext{}, TicketIDs: map[string]string{}, Tickets: map[string]servertransfer.Ticket{}, Returns: map[string]matchservice.ReturnIntent{}}
			value.Candidate = matchmaking.Candidate{ID: id, GameID: "test-game", ArenaModeID: mode, RegionID: "test"}
			allocate := gameservercontrol.AllocateMainArenaInput{MatchID: value.MatchID, ArenaModeID: mode, MapID: value.MapID, WorldID: value.MapID, RegionID: "test"}
			for index := 0; index < 2*size; index++ {
				player := fmt.Sprintf("player-%02d", index)
				team := "team-a"
				if index >= size {
					team = "team-b"
				}
				actor := matchmaking.ActorContext{PlayerID: player, SessionID: "session-" + player, CharacterID: "character-" + player, HeroDefinitionID: "hero-test", CharacterRevision: 1}
				value.Contexts[player] = actor
				value.Candidate.Roster = append(value.Candidate.Roster, matchmaking.CandidateSlot{PlayerID: player, TeamID: team, Accepted: true})
				allocate.Roster = append(allocate.Roster, gc.RosterPlayer{PlayerID: player, CharacterID: actor.CharacterID, TeamID: team})
				value.TicketIDs[player] = "enter-" + player
			}
			assignment, err := control.AllocateMainArenaContext(ctx, allocate)
			if err != nil {
				t.Fatal(err)
			}
			value.Assignment = &assignment
			for player, actor := range value.Contexts {
				ticket, err := control.IssueTransferContext(ctx, gameservercontrol.IssueTransferInput{TicketID: value.TicketIDs[player], GameID: value.Candidate.GameID, PlayerID: player, SessionID: actor.SessionID, DestinationGameServerID: "arena", MatchID: value.MatchID, TTL: time.Minute})
				if err != nil {
					t.Fatal(err)
				}
				value.Tickets[player] = ticket
			}
			value.InputFingerprint, err = matchservice.OperationInputFingerprint(value)
			if err != nil {
				t.Fatal(err)
			}
			writer := NewMatchOperationStore(pool)
			lease, err := writer.ClaimOperation(ctx, value, "match-worker", now, 20*time.Second)
			if err != nil {
				t.Fatal(err)
			}
			var sequence atomic.Int64
			nextID := func(prefix string) string { return fmt.Sprintf("%s-%d", prefix, sequence.Add(1)) }
			queue := matchapi.NewService(matchapi.NewMemoryPartyRepository(), matchapi.NewMemoryTicketRepository(), nextID)
			runtime, err := matchservice.NewRuntime(NewMatchOperationStore(pool), queue, control, matchservice.RuntimeConfig{MapID: value.MapID, ContentRevision: value.ContentRevision, RuleRevision: 1, HeroCatalogRevision: 1}, nextID)
			if err != nil {
				t.Fatal(err)
			}
			request := matchmaking.MigrationRequest{CandidateID: id, Direction: "arena", ClientRequestID: "entry"}
			start := make(chan struct{})
			results := make(chan error, 2*size)
			for player, actor := range value.Contexts {
				go func() {
					<-start
					migration, err := runtime.GetMigration(ctx, actor, request)
					if err == nil && (migration.TicketID != value.TicketIDs[player] || migration.ServerBootID != "test-boot") {
						err = errors.New("已提交票据身份或Boot不一致")
					}
					results <- err
				}()
			}
			close(start)
			for range 2 * size {
				if err := <-results; err != nil {
					t.Errorf("已提交票据读争用了PG写租约: %v", err)
				}
			}
			actor := value.Contexts["player-00"]
			actor.SessionID = "reauthenticated"
			if _, err := runtime.GetMigration(ctx, actor, request); !errors.Is(err, matchservice.ErrOperationBusy) {
				t.Errorf("新会话签票未由实际PG租约拒绝: %v", err)
			}
			persisted, found, err := writer.GetOperation(ctx, id)
			if err != nil || !found || persisted.Fence != lease.Fence || persisted.Version != lease.Version || persisted.LeaseOwner != lease.LeaseOwner {
				t.Errorf("并发只读请求改变了持久租约: %v", err)
			}
		})
	}
}

func formatVersion(value int64) string { return strconv.FormatInt(value, 10) }
