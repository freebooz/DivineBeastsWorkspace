//go:build !productiondeps

// 开发身份装配回归：验证真实bcrypt登录、账号隔离、配置拒绝及刷新/退出生命周期。
// 所有账号和固定口令只属于测试内存仓储，测试不得输出凭据或依赖生产数据库。
package composition

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"sync"
	"testing"
	"time"

	"divinebeasts/backend/internal/modules/identity"
)

type localRotationWaitContext struct {
	context.Context
	checked chan struct{}
	once    sync.Once
}

func (c *localRotationWaitContext) Err() error {
	err := c.Context.Err()
	c.once.Do(func() { close(c.checked) })
	return err
}

// 在仓储边界保留真实账号/会话初始化；固定领域时间不能被系统日期替换。
func TestLocalIdentityRotationWaitHonorsDeadlineAndCancellation(t *testing.T) {
	for _, canceled := range []bool{false, true} {
		t.Run(fmt.Sprintf("canceled=%t", canceled), func(t *testing.T) {
			repo := newLocalIdentityPersistentRepository()
			now := time.Date(2020, 1, 1, 0, 0, 0, 0, time.UTC)
			lifetime := 30 * time.Millisecond
			if canceled {
				lifetime = time.Hour
			}
			if _, err := repo.EnsureAccount(context.Background(), identity.Account{GameID: "game", AccountName: "wait-account", PlayerID: "player"}); err != nil {
				t.Fatal(err)
			}
			original := identity.StoredSession{Session: identity.Session{GameID: "game", PlayerID: "player", SessionID: "wait-session", AccessExpiresAt: now.Add(lifetime), RefreshExpiresAt: now.Add(lifetime)}, AccessDigest: "old-access", RefreshDigest: "old-refresh"}
			if err := repo.CreateSession(context.Background(), original); err != nil {
				t.Fatal(err)
			}
			base, cancel := context.WithCancel(context.Background())
			defer cancel()
			ctx := &localRotationWaitContext{Context: base, checked: make(chan struct{})}
			n := identity.TokenRotation{Now: now, AccessTTL: time.Minute, AccessDigest: "new-access", RefreshDigest: "new-refresh"}
			repo.mu.Lock()
			result := make(chan error, 1)
			go func() { _, err := repo.Rotate(ctx, original.RefreshDigest, n); result <- err }()
			<-ctx.checked
			if canceled {
				cancel()
			} else {
				time.Sleep(60 * time.Millisecond)
			}
			repo.mu.Unlock()
			want := identity.ErrTokenExpired
			if canceled {
				want = context.Canceled
			}
			if err := <-result; !errors.Is(err, want) {
				t.Fatalf("等待后必须复核领域截止及取消: got=%v want=%v", err, want)
			}
			if _, err := repo.Authenticate(context.Background(), n.AccessDigest, now); err == nil {
				t.Fatal("失败不得签发后继Access")
			}
			if canceled {
				if _, err := repo.Authenticate(context.Background(), original.AccessDigest, now); err != nil {
					t.Fatalf("取消不能撤销原会话: %v", err)
				}
				if _, err := repo.Rotate(context.Background(), original.RefreshDigest, n); err != nil {
					t.Fatalf("取消不能消费原凭据: %v", err)
				}
			}
		})
	}
}

