package identity

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"strings"
	"testing"
	"time"
)

// passwordRepositoryDouble 只替代外部持久仓储；密码校验、随机签发、摘要和有效期使用真实领域代码。
type passwordRepositoryDouble struct {
	account Account
	record  StoredSession
	err     error
}

func (r *passwordRepositoryDouble) Probe(context.Context) error { return r.err }
func (r *passwordRepositoryDouble) EnsureAccount(_ context.Context, a Account) (Account, error) {
	if r.err != nil {
		return Account{}, r.err
	}
	if r.account.PlayerID == "" {
		r.account = a
	}
	return r.account, nil
}
func (r *passwordRepositoryDouble) FindAccount(context.Context, string, string) (Account, error) {
	if r.err != nil {
		return Account{}, r.err
	}
	if r.account.PlayerID == "" {
		return Account{}, ErrInvalidCredentials
	}
	return r.account, nil
}
func (r *passwordRepositoryDouble) CreateSession(_ context.Context, s StoredSession) error {
	r.record = s
	return r.err
}
func (r *passwordRepositoryDouble) Authenticate(_ context.Context, d string, now time.Time) (StoredSession, error) {
	if r.err != nil {
		return StoredSession{}, r.err
	}
	if d != r.record.AccessDigest || r.account.Disabled {
		return StoredSession{}, ErrInvalidToken
	}
	if !now.Before(r.record.AccessExpiresAt) {
		return StoredSession{}, ErrTokenExpired
	}
	return r.record, nil
}
func (r *passwordRepositoryDouble) Rotate(_ context.Context, d string, n TokenRotation) (StoredSession, error) {
	if r.err != nil {
		return StoredSession{}, r.err
	}
	if d != r.record.RefreshDigest {
		return StoredSession{}, ErrInvalidToken
	}
	r.record.AccessDigest = n.AccessDigest
	r.record.RefreshDigest = n.RefreshDigest
	r.record.AccessExpiresAt = n.Now.Add(n.AccessTTL)
	return r.record, nil
}
func (r *passwordRepositoryDouble) Logout(context.Context, string) error { return r.err }

func passwordFixture(t *testing.T) (*Service, *passwordRepositoryDouble, *fakeClock) {
	t.Helper()
	r := &passwordRepositoryDouble{}
	c := &fakeClock{now: time.Date(2026, 9, 21, 0, 0, 0, 0, time.UTC)}
	s, err := NewPersistentService(r, c, time.Minute, time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	return s, r, c
}

// 必须真实验证慢哈希，不能把任何非空密码当作认证成功；重复初始化不得覆盖已有口令。
func TestPasswordAccountAndLogin(t *testing.T) {
	s, r, _ := passwordFixture(t)
	ctx := context.Background()
	a, err := s.EnsureAccount(ctx, "game", "alice", "test-only-password")
	if err != nil {
		t.Fatal(err)
	}
	if a.PlayerID == "" || len(a.PasswordHash) != 0 {
		t.Fatal("账户公开结果必须有身份但不得泄漏密码哈希")
	}
	if strings.Contains(string(r.account.PasswordHash), "test-only-password") {
		t.Fatal("不得持久化明文密码")
	}
	a2, err := s.EnsureAccount(ctx, "game", "alice", "test-only-password")
	if err != nil || a2.PlayerID != a.PlayerID {
		t.Fatal("账户初始化必须幂等")
	}
	if _, err = s.EnsureAccount(ctx, "game", "alice", "another-password"); !errors.Is(err, ErrAccountConflict) {
		t.Fatal("不得覆盖已有凭据")
	}
	if _, err = s.LoginPassword(ctx, "game", "alice", "wrong-password", ""); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatal("错误密码必须拒绝")
	}
	got, err := s.LoginPassword(ctx, "game", "alice", "test-only-password", "device")
	if err != nil {
		t.Fatal(err)
	}
	if got.PlayerID != a.PlayerID || got.AccessToken == got.RefreshToken || len(got.AccessToken) < 43 {
		t.Fatal("签发身份或高熵令牌无效")
	}
	sum := sha256.Sum256([]byte(got.AccessToken))
	if r.record.AccessDigest != hex.EncodeToString(sum[:]) {
		t.Fatal("仓储应仅接收SHA256摘要")
	}
	if r.record.AccessToken != "" || r.record.RefreshToken != "" {
		t.Fatal("原始令牌不得传入仓储")
	}
	verified, err := s.Authenticate(ctx, got.AccessToken)
	if err != nil || verified.PlayerID != a.PlayerID || verified.RefreshToken != "" {
		t.Fatal("可信主体验证不得返回刷新凭据")
	}
	fresh, err := s.Refresh(ctx, got.RefreshToken)
	if err != nil || fresh.RefreshToken == got.RefreshToken || !fresh.RefreshExpiresAt.Equal(got.RefreshExpiresAt) {
		t.Fatal("轮换必须换凭据且不延长绝对期限")
	}
}

