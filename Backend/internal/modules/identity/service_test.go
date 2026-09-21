package identity

import (
	"context"
	"testing"
	"time"
)

type fakeClock struct{ now time.Time }

func (f fakeClock) Now() time.Time { return f.now }

type fakeTokens struct{ n int }

func (f *fakeTokens) NewToken(prefix string) string {
	f.n++
	return prefix + "-token-" + string(rune('0'+f.n))
}

func TestGuestLoginCreatesSession(t *testing.T) {
	repo := NewMemorySessionRepository()
	clock := fakeClock{now: time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)}
	tokens := &fakeTokens{}
	service := NewService(repo, clock, tokens, 15*time.Minute, 30*24*time.Hour)

	session, err := service.LoginGuest(context.Background(), "divine-beasts", "device-a")
	if err != nil {
		t.Fatalf("游客登录失败: %v", err)
	}
	if session.PlayerID == "" || session.AccessToken == "" || session.RefreshToken == "" {
		t.Fatalf("登录会话字段不完整: %+v", session)
	}
}

func TestRefreshRotatesTokensAndInvalidatesOldRefreshToken(t *testing.T) {
	repo := NewMemorySessionRepository()
	clock := fakeClock{now: time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)}
	tokens := &fakeTokens{}
	service := NewService(repo, clock, tokens, 15*time.Minute, 30*24*time.Hour)
	old, _ := service.LoginGuest(context.Background(), "divine-beasts", "device-a")

	fresh, err := service.Refresh(context.Background(), old.RefreshToken)
	if err != nil {
		t.Fatalf("刷新Token失败: %v", err)
	}
	if fresh.RefreshToken == old.RefreshToken {
		t.Fatal("Refresh Token必须轮换")
	}
	if _, err := service.Refresh(context.Background(), old.RefreshToken); err == nil {
		t.Fatal("旧Refresh Token必须失效")
	}
}

// TestAuthenticateAccessToken（Access Token认证测试）验证Gateway可以通过Token解析可信PlayerID和SessionID，并拒绝过期Token。
func TestAuthenticateAccessToken(t *testing.T) {
	repo := NewMemorySessionRepository()
	now := time.Date(2026, 9, 17, 4, 0, 0, 0, time.UTC)
	clock := &fakeClock{now: now}
	tokens := &fakeTokens{}
	service := NewService(repo, clock, tokens, 15*time.Minute, 30*24*time.Hour)
	session, err := service.LoginGuest(context.Background(), "divine-beasts", "device-a")
	if err != nil {
		t.Fatal(err)
	}

	authenticated, err := service.Authenticate(context.Background(), session.AccessToken)
	if err != nil {
		t.Fatalf("Access Token认证失败: %v", err)
	}
	if authenticated.PlayerID != session.PlayerID || authenticated.SessionID != session.SessionID {
		t.Fatalf("认证会话错误: %+v", authenticated)
	}

	clock.now = now.Add(16 * time.Minute)
	if _, err := service.Authenticate(context.Background(), session.AccessToken); err == nil {
		t.Fatal("过期Access Token必须被拒绝")
	}
}
