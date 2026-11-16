// Package identity（身份领域）负责登录会话、Access Token和Refresh Token生命周期。
package identity

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"sync"
	"time"
)

// Clock（时钟接口）隔离系统时间，保证Token过期逻辑可测试。
type Clock interface {
	Now() time.Time
}

// TokenGenerator（Token生成器）负责创建不可预测的Token或ID。
type TokenGenerator interface {
	NewToken(prefix string) string
}

// Session（登录会话）表示一个玩家当前有效的业务登录会话。
type Session struct {
	GameID           string    // GameID（游戏ID）。
	PlayerID         string    // PlayerID（玩家ID）。
	SessionID        string    // SessionID（在线会话ID）。
	AccessToken      string    // AccessToken（访问令牌）。
	RefreshToken     string    // RefreshToken（刷新令牌）。
	AccessExpiresAt  time.Time // AccessExpiresAt（访问令牌过期时间）。
	RefreshExpiresAt time.Time // RefreshExpiresAt（刷新令牌过期时间）。
	DeviceID         string    // DeviceID（设备标识）。
}

// SessionRepository（会话仓储接口）隔离Redis等具体存储实现。
type SessionRepository interface {
	Save(ctx context.Context, session Session) error
	GetByRefreshToken(ctx context.Context, refreshToken string) (Session, error)
	GetByAccessToken(ctx context.Context, accessToken string) (Session, error)
	Revoke(ctx context.Context, sessionID string) error
}

// SessionRotationRepository（原子会话轮换仓储）是两种登录入口共用的摘要事务边界。
// Rotate在锁/事务内调用TokenRotation.Apply；已消费凭据重放须提交整会话撤销。
// Logout保留历史摘要归属，重复退出幂等。旧SessionRepository签名保持兼容。
type SessionRotationRepository interface {
	Rotate(context.Context, string, TokenRotation) (StoredSession, error)
	Logout(context.Context, string) error
}

// Service（身份领域服务）实现登录和Token轮换规则。
type Service struct {
	persistent PersistentRepository      // 生产密码路径仅使用摘要仓储，不允许回落旧游客仓储。
	rotation   SessionRotationRepository // 所有登录入口共用原子轮换合同。
	dummyHash  []byte                    // 不存在账户仍执行慢哈希比较，减少账户枚举时间差。
	repo       SessionRepository
	clock      Clock
	tokens     TokenGenerator
	accessTTL  time.Duration
	refreshTTL time.Duration
}

// NewService 保留旧开发/测试装配签名；repo还须实现原子SessionRotationRepository。
// 不支持原子轮换的第三方适配器明确不可用，不再回退Get/Save业务分支；生产密码认证使用NewPersistentService。
func NewService(repo SessionRepository, clock Clock, tokens TokenGenerator, accessTTL, refreshTTL time.Duration) *Service {
	rotation, _ := repo.(SessionRotationRepository)
	return &Service{repo: repo, rotation: rotation, clock: clock, tokens: tokens, accessTTL: accessTTL, refreshTTL: refreshTTL}
}

// LoginGuest（游客登录）创建游客玩家会话。
// 仅供旧开发/测试装配，持久密码服务明确拒绝；不执行资料初始化。
func (s *Service) LoginGuest(ctx context.Context, gameID, deviceID string) (Session, error) {
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	if s.persistent != nil {
		return Session{}, ErrInvalidCredentials
	}
	if s.rotation == nil {
		return Session{}, ErrUnavailable
	}
	if gameID == "" || deviceID == "" {
		return Session{}, errors.New("gameID和deviceID不能为空")
	}
	access, refresh, err := tokenPair()
	if err != nil {
		return Session{}, err
	}
	now := s.clock.Now()
	session := Session{
		GameID:           gameID,
		PlayerID:         s.tokens.NewToken("player"),
		SessionID:        s.tokens.NewToken("session"),
		AccessToken:      access,
		RefreshToken:     refresh,
		AccessExpiresAt:  now.Add(s.accessTTL),
		RefreshExpiresAt: now.Add(s.refreshTTL),
		DeviceID:         deviceID,
	}
	if err := s.repo.Save(ctx, session); err != nil {
		return Session{}, err
	}
	return session, nil
}

// Refresh（刷新令牌）执行Refresh Token Rotation（刷新令牌轮换）。
// 每次刷新都会立即使旧Refresh Token失效，降低Token泄露后的重放风险。
func (s *Service) Refresh(ctx context.Context, refreshToken string) (Session, error) {
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	if !validToken(refreshToken) {
		return Session{}, ErrInvalidToken
	}
	if s.rotation == nil {
		return Session{}, ErrUnavailable
	}
	access, refresh, err := tokenPair()
	if err != nil {
		return Session{}, err
	}
	record, err := s.rotation.Rotate(ctx, tokenDigest(refreshToken), TokenRotation{AccessDigest: tokenDigest(access), RefreshDigest: tokenDigest(refresh), Now: s.clock.Now().UTC(), AccessTTL: s.accessTTL})
	if err != nil {
		return Session{}, safeStorageError(err)
	}
	record.AccessToken, record.RefreshToken = access, refresh
	return record.Session, nil
}

// Authenticate（验证Access Token）解析可信玩家会话并拒绝不存在或已过期的访问令牌。
func (s *Service) Authenticate(ctx context.Context, accessToken string) (Session, error) {
	if s.persistent != nil {
		return s.authenticatePersistent(ctx, accessToken)
	}
	if accessToken == "" {
		return Session{}, errors.New("AUTH_SESSION_INVALID: Access Token不能为空")
	}
	session, err := s.repo.GetByAccessToken(ctx, accessToken)
	if err != nil {
		return Session{}, err
	}
	if !s.clock.Now().Before(session.AccessExpiresAt) {
		return Session{}, errors.New("AUTH_TOKEN_EXPIRED: Access Token已过期")
	}
	return session, nil
}

