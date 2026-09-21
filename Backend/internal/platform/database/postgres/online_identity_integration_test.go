//go:build productiondeps

package postgres

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"os"
	"strings"
	"sync"
	"testing"
	"time"

	"divinebeasts/backend/internal/modules/identity"
)

// 真实库测试不会创建数据库或应用迁移。必须由操作者先批准隔离库，再提供两个显式环境值。
// 未配置时SKIP，不能计入数据库或重启持久化通过数；DSN/凭据绝不输出日志。
func onlineIdentityIntegration(t *testing.T) (*Pool, *identity.Service, string, string) {
	t.Helper()
	dsn := os.Getenv("ONLINE_IDENTITY_TEST_DSN")
	if os.Getenv("ONLINE_IDENTITY_TEST_ISOLATED") != "1" || dsn == "" {
		t.Skip("未授权隔离PG：真实身份并发/持久化测试未执行")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	p, err := Open(ctx, Config{DSN: dsn, MaxConns: 8, PingTimeout: 5 * time.Second})
	if err != nil {
		t.Fatal("无法连接指定隔离数据库")
	}
	t.Cleanup(p.Close)
	var schema string
	if err = p.inner.QueryRow(ctx, `SELECT current_schema()`).Scan(&schema); err != nil || !strings.HasPrefix(schema, "online_identity_test_") {
		t.Fatal("真实测试仅允许online_identity_test_前缀schema，不允许默认public")
	}
	var isolatedTables int
	if err = p.inner.QueryRow(ctx, `SELECT count(*) FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n ON n.oid=c.relnamespace
 WHERE c.oid IN (to_regclass('online_identity_accounts'),to_regclass('online_identity_sessions'),to_regclass('online_identity_refresh_credentials'))
 AND n.nspname=$1 AND c.relkind='r'`, schema).Scan(&isolatedTables); err != nil || isolatedTables != 3 {
		t.Fatal("三张身份表必须实际位于隔离schema，禁止search_path回退业务表")
	}
	var b [16]byte
	if _, err = rand.Read(b[:]); err != nil {
		t.Fatal("测试身份生成失败")
	}
	game := "identity-test-" + hex.EncodeToString(b[:])
	password := hex.EncodeToString(b[:]) + "-test-only"
	s, err := identity.NewPersistentService(NewOnlineIdentityRepository(p), identity.SystemClock{}, time.Minute, time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	if err = s.Probe(ctx); err != nil {
		t.Fatal("所需身份迁移未就绪；测试不会自动应用")
	}
	if _, err = s.EnsureAccount(ctx, game, "account-a", password); err != nil {
		t.Fatal(err)
	}
	// 仅清理本用例随机gameID所有的记录，使用同一事务，不触碰其他测试或玩家资料。
	t.Cleanup(func() {
		cleanupCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		tx, e := p.inner.Begin(cleanupCtx)
		if e != nil {
			t.Error("清理本次身份记录失败")
			return
		}
		defer tx.Rollback(cleanupCtx)
		for _, query := range []string{`DELETE FROM online_identity_refresh_credentials WHERE session_id IN (SELECT session_id FROM online_identity_sessions WHERE game_id=$1)`, `DELETE FROM online_identity_sessions WHERE game_id=$1`, `DELETE FROM online_identity_accounts WHERE game_id=$1`} {
			if _, e = tx.Exec(cleanupCtx, query, game); e != nil {
				t.Error("清理本次身份记录失败")
				return
			}
		}
		if tx.Commit(cleanupCtx) != nil {
			t.Error("提交测试清理失败")
		}
	})
	return p, s, game, password
}

func loginOnlineIdentity(t *testing.T, s *identity.Service, game, password string) identity.Session {
	t.Helper()
	session, err := s.LoginPassword(context.Background(), game, "account-a", password, "test-device")
	if err != nil {
		t.Fatal(err)
	}
	return session
}

// 两个真实连接争抢同一刷新：最多一次成功，第二次检测重放并撤销成功方的新令牌。
func TestOnlineIdentityPGConcurrentRefreshReplayRevokes(t *testing.T) {
	_, s, game, password := onlineIdentityIntegration(t)
	old := loginOnlineIdentity(t, s, game, password)
	start := make(chan struct{})
	var wg sync.WaitGroup
	results := make([]identity.Session, 2)
	errs := make([]error, 2)
	for i := 0; i < 2; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			<-start
			results[i], errs[i] = s.Refresh(context.Background(), old.RefreshToken)
		}(i)
	}
	close(start)
	wg.Wait()
	success := 0
	for i, e := range errs {
		if e == nil {
			success++
			if _, e = s.Authenticate(context.Background(), results[i].AccessToken); !errors.Is(e, identity.ErrInvalidToken) {
				t.Fatal("重放必须撤销第一次轮换的新access")
			}
			if _, e = s.Refresh(context.Background(), results[i].RefreshToken); !errors.Is(e, identity.ErrInvalidToken) {
				t.Fatal("重放必须撤销新refresh")
			}
		} else if !errors.Is(e, identity.ErrInvalidToken) {
			t.Fatal(e)
		}
	}
	if success != 1 {
		t.Fatal("同一刷新令牌必须且仅能消费一次")
	}
}

