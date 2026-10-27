package identity

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// rotationWaitContext只观察真实仓储进入检查，不注入或替代轮换业务规则。
type rotationWaitContext struct {
	context.Context
	checked chan struct{}
	once    sync.Once
}

func (c *rotationWaitContext) Err() error {
	err := c.Context.Err()
	c.once.Do(func() { close(c.checked) })
	return err
}

// 固定领域时钟与系统当前日期无关；只有本次真实锁等待可以推进有效期。
func TestMemoryRotationWaitHonorsAbsoluteDeadline(t *testing.T) {
	repo := NewMemorySessionRepository()
	now := time.Date(2020, 1, 1, 0, 0, 0, 0, time.UTC)
	original := Session{SessionID: "wait-expiry", AccessToken: "old-access", RefreshToken: "old-refresh", AccessExpiresAt: now.Add(30 * time.Millisecond), RefreshExpiresAt: now.Add(30 * time.Millisecond)}
	if err := repo.Save(context.Background(), original); err != nil {
		t.Fatal(err)
	}
	n := TokenRotation{Now: now, AccessTTL: time.Minute, AccessDigest: tokenDigest("new-access"), RefreshDigest: tokenDigest("new-refresh")}
	ctx := &rotationWaitContext{Context: context.Background(), checked: make(chan struct{})}
	repo.mu.Lock()
	result := make(chan error, 1)
	go func() { _, err := repo.Rotate(ctx, tokenDigest(original.RefreshToken), n); result <- err }()
	<-ctx.checked
	time.Sleep(60 * time.Millisecond)
	repo.mu.Unlock()
	if err := <-result; !errors.Is(err, ErrTokenExpired) {
		t.Fatalf("锁等待跨绝对截止必须拒绝: %v", err)
	}
	if _, err := repo.GetByAccessToken(context.Background(), "new-access"); err == nil {
		t.Fatal("过期等待不得签发后继Access")
	}
}

func TestMemoryRotationCanceledWhileWaitingDoesNotConsume(t *testing.T) {
	repo := NewMemorySessionRepository()
	now := time.Date(2020, 1, 1, 0, 0, 0, 0, time.UTC)
	original := Session{SessionID: "wait-cancel", AccessToken: "old-access", RefreshToken: "old-refresh", AccessExpiresAt: now.Add(time.Hour), RefreshExpiresAt: now.Add(time.Hour)}
	if err := repo.Save(context.Background(), original); err != nil {
		t.Fatal(err)
	}
	base, cancel := context.WithCancel(context.Background())
	defer cancel()
	ctx := &rotationWaitContext{Context: base, checked: make(chan struct{})}
	n := TokenRotation{Now: now, AccessTTL: time.Minute, AccessDigest: tokenDigest("new-access"), RefreshDigest: tokenDigest("new-refresh")}
	repo.mu.Lock()
	result := make(chan error, 1)
	go func() { _, err := repo.Rotate(ctx, tokenDigest(original.RefreshToken), n); result <- err }()
	<-ctx.checked
	cancel()
	repo.mu.Unlock()
	if err := <-result; !errors.Is(err, context.Canceled) {
		t.Fatalf("锁等待中取消须拒绝: %v", err)
	}
	if _, err := repo.GetByRefreshToken(context.Background(), original.RefreshToken); err != nil {
		t.Fatalf("取消不得消费或撤销原凭据: %v", err)
	}
	if _, err := repo.Rotate(context.Background(), tokenDigest(original.RefreshToken), n); err != nil {
		t.Fatalf("固定领域时钟仍应有效: %v", err)
	}
}

// 游客入口与密码入口必须遵循同一绝对截止与重放撤销合同。
func TestGuestRefreshKeepsAbsoluteDeadline(t *testing.T) {
	ctx := context.Background()
	c := &fakeClock{now: time.Date(2026, 10, 27, 0, 0, 0, 0, time.UTC)}
	s := NewService(NewMemorySessionRepository(), c, &fakeTokens{}, time.Minute, time.Hour)
	old, err := s.LoginGuest(ctx, "game", "device")
	if err != nil {
		t.Fatal(err)
	}
	c.now = c.now.Add(time.Hour - 30*time.Second)
	fresh, err := s.Refresh(ctx, old.RefreshToken)
	if err != nil {
		t.Fatal(err)
	}
	if !fresh.RefreshExpiresAt.Equal(old.RefreshExpiresAt) || !fresh.AccessExpiresAt.Equal(old.RefreshExpiresAt) {
		t.Fatal("刷新不得延长登录时绝对截止，Access必须封顶")
	}
	c.now = old.RefreshExpiresAt
	if _, err := s.Refresh(ctx, fresh.RefreshToken); !errors.Is(err, ErrTokenExpired) {
		t.Fatalf("截止时必须拒绝: %v", err)
	}
}

func TestGuestRefreshReplayRevokesSuccessor(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemorySessionRepository(), SystemClock{}, &fakeTokens{}, time.Minute, time.Hour)
	old, _ := s.LoginGuest(ctx, "game", "device")
	fresh, err := s.Refresh(ctx, old.RefreshToken)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = s.Refresh(ctx, old.RefreshToken); !errors.Is(err, ErrInvalidToken) {
		t.Fatalf("重放必须分类拒绝: %v", err)
	}
	if _, err = s.Authenticate(ctx, fresh.AccessToken); err == nil {
		t.Fatal("重放必须撤销后继Access")
	}
	if err = s.Logout(ctx, old.RefreshToken); err != nil {
		t.Fatalf("历史凭据退出应幂等: %v", err)
	}
	if err = s.Logout(ctx, old.RefreshToken); err != nil {
		t.Fatal(err)
	}
}

func TestGuestRefreshConcurrentAcrossServicesHasOneWinner(t *testing.T) {
	ctx := context.Background()
	repo := NewMemorySessionRepository()
	first := NewService(repo, SystemClock{}, &fakeTokens{}, time.Minute, time.Hour)
	second := NewService(repo, SystemClock{}, &fakeTokens{}, time.Minute, time.Hour)
	old, _ := first.LoginGuest(ctx, "game", "device")
	var successes atomic.Int32
	var wg sync.WaitGroup
	for i := 0; i < 16; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			s := first
			if i%2 != 0 {
				s = second
			}
			if _, err := s.Refresh(ctx, old.RefreshToken); err == nil {
				successes.Add(1)
			}
		}(i)
	}
	wg.Wait()
	if successes.Load() != 1 {
		t.Fatalf("原子轮换只能成功一次，实际%d", successes.Load())
	}
}

func TestGuestRefreshCancellationDoesNotConsumeCredential(t *testing.T) {
	s := NewService(NewMemorySessionRepository(), SystemClock{}, &fakeTokens{}, time.Minute, time.Hour)
	old, _ := s.LoginGuest(context.Background(), "game", "device")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := s.Refresh(ctx, old.RefreshToken); !errors.Is(err, context.Canceled) {
		t.Fatalf("应保留取消: %v", err)
	}
	if _, err := s.Refresh(context.Background(), old.RefreshToken); err != nil {
		t.Fatalf("取消不能消费凭据: %v", err)
	}
}

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