// SystemClock（系统时钟）用于生产环境读取UTC时间。
type SystemClock struct{}

// Now（当前时间）返回UTC当前时间。
func (SystemClock) Now() time.Time { return time.Now().UTC() }

// CryptoTokenGenerator（加密Token生成器）使用crypto/rand创建不可预测Token。
type CryptoTokenGenerator struct{}

// NewToken（生成Token）创建带业务前缀的随机Token。
func (CryptoTokenGenerator) NewToken(prefix string) string {
	var raw [24]byte
	if _, err := rand.Read(raw[:]); err != nil {
		panic(fmt.Sprintf("安全随机数生成失败: %v", err))
	}
	return prefix + "-" + hex.EncodeToString(raw[:])
}

// MemorySessionRepository（内存会话仓储）仅用于单元测试和本地开发。
// 不可用于生产密码登录；生产使用 PersistentRepository 的原子事务。
type MemorySessionRepository struct {
	mu             sync.RWMutex
	bySessionID    map[string]Session
	byRefreshToken map[string]string
	byAccessToken  map[string]string
	revoked        map[string]bool
}

// NewMemorySessionRepository（创建内存会话仓储）创建线程安全的本地仓储。
func NewMemorySessionRepository() *MemorySessionRepository {
	return &MemorySessionRepository{bySessionID: map[string]Session{}, byRefreshToken: map[string]string{}, byAccessToken: map[string]string{}, revoked: map[string]bool{}}
}

// Save（保存会话）保存会话并自动撤销同SessionID的旧Refresh Token。
func (r *MemorySessionRepository) Save(ctx context.Context, session Session) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	session.AccessToken, session.RefreshToken = tokenDigest(session.AccessToken), tokenDigest(session.RefreshToken)
	if old, ok := r.bySessionID[session.SessionID]; ok {
		delete(r.byAccessToken, old.AccessToken)
	}
	r.bySessionID[session.SessionID] = session
	r.byRefreshToken[session.RefreshToken] = session.SessionID
	r.byAccessToken[session.AccessToken] = session.SessionID
	delete(r.revoked, session.SessionID)
	return nil
}

// GetByRefreshToken（按刷新令牌查询）获取对应会话。
func (r *MemorySessionRepository) GetByRefreshToken(_ context.Context, refreshToken string) (Session, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	sessionID, ok := r.byRefreshToken[tokenDigest(refreshToken)]
	if !ok || r.revoked[sessionID] || r.bySessionID[sessionID].RefreshToken != tokenDigest(refreshToken) {
		return Session{}, ErrInvalidToken
	}
	return r.bySessionID[sessionID], nil
}

// GetByAccessToken（按访问令牌查询）获取当前有效会话的存储快照。
func (r *MemorySessionRepository) GetByAccessToken(_ context.Context, accessToken string) (Session, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	sessionID, ok := r.byAccessToken[tokenDigest(accessToken)]
	if !ok || r.revoked[sessionID] {
		return Session{}, ErrInvalidToken
	}
	return r.bySessionID[sessionID], nil
}

// Revoke（撤销会话）使Session和Refresh Token立即失效。
func (r *MemorySessionRepository) Revoke(_ context.Context, sessionID string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	if old, ok := r.bySessionID[sessionID]; ok {
		delete(r.byAccessToken, old.AccessToken)
		r.revoked[sessionID] = true
	}
	return nil
}

// Rotate（内存原子轮换）锁属于仓储，多个Service共享仓储时仍只能消费一次。
func (r *MemorySessionRepository) Rotate(ctx context.Context, digest string, n TokenRotation) (StoredSession, error) {
	started := time.Now()
	if err := ctx.Err(); err != nil {
		return StoredSession{}, err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if err := ctx.Err(); err != nil {
		return StoredSession{}, err
	}
	id, ok := r.byRefreshToken[digest]
	if !ok || r.revoked[id] {
		return StoredSession{}, ErrInvalidToken
	}
	old := r.bySessionID[id]
	record := StoredSession{Session: old, AccessDigest: old.AccessToken, RefreshDigest: old.RefreshToken}
	// 保留注入的领域时间，只追加本次锁等待的单调耗时，不能用系统日期替换领域时钟。
	n.Now = n.Now.Add(time.Since(started))
	fresh, err := n.Apply(record, digest)
	if err != nil {
		r.revoked[id] = true
		delete(r.byAccessToken, old.AccessToken)
		return StoredSession{}, err
	}
	delete(r.byAccessToken, old.AccessToken)
	stored := fresh.Session
	stored.AccessToken, stored.RefreshToken = fresh.AccessDigest, fresh.RefreshDigest
	r.bySessionID[id] = stored
	r.byAccessToken[fresh.AccessDigest] = id
	r.byRefreshToken[fresh.RefreshDigest] = id
	return fresh, nil
}

// Logout（按历史摘要幂等退出）不删除历史归属，防止轮换后旧凭据失去退出能力。
func (r *MemorySessionRepository) Logout(ctx context.Context, digest string) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	id, ok := r.byRefreshToken[digest]
	if !ok {
		return ErrInvalidToken
	}
	r.revoked[id] = true
	delete(r.byAccessToken, r.bySessionID[id].AccessToken)
	return nil
}