func TestPersistentRejectsGuestAndInvalidInput(t *testing.T) {
	s, _, _ := passwordFixture(t)
	ctx := context.Background()
	if _, err := s.LoginGuest(ctx, "game", "device"); err == nil {
		t.Fatal("生产路径不得回落guest")
	}
	for _, password := range []string{"", strings.Repeat("x", 73)} {
		if _, err := s.EnsureAccount(ctx, "game", "alice", password); !errors.Is(err, ErrInvalidInput) {
			t.Fatal("拒绝不满足bcrypt输入边界的密码")
		}
	}
	if _, err := NewPersistentService(nil, SystemClock{}, time.Minute, time.Hour); err == nil {
		t.Fatal("缺仓储不能构造生产服务")
	}
	if _, err := NewPersistentService(&passwordRepositoryDouble{}, SystemClock{}, time.Hour, time.Minute); err == nil {
		t.Fatal("访问期限不得长于刷新期限")
	}
	for _, token := range []string{"", "not-a-token", strings.Repeat("x", 1024)} {
		if _, err := s.Authenticate(ctx, token); !errors.Is(err, ErrInvalidToken) {
			t.Fatal("畸形access应拒绝")
		}
		if _, err := s.Refresh(ctx, token); !errors.Is(err, ErrInvalidToken) {
			t.Fatal("畸形refresh应拒绝")
		}
		if err := s.Logout(ctx, token); !errors.Is(err, ErrInvalidToken) {
			t.Fatal("畸形logout应拒绝")
		}
	}
}

func TestPersistentFailureAndExpiry(t *testing.T) {
	s, r, c := passwordFixture(t)
	ctx := context.Background()
	_, err := s.EnsureAccount(ctx, "game", "alice", "test-only-password")
	if err != nil {
		t.Fatal(err)
	}
	login, err := s.LoginPassword(ctx, "game", "alice", "test-only-password", "")
	if err != nil {
		t.Fatal(err)
	}
	c.now = c.now.Add(time.Minute)
	if _, err = s.Authenticate(ctx, login.AccessToken); !errors.Is(err, ErrTokenExpired) {
		t.Fatal("等于截止时间即过期")
	}
	r.account.Disabled = true
	if _, err = s.LoginPassword(ctx, "game", "alice", "test-only-password", ""); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatal("禁用账户应拒绝登录")
	}
	r.err = errors.New("secret database detail")
	if err = s.Probe(ctx); !errors.Is(err, ErrUnavailable) || strings.Contains(err.Error(), "secret") {
		t.Fatal("故障应真实失败且不泄漏存储细节")
	}
	if _, err = s.Refresh(ctx, login.RefreshToken); !errors.Is(err, ErrUnavailable) {
		t.Fatal("不可用不能被伪装成认证失败")
	}
	if err = s.Logout(ctx, login.RefreshToken); !errors.Is(err, ErrUnavailable) {
		t.Fatal("退出失败不能伪成功")
	}
	cancelled, cancel := context.WithCancel(ctx)
	cancel()
	if _, err = s.LoginPassword(cancelled, "game", "alice", "test-only-password", ""); !errors.Is(err, context.Canceled) {
		t.Fatal("取消请求应提前停止")
	}
}
