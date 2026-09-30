//go:build !productiondeps

package composition

import (
	"context"
	"fmt"
	"sync"
	"time"

	"divinebeasts/backend/internal/modules/identity"

	"golang.org/x/crypto/bcrypt"
)

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
