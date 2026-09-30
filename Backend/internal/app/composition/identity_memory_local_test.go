//go:build !productiondeps

package composition

import (
	"context"
	"errors"
	"testing"
	"time"

	"divinebeasts/backend/internal/modules/identity"
)

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