func TestOnlineIdentityPGLogoutHistoricalTokenAndIsolation(t *testing.T) {
	p, s, game, password := onlineIdentityIntegration(t)
	old := loginOnlineIdentity(t, s, game, password)
	other := loginOnlineIdentity(t, s, game, password)
	fresh, err := s.Refresh(context.Background(), old.RefreshToken)
	if err != nil {
		t.Fatal(err)
	}
	// 新Service重新读取持久状态，不能依赖原实例内存缓存；不等价于进程/数据库重启验证。
	reopened, err := identity.NewPersistentService(NewOnlineIdentityRepository(p), identity.SystemClock{}, time.Minute, time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 2; i++ {
		if err = reopened.Logout(context.Background(), old.RefreshToken); err != nil {
			t.Fatal("历史凭据退出必须幂等")
		}
	}
	for _, token := range []string{old.AccessToken, fresh.AccessToken} {
		if _, err = reopened.Authenticate(context.Background(), token); !errors.Is(err, identity.ErrInvalidToken) {
			t.Fatal("退出后全部access必须失效")
		}
	}
	for _, token := range []string{old.RefreshToken, fresh.RefreshToken} {
		if _, err = reopened.Refresh(context.Background(), token); !errors.Is(err, identity.ErrInvalidToken) {
			t.Fatal("退出后全部refresh必须失效")
		}
	}
	if _, err = reopened.Authenticate(context.Background(), other.AccessToken); err != nil {
		t.Fatal("退出不得撤销另一个会话")
	}
}

func TestOnlineIdentityPGDisabledExpiryAndScope(t *testing.T) {
	p, s, game, password := onlineIdentityIntegration(t)
	session := loginOnlineIdentity(t, s, game, password)
	ctx := context.Background()
	if _, err := s.LoginPassword(ctx, game+"-other", "account-a", password, ""); !errors.Is(err, identity.ErrInvalidCredentials) {
		t.Fatal("账户不可跨游戏范围使用")
	}
	if _, err := p.inner.Exec(ctx, `UPDATE online_identity_sessions SET access_expires_at=now()-interval '1 second' WHERE session_id=$1`, session.SessionID); err != nil {
		t.Fatal("设置本次会话边界失败")
	}
	if _, err := s.Authenticate(ctx, session.AccessToken); !errors.Is(err, identity.ErrTokenExpired) {
		t.Fatal("到期access必须拒绝")
	}
	if _, err := p.inner.Exec(ctx, `UPDATE online_identity_accounts SET disabled=TRUE WHERE game_id=$1`, game); err != nil {
		t.Fatal("设置本次账户禁用失败")
	}
	if _, err := s.LoginPassword(ctx, game, "account-a", password, ""); !errors.Is(err, identity.ErrInvalidCredentials) {
		t.Fatal("禁用账户必须拒绝登录")
	}
	if _, err := s.Refresh(ctx, session.RefreshToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatal("禁用账户必须拒绝刷新")
	}
	if _, err := s.Authenticate(ctx, session.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatal("禁用账户必须拒绝认证")
	}
}

// 退出与刷新使用同一会话行锁，不论谁先得到锁，退出成功后的认证必须全部拒绝。
func TestOnlineIdentityPGLogoutRefreshRace(t *testing.T) {
	_, s, game, password := onlineIdentityIntegration(t)
	for i := 0; i < 8; i++ {
		old := loginOnlineIdentity(t, s, game, password)
		start := make(chan struct{})
		var wg sync.WaitGroup
		var fresh identity.Session
		var refreshErr, logoutErr error
		wg.Add(2)
		go func() {
			defer wg.Done()
			<-start
			fresh, refreshErr = s.Refresh(context.Background(), old.RefreshToken)
		}()
		go func() { defer wg.Done(); <-start; logoutErr = s.Logout(context.Background(), old.RefreshToken) }()
		close(start)
		wg.Wait()
		if logoutErr != nil {
			t.Fatal("退出事务不应失败")
		}
		if refreshErr != nil && !errors.Is(refreshErr, identity.ErrInvalidToken) {
			t.Fatal(refreshErr)
		}
		if _, err := s.Authenticate(context.Background(), old.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
			t.Fatal("旧access不得在已确认退出后恢复")
		}
		if refreshErr == nil {
			if _, err := s.Authenticate(context.Background(), fresh.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
				t.Fatal("并发轮换的新access亦必须撤销")
			}
		}
	}
}
