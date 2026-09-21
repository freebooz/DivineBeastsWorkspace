//go:build productiondeps

package postgres

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"reflect"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/jackc/pgx/v5/pgconn"

	"divinebeasts/backend/internal/modules/playerdata"
)

// 真实PG测试只连接显式授权且预先迁移的隔离schema；不启动DB、不执行迁移、不清库。
// 缺少配置必须SKIP，绝不以连接替身或固定成功冒充数据库事务验证。
func onlinePlayerFixture(t *testing.T) (*Pool, *playerdata.Service, string) {
	t.Helper()
	dsn := os.Getenv("ONLINE_PLAYER_TEST_DSN")
	if dsn == "" {
		t.Skip("未提供隔离PG：真实玩家资料事务/持久化测试未执行")
	}
	if os.Getenv("ONLINE_PLAYER_TEST_ALLOW_WRITES") != "1" {
		t.Fatal("必须显式授权ONLINE_PLAYER_TEST_ALLOW_WRITES=1")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	pool, err := Open(ctx, Config{DSN: dsn, MaxConns: 16, PingTimeout: 5 * time.Second})
	if err != nil {
		t.Fatal("隔离PG连接失败；不输出可能包含凭据的底层连接错误")
	}
	t.Cleanup(pool.Close)
	var schema string
	if err := pool.inner.QueryRow(ctx, `SELECT current_schema()`).Scan(&schema); err != nil || !strings.HasPrefix(schema, "online_player_test_") {
		t.Fatal("只允许预先准备的online_player_test_前缀schema；禁止默认public或生产schema")
	}
	// current_schema合法仍不足以阻止search_path回退public；两张可见表都必须实际属于该schema。
	var isolatedTables int
	if err := pool.inner.QueryRow(ctx, `SELECT count(*) FROM pg_catalog.pg_class c
		JOIN pg_catalog.pg_namespace n ON n.oid=c.relnamespace
		WHERE c.oid IN (to_regclass('player_profiles'),to_regclass('online_profile_idempotency'))
		AND n.nspname=$1 AND c.relkind='r'`, schema).Scan(&isolatedTables); err != nil || isolatedTables != 2 {
		t.Fatal("资料与幂等表必须实际位于隔离schema，禁止search_path回退其他业务表")
	}
	service := playerdata.NewService(NewOnlinePlayerRepository(pool))
	if err := service.Probe(ctx); err != nil {
		t.Fatal("隔离schema须事先应用核心表与000006迁移", err)
	}
	var random [16]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	playerID := "online-test-" + hex.EncodeToString(random[:])
	t.Cleanup(func() {
		cleanupCtx, stop := context.WithTimeout(context.Background(), 5*time.Second)
		defer stop()
		// 仅清理本测试随机主体的两张表记录；从不按前缀批量删除或删除schema。
		if _, err := pool.inner.Exec(cleanupCtx, `DELETE FROM online_profile_idempotency WHERE player_id=$1`, playerID); err != nil {
			t.Error("本测试幂等记录清理失败")
		}
		if _, err := pool.inner.Exec(cleanupCtx, `DELETE FROM player_profiles WHERE player_id=$1`, playerID); err != nil {
			t.Error("本测试资料清理失败")
		}
	})
	return pool, service, playerID
}

func TestOnlinePlayerPGReplayPersistsAndOnlyEditsDisplayName(t *testing.T) {
	pool, service, playerID := onlinePlayerFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	_, err := service.GetProfile(ctx, playerID)
	requirePlayerCode(t, err, "PLAYER_PROFILE_NOT_FOUND")
	_, err = service.UpdateDisplayNameIdempotent(ctx, playerID, "不存在", 1, "missing")
	requirePlayerCode(t, err, "PLAYER_PROFILE_NOT_FOUND")
	if err := service.EnsureProfile(ctx, playerID, "game-a"); err != nil {
		t.Fatal(err)
	}
	// 测试专属权威字段夹具：白名单更新必须保留这些非可编辑字段。
	_, err = pool.inner.Exec(ctx, `UPDATE player_profiles SET data_version=3,tutorial_completed=TRUE,
		default_world_id='world-fixture',owned_character_ids='["owned-fixture"]'::jsonb WHERE player_id=$1`, playerID)
	if err != nil {
		t.Fatal(err)
	}
	first, err := service.UpdateDisplayNameIdempotent(ctx, playerID, "  名称一  ", 1, "same-key")
	if err != nil {
		t.Fatal(err)
	}
	want := playerdata.Profile{PlayerID: playerID, GameID: "game-a", DisplayName: "名称一", DataVersion: 3, Revision: 2,
		TutorialCompleted: true, DefaultWorldID: "world-fixture", OwnedCharacterIDs: []string{"owned-fixture"}}
	if !reflect.DeepEqual(first, want) {
		t.Fatalf("白名单更新结果错误: %+v", first)
	}
	if _, err := service.UpdateDisplayNameIdempotent(ctx, playerID, "名称二", 2, "new-key"); err != nil {
		t.Fatal(err)
	}
	// 创建独立客户端连接池与仓储，排除仓储内存结果缓存；这不是后端进程重启验收。
	reopened, err := Open(ctx, Config{DSN: os.Getenv("ONLINE_PLAYER_TEST_DSN"), MaxConns: 2, PingTimeout: 5 * time.Second})
	if err != nil {
		t.Fatal("重开测试连接失败")
	}
	defer reopened.Close()
	fresh := playerdata.NewService(NewOnlinePlayerRepository(reopened))
	replay, err := fresh.UpdateDisplayNameIdempotent(ctx, playerID, "名称一", 1, "same-key")
	if err != nil || !reflect.DeepEqual(replay, first) {
		t.Fatalf("旧revision重试未重放首次持久结果: %+v %v", replay, err)
	}
	_, err = fresh.UpdateDisplayNameIdempotent(ctx, playerID, "异内容", 1, "same-key")
	requirePlayerCode(t, err, "IDEMPOTENCY_CONFLICT")
	_, err = fresh.UpdateDisplayNameIdempotent(ctx, playerID, "名称一", 2, "same-key")
	requirePlayerCode(t, err, "IDEMPOTENCY_CONFLICT")
	_, err = fresh.UpdateDisplayNameIdempotent(ctx, playerID, "旧修订", 1, "another-key")
	requirePlayerCode(t, err, "PLAYER_DATA_CONFLICT")
	if err := fresh.EnsureProfile(ctx, playerID, "game-a"); err != nil {
		t.Fatal(err)
	}
	requirePlayerCode(t, fresh.EnsureProfile(ctx, playerID, "game-b"), "PLAYER_DATA_CONFLICT")
	current, err := fresh.GetProfile(ctx, playerID)
	if err != nil || current.DisplayName != "名称二" || current.Revision != 3 {
		t.Fatalf("重放/Ensure覆盖当前资料: %+v %v", current, err)
	}
	var count int
	if err := pool.inner.QueryRow(ctx, `SELECT count(*) FROM online_profile_idempotency WHERE player_id=$1`, playerID).Scan(&count); err != nil || count != 2 {
		t.Fatalf("失败写入留下幂等记录: %d %v", count, err)
	}
}

// 同一键同内容只更新一次；同键异内容、新键旧修订分别由不同冲突码表达。
func TestOnlinePlayerPGConcurrentUpdates(t *testing.T) {
	if os.Getenv("ONLINE_PLAYER_TEST_DSN") == "" {
		t.Skip("未提供隔离PG：真实并发测试未执行")
	}
	for _, mode := range []string{"same-request", "different-content", "different-keys"} {
		t.Run(mode, func(t *testing.T) {
			pool, service, playerID := onlinePlayerFixture(t)
			ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
			defer cancel()
			if err := service.EnsureProfile(ctx, playerID, "game"); err != nil {
				t.Fatal(err)
			}
			type result struct {
				profile playerdata.Profile
				err     error
			}
			results := make(chan result, 8)
			start := make(chan struct{})
			var group sync.WaitGroup
			for i := 0; i < 8; i++ {
				group.Add(1)
				go func(index int) {
					defer group.Done()
					<-start
					name, key := "同名", "key"
					if mode == "different-content" {
						name = fmt.Sprintf("姓名%d", index)
					}
					if mode == "different-keys" {
						key = fmt.Sprintf("key-%d", index)
					}
					profile, err := service.UpdateDisplayNameIdempotent(ctx, playerID, name, 1, key)
					results <- result{profile, err}
				}(i)
			}
			close(start)
			group.Wait()
			close(results)
			successes := 0
			for result := range results {
				if result.err == nil {
					successes++
					if result.profile.Revision != 2 {
						t.Error("重复修改revision")
					}
					continue
				}
				code := "IDEMPOTENCY_CONFLICT"
				if mode == "different-keys" {
					code = "PLAYER_DATA_CONFLICT"
				}
				requirePlayerCode(t, result.err, code)
			}
			wantSuccesses := 1
			if mode == "same-request" {
				wantSuccesses = 8
			}
			if successes != wantSuccesses {
				t.Fatalf("成功数%d，期望%d", successes, wantSuccesses)
			}
			var records int
			if err := pool.inner.QueryRow(ctx, `SELECT count(*) FROM online_profile_idempotency WHERE player_id=$1`, playerID).Scan(&records); err != nil || records != 1 {
				t.Fatalf("持久结果不唯一: %d %v", records, err)
			}
		})
	}
}

func TestOnlinePlayerPGKeysAreScopedToSubject(t *testing.T) {
	_, left, leftID := onlinePlayerFixture(t)
	_, right, rightID := onlinePlayerFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	for _, tc := range []struct {
		service  *playerdata.Service
		id, name string
	}{{left, leftID, "甲"}, {right, rightID, "乙"}} {
		if err := tc.service.EnsureProfile(ctx, tc.id, "game"); err != nil {
			t.Fatal(err)
		}
		profile, err := tc.service.UpdateDisplayNameIdempotent(ctx, tc.id, tc.name, 1, "shared-key")
		if err != nil || profile.PlayerID != tc.id || profile.DisplayName != tc.name {
			t.Fatal("同键串主体", profile, err)
		}
	}
}

// 在真实PG表锁下让结果INSERT等待：取消后，之前的UPDATE也必须回滚且原键可重新使用。
// 锁仅用于预先批准的隔离schema，不能在共享/生产schema运行此故障注入。
func TestOnlinePlayerPGCancelledInsertRollsBackProfile(t *testing.T) {
	pool, service, playerID := onlinePlayerFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	if err := service.EnsureProfile(ctx, playerID, "game"); err != nil {
		t.Fatal(err)
	}
	blocker, err := pool.inner.Begin(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer rollbackOnlinePlayer(blocker)
	if _, err := blocker.Exec(ctx, `LOCK TABLE online_profile_idempotency IN SHARE MODE`); err != nil {
		t.Fatal(err)
	}
	writeCtx, stopWrite := context.WithCancel(ctx)
	defer stopWrite()
	done := make(chan error, 1)
	go func() {
		_, err := service.UpdateDisplayNameIdempotent(writeCtx, playerID, "不可部分提交", 1, "retry-key")
		done <- err
	}()
	// UPDATE持有行锁而INSERT被表锁阻塞，证明取消发生在写资料之后，而非请求开始前。
	deadline := time.Now().Add(5 * time.Second)
	locked := false
	for time.Now().Before(deadline) {
		var revision int64
		err := pool.inner.QueryRow(ctx, `SELECT revision FROM player_profiles WHERE player_id=$1 FOR UPDATE NOWAIT`, playerID).Scan(&revision)
		var pgError *pgconn.PgError
		if errors.As(err, &pgError) && pgError.Code == "55P03" {
			locked = true
			break
		}
		if err != nil {
			stopWrite()
			<-done
			t.Fatal(err)
		}
		time.Sleep(10 * time.Millisecond)
	}
	stopWrite()
	err = <-done
	if !locked {
		t.Fatal("未观察到写入后的行锁，故障注入没有命中目标窗口")
	}
	if !errors.Is(err, context.Canceled) {
		t.Fatalf("取消未真实中断等待: %v", err)
	}
	if err := blocker.Rollback(ctx); err != nil {
		t.Fatal(err)
	}
	profile, err := service.GetProfile(ctx, playerID)
	if err != nil || profile.DisplayName != "新玩家" || profile.Revision != 1 {
		t.Fatalf("事务失败留下部分资料更新: %+v %v", profile, err)
	}
	var records int
	if err := pool.inner.QueryRow(ctx, `SELECT count(*) FROM online_profile_idempotency WHERE player_id=$1`, playerID).Scan(&records); err != nil || records != 0 {
		t.Fatalf("事务失败留下幂等行: %d %v", records, err)
	}
	if _, err := service.UpdateDisplayNameIdempotent(ctx, playerID, "不可部分提交", 1, "retry-key"); err != nil {
		t.Fatal("回滚后原键不可重试", err)
	}
}

func TestOnlinePlayerPGCancelledKeyWait(t *testing.T) {
	pool, service, playerID := onlinePlayerFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := service.EnsureProfile(ctx, playerID, "game"); err != nil {
		t.Fatal(err)
	}
	blocker, err := pool.inner.Begin(ctx)
	if err != nil {
		t.Fatal(err)
	}
	defer rollbackOnlinePlayer(blocker)
	if _, err := blocker.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`, onlinePlayerLockID(playerID, "key")); err != nil {
		t.Fatal(err)
	}
	short, stop := context.WithTimeout(ctx, 100*time.Millisecond)
	defer stop()
	_, err = service.UpdateDisplayNameIdempotent(short, playerID, "姓名", 1, "key")
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatal("等待同键锁没有响应截止时间", err)
	}
	if err := blocker.Rollback(ctx); err != nil {
		t.Fatal(err)
	}
	if _, err := service.UpdateDisplayNameIdempotent(ctx, playerID, "姓名", 1, "key"); err != nil {
		t.Fatal(err)
	}
	_, err = service.UpdateDisplayNameIdempotent(ctx, playerID, "姓名", 1, "key")
	if err != nil {
		t.Fatal("锁取消后的重试未正常重放", err)
	}
}
