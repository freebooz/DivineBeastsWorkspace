package identity

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"errors"
	"strings"
	"time"
	"unicode/utf8"

	"golang.org/x/crypto/bcrypt"
)

// 对外安全错误不包含账户是否存在、密码、令牌或数据库详情；适配器以 errors.Is 分类。
var (
	ErrInvalidCredentials = errors.New("AUTH_INVALID_CREDENTIALS: 认证失败")     // 错误密码、不存在或禁用账户。
	ErrInvalidToken       = errors.New("AUTH_SESSION_INVALID: 凭据无效")         // 未知、重放或撤销凭据。
	ErrTokenExpired       = errors.New("AUTH_TOKEN_EXPIRED: 凭据已过期")          // 到达截止时刻即过期。
	ErrUnavailable        = errors.New("IDENTITY_UNAVAILABLE: 身份存储不可用")      // 不冒充认证错误。
	ErrInvalidInput       = errors.New("INVALID_ARGUMENT: 身份输入无效")           // 长度/格式/构造配置错误。
	ErrAccountConflict    = errors.New("IDENTITY_ACCOUNT_CONFLICT: 账户初始化冲突") // 不覆盖既有凭据。
)

// Account 是身份领域账户，不是玩家资料；PasswordHash 仅用于受信任仓储边界，禁止序列化和日志。
// EnsureAccount 返回副本时清空哈希；游戏内名字等资料只由 PlayerData 维护。
type Account struct {
	GameID       string // 游戏授权范围。
	AccountName  string // 区分大小写、无首尾空白的登录名（1..128字节）。
	PlayerID     string // 服务器生成的稳定玩家身份，不由客户端指定。
	PasswordHash []byte `json:"-"` // bcrypt 慢哈希，不含明文。
	Disabled     bool   // 禁用后登录、访问和刷新均拒绝。
}

// StoredSession 只携带摘要；嵌入 Session 的原始令牌字段必须始终为空。
type StoredSession struct {
	Session
	AccessDigest  string // 256位令牌的SHA256小写十六进制摘要。
	RefreshDigest string // 当前刷新摘要；消费历史由仓储事务保存。
}

// TokenRotation 是一次刷新事务的候选值；绝对 RefreshExpiresAt 不被延长。
type TokenRotation struct {
	AccessDigest  string        // 新访问凭据摘要。
	RefreshDigest string        // 新刷新凭据摘要。
	Now           time.Time     // 本次判断的UTC时间，由领域时钟提供。
	AccessTTL     time.Duration // 新访问期限，上限为会话刷新截止时间。
}

// PersistentRepository 是账户/认证会话的唯一持久化边界；实现必须并发安全且尊重ctx取消。
// 所有方法仅接收摘要，不存原始token，不写 player_profiles。
type PersistentRepository interface {
	Probe(context.Context) error                                            // 真实检查所需表可读，缺迁移必须失败。
	EnsureAccount(context.Context, Account) (Account, error)                // 唯一键冲突返回原账户，不更新已有哈希。
	FindAccount(context.Context, string, string) (Account, error)           // gameID+accountName查找。
	CreateSession(context.Context, StoredSession) error                     // 同事务复核账户未禁用后创建。
	Authenticate(context.Context, string, time.Time) (StoredSession, error) // 摘要、账户、撤销、截止全部检查。
	Rotate(context.Context, string, TokenRotation) (StoredSession, error)   // 行锁+历史消费+轮换原子；重放撤销需先提交再返回错误。
	Logout(context.Context, string) error                                   // 当前或历史刷新摘要关联会话；重复同凭据幂等，未知凭据拒绝。
}

// NewPersistentService 构造真实密码服务；显式依赖不可缺，0<accessTTL<=refreshTTL。
// 不探测或迁移数据库；装配方使用 Probe 判断业务就绪。共享实例支持并发调用，时钟须并发安全。
func NewPersistentService(repo PersistentRepository, clock Clock, accessTTL, refreshTTL time.Duration) (*Service, error) {
	if repo == nil || clock == nil || accessTTL <= 0 || refreshTTL < accessTTL {
		return nil, ErrInvalidInput
	}
	// 随机占位口令只用于等成本拒绝未知账户，不是任何可登录的测试账户。
	secret, err := opaqueToken()
	if err != nil {
		return nil, err
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(secret), 12)
	if err != nil {
		return nil, ErrUnavailable
	}
	return &Service{persistent: repo, clock: clock, accessTTL: accessTTL, refreshTTL: refreshTTL, dummyHash: hash}, nil
}

// EnsureAccount 仅供受控初始化工具调用；相同游戏/账户/密码幂等，不同密码或禁用账户返回冲突。
// 密码8..72字节且不裁剪；不写资料，不改旧密码，不恢复禁用账户，不返回密码哈希。
func (s *Service) EnsureAccount(ctx context.Context, gameID, accountName, password string) (Account, error) {
	if err := ctx.Err(); err != nil {
		return Account{}, err
	}
	if s.persistent == nil {
		return Account{}, ErrUnavailable
	}
	if !validAccountKey(gameID, accountName) || len(password) < 8 || len(password) > 72 {
		return Account{}, ErrInvalidInput
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), 12)
	if err != nil {
		return Account{}, ErrUnavailable
	}
	id, err := opaqueToken()
	if err != nil {
		return Account{}, err
	}
	a, err := s.persistent.EnsureAccount(ctx, Account{GameID: gameID, AccountName: accountName, PlayerID: "player-" + id, PasswordHash: hash})
	if err != nil {
		return Account{}, safeStorageError(err)
	}
	if a.Disabled || bcrypt.CompareHashAndPassword(a.PasswordHash, []byte(password)) != nil {
		return Account{}, ErrAccountConflict
	}
	a.PasswordHash = nil
	return a, nil
}

