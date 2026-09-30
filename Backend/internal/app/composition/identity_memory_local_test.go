//go:build !productiondeps

// 开发身份装配回归：验证真实bcrypt登录、账号隔离、配置拒绝及刷新/退出生命周期。
// 所有账号和固定口令只属于测试内存仓储，测试不得输出凭据或依赖生产数据库。
package composition

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"testing"
	"time"

	"divinebeasts/backend/internal/modules/identity"
)

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