// 本地批量账号初始化回归：仅验证!productiondeps装配，测试口令不进入生产配置。
// 十个独立账号必须通过正式bcrypt密码登录，错误密码拒绝；配置错误须在播种前失败。
func TestLocalIdentityBatchAccounts(t *testing.T) {
	entries := make([]string, 0, 10)
	for number := 1; number <= 10; number++ {
		entries = append(entries, fmt.Sprintf(`{"accountName":"player%02d","password":"123456"}`, number))
	}
	seeds, err := parseLocalIdentityAccountSeeds("", "", "["+strings.Join(entries, ",")+"]")
	if err != nil || len(seeds) != 10 {
		t.Fatalf("本地配置应解析十个账号: count=%d err=%v", len(seeds), err)
	}
	ctx := context.Background()
	repo := newLocalIdentityPersistentRepository()
	for _, seed := range seeds {
		if err := seedLocalIdentityAccount(ctx, repo, "divine-beasts", seed.AccountName, seed.Password); err != nil {
			t.Fatal("本地账号播种失败")
		}
	}
	service, err := identity.NewPersistentService(repo, identity.SystemClock{}, time.Minute, time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	players := make(map[string]bool)
	for _, seed := range seeds {
		session, err := service.LoginPassword(ctx, "divine-beasts", seed.AccountName, seed.Password, "")
		if err != nil || session.PlayerID == "" || players[session.PlayerID] {
			t.Fatal("各账号应获得独立的可信玩家身份")
		}
		players[session.PlayerID] = true
		if _, err := service.LoginPassword(ctx, "divine-beasts", seed.AccountName, "wrong-password", ""); !errors.Is(err, identity.ErrInvalidCredentials) {
			t.Fatal("错误密码必须拒绝")
		}
	}
}

// 旧单账号环境变量继续有效；批量重复、空密码、未知字段或拼接JSON不得产生部分初始化。
func TestLocalIdentityAccountSeedConfiguration(t *testing.T) {
	legacy, err := parseLocalIdentityAccountSeeds("legacy", "legacy-password", "")
	if err != nil || len(legacy) != 1 || legacy[0].AccountName != "legacy" {
		t.Fatal("旧单账号配置应保持兼容")
	}
	for _, invalid := range []string{
		`[{"accountName":"duplicate","password":"local-password"},{"accountName":"duplicate","password":"local-password"}]`,
		`[{"accountName":"empty","password":""}]`,
		`[{"accountName":"unknown","password":"local-password","extra":true}]`,
		`[] {}`,
	} {
		if _, err := parseLocalIdentityAccountSeeds("", "", invalid); err == nil {
			t.Fatal("无效批量配置必须拒绝")
		}
	}
	if _, err := parseLocalIdentityAccountSeeds("legacy", "", ""); err == nil {
		t.Fatal("旧单账号密码缺失必须拒绝")
	}
}

func TestLocalIdentitySeedAllowsDevelopmentOnlyShortPassword(t *testing.T) {
	ctx := context.Background()
	repo := newLocalIdentityPersistentRepository()
	if err := seedLocalIdentityAccount(
		ctx,
		repo,
		"divine-beasts",
		"local-short-password-review",
		"short7",
	); err != nil {
		t.Fatal(err)
	}
	service, err := identity.NewPersistentService(
		repo,
		identity.SystemClock{},
		time.Minute,
		time.Hour,
	)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.LoginPassword(
		ctx,
		"divine-beasts",
		"local-short-password-review",
		"short7",
		"",
	); err != nil {
		t.Fatalf("本地开发短密码种子必须能通过正式LoginPassword验证: %v", err)
	}
}

func TestLocalIdentityPersistentRepositoryPasswordLifecycle(t *testing.T) {
	ctx := context.Background()
	repo := newLocalIdentityPersistentRepository()
	service, err := identity.NewPersistentService(
		repo,
		identity.SystemClock{},
		time.Minute,
		time.Hour,
	)
	if err != nil {
		t.Fatal(err)
	}

	if _, err := service.EnsureAccount(
		ctx,
		"divine-beasts",
		"local-review-account",
		"test-only-password",
	); err != nil {
		t.Fatal(err)
	}

	session, err := service.LoginPassword(
		ctx,
		"divine-beasts",
		"local-review-account",
		"test-only-password",
		"device-local-review",
	)
	if err != nil {
		t.Fatal(err)
	}
	if session.PlayerID == "" || session.AccessToken == "" || session.RefreshToken == "" {
		t.Fatal("密码登录必须返回可信玩家身份和一次性令牌")
	}

	if _, err := service.Authenticate(ctx, session.AccessToken); err != nil {
		t.Fatalf("Access Token认证失败: %v", err)
	}

	refreshed, err := service.Refresh(ctx, session.RefreshToken)
	if err != nil {
		t.Fatal(err)
	}
	if refreshed.AccessToken == session.AccessToken ||
		refreshed.RefreshToken == session.RefreshToken {
		t.Fatal("Refresh必须轮换Access和Refresh Token")
	}
	if _, err := service.Authenticate(ctx, session.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatalf("旧Access Token必须立即失效，实际: %v", err)
	}
	if _, err := service.Authenticate(ctx, refreshed.AccessToken); err != nil {
		t.Fatalf("新Access Token必须有效: %v", err)
	}

	if err := service.Logout(ctx, refreshed.RefreshToken); err != nil {
		t.Fatal(err)
	}
	if _, err := service.Authenticate(ctx, refreshed.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatalf("Logout后Access Token必须失效，实际: %v", err)
	}
}

// 同一账号后登录应替换前一会话；错误密码、其他账号及旧设备退出均不能伤及当前会话。
func TestLocalIdentityLaterLoginReplacesEarlierSession(t *testing.T) {
	ctx := context.Background()
	repo := newLocalIdentityPersistentRepository()
	service, err := identity.NewPersistentService(repo, identity.SystemClock{}, time.Minute, time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"alice", "bob"} {
		if _, err := service.EnsureAccount(ctx, "divine-beasts", name, "test-only-password"); err != nil {
			t.Fatal(err)
		}
	}
	first, err := service.LoginPassword(ctx, "divine-beasts", "alice", "test-only-password", "device-a")
	if err != nil {
		t.Fatal(err)
	}
	other, err := service.LoginPassword(ctx, "divine-beasts", "bob", "test-only-password", "device-c")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := service.LoginPassword(ctx, "divine-beasts", "alice", "wrong-password", "device-b"); !errors.Is(err, identity.ErrInvalidCredentials) {
		t.Fatal("错误密码必须拒绝")
	}
	if _, err := service.Authenticate(ctx, first.AccessToken); err != nil {
		t.Fatal("失败登录不得撤销旧会话")
	}
	second, err := service.LoginPassword(ctx, "divine-beasts", "alice", "test-only-password", "device-b")
	if err != nil {
		t.Fatal(err)
	}
	if second.SessionID == first.SessionID {
		t.Fatal("后登录必须取得新会话")
	}
	if _, err := service.Authenticate(ctx, first.AccessToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatal("前一Access必须失效")
	}
	if _, err := service.Refresh(ctx, first.RefreshToken); !errors.Is(err, identity.ErrInvalidToken) {
		t.Fatal("前一Refresh必须失效")
	}
	if _, err := service.Authenticate(ctx, second.AccessToken); err != nil {
		t.Fatal("后一Access必须有效")
	}
	if _, err := service.Authenticate(ctx, other.AccessToken); err != nil {
		t.Fatal("其他账号不得受影响")
	}
	if err := service.Logout(ctx, first.RefreshToken); err != nil {
		t.Fatal("旧设备重复退出应幂等")
	}
	if _, err := service.Authenticate(ctx, second.AccessToken); err != nil {
		t.Fatal("旧设备退出不得顶掉新会话")
	}
}
