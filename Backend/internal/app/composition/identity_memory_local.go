//go:build !productiondeps

// 本地身份装配的账号配置、bcrypt播种与内存仓储；唯一调用方为开发RunIdentity及其测试。
// 仓储拥有本进程账号/会话摘要，随进程结束销毁；不访问玩家资料、不进入productiondeps。
package composition

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"strings"
	"sync"
	"time"

	"divinebeasts/backend/internal/modules/identity"

	"golang.org/x/crypto/bcrypt"
)

// localIdentityAccountSeed只用于本机开发身份装配；密码瞬时输入bcrypt，不进入日志或响应。
// 字段仅来自受控环境变量，不构成公开注册接口，productiondeps构建排除本文件。
type localIdentityAccountSeed struct {
	AccountName string `json:"accountName"` // 登录名1..128字节，无首尾空白，区分大小写。
	Password    string `json:"password"`    // 本地测试口令1..72字节；生产初始化门槛仍为8字节。
}

// parseLocalIdentityAccountSeeds先完整验证最多32个配置账号，再允许调用者播种。
// 保留旧单账号变量，可另加JSON数组；重复、未知字段、残余JSON和缺失口令均拒绝，
// 错误不包含输入内容，防止将口令带入启动日志；无账号配置时维持原游客开发模式。
func parseLocalIdentityAccountSeeds(accountName, password, accountsJSON string) ([]localIdentityAccountSeed, error) {
	const maxAccounts = 32
	seeds := make([]localIdentityAccountSeed, 0)
	accountName = strings.TrimSpace(accountName)
	if accountName != "" || password != "" {
		if accountName == "" || password == "" {
			return nil, fmt.Errorf("本地单账号名称与密码必须成对设置")
		}
		seeds = append(seeds, localIdentityAccountSeed{AccountName: accountName, Password: password})
	}
	if strings.TrimSpace(accountsJSON) != "" {
		if len(accountsJSON) > 32768 {
			return nil, fmt.Errorf("本地批量账号配置超过32KiB限制")
		}
		var batch []localIdentityAccountSeed
		decoder := json.NewDecoder(strings.NewReader(accountsJSON))
		decoder.DisallowUnknownFields()
		if err := decoder.Decode(&batch); err != nil || len(batch) == 0 {
			return nil, fmt.Errorf("本地批量账号必须是非空合法JSON数组")
		}
		var trailing any
		if err := decoder.Decode(&trailing); err != io.EOF {
			return nil, fmt.Errorf("本地批量账号配置存在多余JSON")
		}
		seeds = append(seeds, batch...)
	}
	if len(seeds) > maxAccounts {
		return nil, fmt.Errorf("本地开发账号最多32个")
	}
	seen := make(map[string]bool, len(seeds))
	for _, seed := range seeds {
		if seed.AccountName == "" || len(seed.AccountName) > 128 || strings.TrimSpace(seed.AccountName) != seed.AccountName || seed.Password == "" || len(seed.Password) > 72 || seen[seed.AccountName] {
			return nil, fmt.Errorf("本地开发账号名称、密码长度或唯一性校验失败")
		}
		seen[seed.AccountName] = true
	}
	return seeds, nil
}

// localIdentityPersistentRepository（本地身份持久仓储）只用于开发/集成联调。
// 它实现正式PersistentRepository契约：账户只保存bcrypt摘要，会话只保存Token摘要；
// productiondeps构建不会包含本文件。
type localIdentityPersistentRepository struct {
	mu sync.RWMutex

	accounts       map[string]identity.Account
	sessions       map[string]identity.StoredSession
	accessOwners   map[string]string
	refreshOwners  map[string]string
	revokedSession map[string]bool
}

func newLocalIdentityPersistentRepository() *localIdentityPersistentRepository {
	return &localIdentityPersistentRepository{
		accounts:       make(map[string]identity.Account),
		sessions:       make(map[string]identity.StoredSession),
		accessOwners:   make(map[string]string),
		refreshOwners:  make(map[string]string),
		revokedSession: make(map[string]bool),
	}
}

func localIdentityAccountKey(gameID, accountName string) string {
	return fmt.Sprintf("%d:%s%d:%s", len(gameID), gameID, len(accountName), accountName)
}

func cloneIdentityAccount(account identity.Account) identity.Account {
	account.PasswordHash = append([]byte(nil), account.PasswordHash...)
	return account
}

// seedLocalIdentityAccount（本地身份账号种子）允许开发联调使用短于生产初始化门槛的临时密码，
// 但仍执行bcrypt慢哈希且只存在于!productiondeps构建；正式LoginPassword校验逻辑不变。
func seedLocalIdentityAccount(
	ctx context.Context,
	repo *localIdentityPersistentRepository,
	gameID string,
	accountName string,
	password string,
) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if repo == nil || gameID == "" || accountName == "" || password == "" || len(password) > 72 {
		return identity.ErrInvalidInput
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), 12)
	if err != nil {
		return identity.ErrUnavailable
	}
	_, err = repo.EnsureAccount(ctx, identity.Account{
		GameID:       gameID,
		AccountName:  accountName,
		PlayerID:     newID("player-dev"),
		PasswordHash: hash,
	})
	return err
}