// Probe 真实透传仓储就绪；旧开发服务没有真实持久依赖，故返回不可用而非伪就绪。
func (s *Service) Probe(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if s.persistent == nil {
		return ErrUnavailable
	}
	return safeStorageError(s.persistent.Probe(ctx))
}

// LoginPassword 验证游戏范围账户，签发一次性返回的高熵随机凭据；deviceID可空且最多256字节。
// 持久仓储只收到摘要；写入失败不返回成功会话。公开会话含秘密，仅交给受控传输，不得记录。
func (s *Service) LoginPassword(ctx context.Context, gameID, accountName, password, deviceID string) (Session, error) {
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	if s.persistent == nil {
		return Session{}, ErrUnavailable
	}
	if !validAccountKey(gameID, accountName) || len(password) == 0 || len(password) > 72 || len(deviceID) > 256 || !utf8.ValidString(deviceID) {
		return Session{}, ErrInvalidInput
	}
	a, err := s.persistent.FindAccount(ctx, gameID, accountName)
	hash := a.PasswordHash
	if errors.Is(err, ErrInvalidCredentials) {
		hash = s.dummyHash
	} else if err != nil {
		return Session{}, safeStorageError(err)
	}
	matched := bcrypt.CompareHashAndPassword(hash, []byte(password)) == nil
	if !matched || err != nil || a.Disabled {
		return Session{}, ErrInvalidCredentials
	}
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	access, refresh, err := tokenPair()
	if err != nil {
		return Session{}, err
	}
	id, err := opaqueToken()
	if err != nil {
		return Session{}, err
	}
	now := s.clock.Now().UTC()
	record := StoredSession{Session: Session{GameID: a.GameID, PlayerID: a.PlayerID, SessionID: "session-" + id, DeviceID: deviceID, AccessExpiresAt: now.Add(s.accessTTL), RefreshExpiresAt: now.Add(s.refreshTTL)}, AccessDigest: tokenDigest(access), RefreshDigest: tokenDigest(refresh)}
	if err := s.persistent.CreateSession(ctx, record); err != nil {
		return Session{}, safeStorageError(err)
	}
	record.AccessToken = access
	record.RefreshToken = refresh
	return record.Session, nil
}

func (s *Service) refreshPersistent(ctx context.Context, token string) (Session, error) {
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	if !validToken(token) {
		return Session{}, ErrInvalidToken
	}
	access, refresh, err := tokenPair()
	if err != nil {
		return Session{}, err
	}
	record, err := s.persistent.Rotate(ctx, tokenDigest(token), TokenRotation{AccessDigest: tokenDigest(access), RefreshDigest: tokenDigest(refresh), Now: s.clock.Now().UTC(), AccessTTL: s.accessTTL})
	if err != nil {
		return Session{}, safeStorageError(err)
	}
	record.AccessToken = access
	record.RefreshToken = refresh
	return record.Session, nil
}

func (s *Service) authenticatePersistent(ctx context.Context, token string) (Session, error) {
	if err := ctx.Err(); err != nil {
		return Session{}, err
	}
	if !validToken(token) {
		return Session{}, ErrInvalidToken
	}
	record, err := s.persistent.Authenticate(ctx, tokenDigest(token), s.clock.Now().UTC())
	if err != nil {
		return Session{}, safeStorageError(err)
	}
	record.AccessToken = ""
	record.RefreshToken = ""
	return record.Session, nil
}

// Logout 通过当前或已消费刷新凭据撤销整个会话，数据库提交后才成功。
// 生产仓储须保留历史摘要以支持重复退出；旧开发路径只支持当前刷新凭据，不宣称同等持久保证。
func (s *Service) Logout(ctx context.Context, refreshToken string) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if s.persistent != nil {
		if !validToken(refreshToken) {
			return ErrInvalidToken
		}
		return safeStorageError(s.persistent.Logout(ctx, tokenDigest(refreshToken)))
	}
	if refreshToken == "" {
		return ErrInvalidToken
	}
	session, err := s.repo.GetByRefreshToken(ctx, refreshToken)
	if err != nil {
		return ErrInvalidToken
	}
	return s.repo.Revoke(ctx, session.SessionID)
}

func validAccountKey(gameID, accountName string) bool {
	for _, v := range []string{gameID, accountName} {
		if len(v) == 0 || len(v) > 128 || !utf8.ValidString(v) || strings.TrimSpace(v) != v || strings.ContainsAny(v, "\x00\r\n\t") {
			return false
		}
	}
	return true
}
func opaqueToken() (string, error) {
	var b [32]byte
	if _, err := rand.Read(b[:]); err != nil {
		return "", ErrUnavailable
	}
	return base64.RawURLEncoding.EncodeToString(b[:]), nil
}
func tokenPair() (string, string, error) {
	a, e := opaqueToken()
	if e != nil {
		return "", "", e
	}
	b, e := opaqueToken()
	return a, b, e
}
func validToken(s string) bool {
	if len(s) != 43 {
		return false
	}
	b, e := base64.RawURLEncoding.Strict().DecodeString(s)
	return e == nil && len(b) == 32
}
func tokenDigest(s string) string { sum := sha256.Sum256([]byte(s)); return hex.EncodeToString(sum[:]) }
func safeStorageError(err error) error {
	if err == nil {
		return nil
	}
	for _, known := range []error{context.Canceled, context.DeadlineExceeded, ErrInvalidCredentials, ErrInvalidToken, ErrTokenExpired, ErrInvalidInput, ErrAccountConflict} {
		if errors.Is(err, known) {
			return known
		}
	}
	return ErrUnavailable
}