func (r *localIdentityPersistentRepository) Probe(ctx context.Context) error {
	return ctx.Err()
}

func (r *localIdentityPersistentRepository) EnsureAccount(
	ctx context.Context,
	account identity.Account,
) (identity.Account, error) {
	if err := ctx.Err(); err != nil {
		return identity.Account{}, err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	key := localIdentityAccountKey(account.GameID, account.AccountName)
	if existing, ok := r.accounts[key]; ok {
		return cloneIdentityAccount(existing), nil
	}

	r.accounts[key] = cloneIdentityAccount(account)
	return cloneIdentityAccount(account), nil
}

func (r *localIdentityPersistentRepository) FindAccount(
	ctx context.Context,
	gameID string,
	accountName string,
) (identity.Account, error) {
	if err := ctx.Err(); err != nil {
		return identity.Account{}, err
	}

	r.mu.RLock()
	defer r.mu.RUnlock()

	account, ok := r.accounts[localIdentityAccountKey(gameID, accountName)]
	if !ok {
		return identity.Account{}, identity.ErrInvalidCredentials
	}
	return cloneIdentityAccount(account), nil
}

func accountForPlayer(
	accounts map[string]identity.Account,
	gameID string,
	playerID string,
) (identity.Account, bool) {
	for _, account := range accounts {
		if account.GameID == gameID && account.PlayerID == playerID {
			return account, true
		}
	}
	return identity.Account{}, false
}

func (r *localIdentityPersistentRepository) CreateSession(
	ctx context.Context,
	record identity.StoredSession,
) error {
	if err := ctx.Err(); err != nil {
		return err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	account, ok := accountForPlayer(r.accounts, record.GameID, record.PlayerID)
	if !ok || account.Disabled {
		return identity.ErrInvalidCredentials
	}

	r.sessions[record.SessionID] = record
	r.accessOwners[record.AccessDigest] = record.SessionID
	r.refreshOwners[record.RefreshDigest] = record.SessionID
	delete(r.revokedSession, record.SessionID)
	return nil
}

func (r *localIdentityPersistentRepository) Authenticate(
	ctx context.Context,
	accessDigest string,
	now time.Time,
) (identity.StoredSession, error) {
	if err := ctx.Err(); err != nil {
		return identity.StoredSession{}, err
	}

	r.mu.RLock()
	defer r.mu.RUnlock()

	sessionID, ok := r.accessOwners[accessDigest]
	if !ok || r.revokedSession[sessionID] {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	record, ok := r.sessions[sessionID]
	if !ok {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if !now.Before(record.AccessExpiresAt) {
		return identity.StoredSession{}, identity.ErrTokenExpired
	}

	account, ok := accountForPlayer(r.accounts, record.GameID, record.PlayerID)
	if !ok || account.Disabled {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	return record, nil
}

func (r *localIdentityPersistentRepository) Rotate(
	ctx context.Context,
	refreshDigest string,
	rotation identity.TokenRotation,
) (identity.StoredSession, error) {
	if err := ctx.Err(); err != nil {
		return identity.StoredSession{}, err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	sessionID, ok := r.refreshOwners[refreshDigest]
	if !ok || r.revokedSession[sessionID] {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	record, ok := r.sessions[sessionID]
	if !ok {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if record.RefreshDigest != refreshDigest {
		// 旧Refresh摘要重放必须先撤销整个会话，再返回安全错误。
		r.revokedSession[sessionID] = true
		delete(r.accessOwners, record.AccessDigest)
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if !rotation.Now.Before(record.RefreshExpiresAt) {
		r.revokedSession[sessionID] = true
		delete(r.accessOwners, record.AccessDigest)
		return identity.StoredSession{}, identity.ErrTokenExpired
	}

	delete(r.accessOwners, record.AccessDigest)
	// 旧Refresh摘要保留在refreshOwners中，使重复Logout仍能定位同一Session。
	record.AccessDigest = rotation.AccessDigest
	record.RefreshDigest = rotation.RefreshDigest
	candidateAccessExpiry := rotation.Now.Add(rotation.AccessTTL)
	if candidateAccessExpiry.After(record.RefreshExpiresAt) {
		candidateAccessExpiry = record.RefreshExpiresAt
	}
	record.AccessExpiresAt = candidateAccessExpiry

	r.sessions[sessionID] = record
	r.accessOwners[record.AccessDigest] = sessionID
	r.refreshOwners[record.RefreshDigest] = sessionID
	return record, nil
}

func (r *localIdentityPersistentRepository) Logout(
	ctx context.Context,
	refreshDigest string,
) error {
	if err := ctx.Err(); err != nil {
		return err
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	sessionID, ok := r.refreshOwners[refreshDigest]
	if !ok {
		return identity.ErrInvalidToken
	}
	if r.revokedSession[sessionID] {
		return nil
	}

	record, ok := r.sessions[sessionID]
	if !ok {
		return identity.ErrInvalidToken
	}
	r.revokedSession[sessionID] = true
	delete(r.accessOwners, record.AccessDigest)
	return nil
}
