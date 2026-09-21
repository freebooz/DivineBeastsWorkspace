//go:build productiondeps

package postgres

import (
	"context"
	"errors"
	"time"

	"divinebeasts/backend/internal/modules/identity"
	"github.com/jackc/pgx/v5"
)

// onlineIdentityDatabase 仅隔离驱动以便事务故障注入；生产绑定现有Pool，不另开连接池。
type onlineIdentityDatabase interface {
	BeginTx(context.Context, pgx.TxOptions) (pgx.Tx, error)
	QueryRow(context.Context, string, ...any) pgx.Row
}

// OnlineIdentityRepository 持久化真实密码账户与摘要会话；实例无可变认证缓存，可跨请求并发使用。
// 所有认证读取都检查账户/撤销/期限；与 PlayerData 没有反向依赖。
type OnlineIdentityRepository struct{ db onlineIdentityDatabase }

var _ identity.PersistentRepository = (*OnlineIdentityRepository)(nil)

// NewOnlineIdentityRepository 绑定已有生产Pool；不连接、迁移或创建测试数据，nil配置由Probe失败报告。
func NewOnlineIdentityRepository(pool *Pool) *OnlineIdentityRepository {
	r := &OnlineIdentityRepository{}
	if pool != nil && pool.inner != nil {
		r.db = pool.inner
	}
	return r
}

// Probe 检查三张身份表以及实际读权限；仅Ping数据库不能证明迁移已就绪。
func (r *OnlineIdentityRepository) Probe(ctx context.Context) error {
	if r.db == nil {
		return identity.ErrUnavailable
	}
	var ready bool
	err := r.db.QueryRow(ctx, `SELECT EXISTS (SELECT 1 FROM online_identity_accounts LIMIT 1)
 OR EXISTS (SELECT 1 FROM online_identity_sessions LIMIT 1)
 OR EXISTS (SELECT 1 FROM online_identity_refresh_credentials LIMIT 1)`).Scan(&ready)
	return identityStorageError(err)
}

// EnsureAccount 利用唯一键串行初始化；冲突返回原记录，不覆盖密码/禁用状态。
// 不以无意义UPDATE实现UPSERT，避免触发未来审计或更新时间副作用。
func (r *OnlineIdentityRepository) EnsureAccount(ctx context.Context, a identity.Account) (identity.Account, error) {
	if r.db == nil {
		return identity.Account{}, identity.ErrUnavailable
	}
	tx, err := r.db.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return identity.Account{}, identityStorageError(err)
	}
	defer tx.Rollback(ctx)
	_, err = tx.Exec(ctx, `INSERT INTO online_identity_accounts (game_id,account_name,player_id,password_hash)
 VALUES ($1,$2,$3,$4) ON CONFLICT (game_id,account_name) DO NOTHING`, a.GameID, a.AccountName, a.PlayerID, a.PasswordHash)
	if err != nil {
		return identity.Account{}, identityStorageError(err)
	}
	var saved identity.Account
	err = tx.QueryRow(ctx, `SELECT game_id,account_name,player_id,password_hash,disabled FROM online_identity_accounts
 WHERE game_id=$1 AND account_name=$2`, a.GameID, a.AccountName).Scan(&saved.GameID, &saved.AccountName, &saved.PlayerID, &saved.PasswordHash, &saved.Disabled)
	if err != nil {
		return identity.Account{}, identityStorageError(err)
	}
	if err = tx.Commit(ctx); err != nil {
		return identity.Account{}, identityStorageError(err)
	}
	return saved, nil
}

// FindAccount 仅供密码验证；未知账户返回统一凭据错误，不向公网暴露存储诊断。
func (r *OnlineIdentityRepository) FindAccount(ctx context.Context, gameID, accountName string) (identity.Account, error) {
	if r.db == nil {
		return identity.Account{}, identity.ErrUnavailable
	}
	var a identity.Account
	err := r.db.QueryRow(ctx, `SELECT game_id,account_name,player_id,password_hash,disabled FROM online_identity_accounts
 WHERE game_id=$1 AND account_name=$2`, gameID, accountName).Scan(&a.GameID, &a.AccountName, &a.PlayerID, &a.PasswordHash, &a.Disabled)
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.Account{}, identity.ErrInvalidCredentials
	}
	if err != nil {
		return identity.Account{}, identityStorageError(err)
	}
	return a, nil
}

// CreateSession 创建会话和初始刷新摘要为一个事务；账户共享行锁阻止与禁用操作交错后误签发。
func (r *OnlineIdentityRepository) CreateSession(ctx context.Context, s identity.StoredSession) error {
	if r.db == nil {
		return identity.ErrUnavailable
	}
	if s.AccessToken != "" || s.RefreshToken != "" {
		return identity.ErrInvalidInput
	}
	tx, err := r.db.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return identityStorageError(err)
	}
	defer tx.Rollback(ctx)
	var disabled bool
	err = tx.QueryRow(ctx, `SELECT disabled FROM online_identity_accounts WHERE game_id=$1 AND player_id=$2 FOR SHARE`, s.GameID, s.PlayerID).Scan(&disabled)
	if errors.Is(err, pgx.ErrNoRows) || err == nil && disabled {
		return identity.ErrInvalidCredentials
	}
	if err != nil {
		return identityStorageError(err)
	}
	_, err = tx.Exec(ctx, `INSERT INTO online_identity_sessions
 (session_id,game_id,player_id,device_id,access_digest,refresh_digest,access_expires_at,refresh_expires_at)
 VALUES ($1,$2,$3,$4,$5,$6,$7,$8)`, s.SessionID, s.GameID, s.PlayerID, s.DeviceID, s.AccessDigest, s.RefreshDigest, s.AccessExpiresAt, s.RefreshExpiresAt)
	if err != nil {
		return identityStorageError(err)
	}
	_, err = tx.Exec(ctx, `INSERT INTO online_identity_refresh_credentials (digest,session_id) VALUES ($1,$2)`, s.RefreshDigest, s.SessionID)
	if err != nil {
		return identityStorageError(err)
	}
	return identityStorageError(tx.Commit(ctx))
}

const onlineIdentitySessionColumns = `s.game_id,s.player_id,s.session_id,s.device_id,s.access_expires_at,s.refresh_expires_at,
 s.access_digest,s.refresh_digest,s.revoked,a.disabled`

func scanOnlineIdentitySession(row pgx.Row) (identity.StoredSession, bool, bool, error) {
	var s identity.StoredSession
	var revoked, disabled bool
	err := row.Scan(&s.GameID, &s.PlayerID, &s.SessionID, &s.DeviceID, &s.AccessExpiresAt, &s.RefreshExpiresAt, &s.AccessDigest, &s.RefreshDigest, &revoked, &disabled)
	return s, revoked, disabled, err
}

// Authenticate 每次从持久状态解析主体；撤销提交后开始的认证绝不接受旧access。
// 并发撤销前已开始的读取仍以数据库语句快照为准，不声称能追回已经授权的业务操作。
func (r *OnlineIdentityRepository) Authenticate(ctx context.Context, digest string, now time.Time) (identity.StoredSession, error) {
	started := time.Now()
	if r.db == nil {
		return identity.StoredSession{}, identity.ErrUnavailable
	}
	s, revoked, disabled, err := scanOnlineIdentitySession(r.db.QueryRow(ctx, `SELECT `+onlineIdentitySessionColumns+`
 FROM online_identity_sessions s JOIN online_identity_accounts a ON a.game_id=s.game_id AND a.player_id=s.player_id
 WHERE s.access_digest=$1`, digest))
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if revoked || disabled {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	// 将连接池/数据库等待消耗的单调时间计入期限，不因排队继续接受已经到期的凭据。
	now = now.Add(time.Since(started))
	if !now.Before(s.AccessExpiresAt) || !now.Before(s.RefreshExpiresAt) {
		return identity.StoredSession{}, identity.ErrTokenExpired
	}
	return s, nil
}

// lockRefreshSession 先由不可变刷新历史定位，再锁定会话；所有刷新/退出使用相同锁顺序。
// 在锁后另读consumed状态，避免两个并发事务都凭锁前快照轮换成功。
func lockRefreshSession(ctx context.Context, tx pgx.Tx, digest string) (identity.StoredSession, bool, bool, error) {
	return scanOnlineIdentitySession(tx.QueryRow(ctx, `SELECT `+onlineIdentitySessionColumns+`
 FROM online_identity_sessions s JOIN online_identity_accounts a ON a.game_id=s.game_id AND a.player_id=s.player_id
 WHERE s.session_id=(SELECT session_id FROM online_identity_refresh_credentials WHERE digest=$1)
 FOR UPDATE OF s FOR SHARE OF a`, digest))
}

// Rotate 将消费旧摘要、更新令牌、保存新摘要放入同一事务。重放撤销先提交再返回错误。
// 网络在提交后断开存在结果不确定性，调用方不得盲目重试同一刷新令牌。
func (r *OnlineIdentityRepository) Rotate(ctx context.Context, digest string, n identity.TokenRotation) (identity.StoredSession, error) {
	started := time.Now()
	if r.db == nil {
		return identity.StoredSession{}, identity.ErrUnavailable
	}
	tx, err := r.db.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	defer tx.Rollback(ctx)
	s, revoked, disabled, err := lockRefreshSession(ctx, tx, digest)
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if revoked || disabled {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	var consumed bool
	if err = tx.QueryRow(ctx, `SELECT consumed_at IS NOT NULL FROM online_identity_refresh_credentials WHERE digest=$1`, digest).Scan(&consumed); err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if consumed || s.RefreshDigest != digest {
		if _, err = tx.Exec(ctx, `UPDATE online_identity_sessions SET revoked = TRUE WHERE session_id=$1`, s.SessionID); err != nil {
			return identity.StoredSession{}, identityStorageError(err)
		}
		if err = tx.Commit(ctx); err != nil {
			return identity.StoredSession{}, identityStorageError(err)
		}
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	// 行锁等待也消耗令牌有效期；保留领域时钟作为基准，只追加本调用内的单调耗时。
	n.Now = n.Now.Add(time.Since(started))
	if !n.Now.Before(s.RefreshExpiresAt) {
		return identity.StoredSession{}, identity.ErrTokenExpired
	}
	s.AccessDigest = n.AccessDigest
	s.RefreshDigest = n.RefreshDigest
	s.AccessExpiresAt = n.Now.Add(n.AccessTTL)
	if s.AccessExpiresAt.After(s.RefreshExpiresAt) {
		s.AccessExpiresAt = s.RefreshExpiresAt
	}
	if _, err = tx.Exec(ctx, `UPDATE online_identity_refresh_credentials SET consumed_at=$2 WHERE digest=$1`, digest, n.Now); err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if _, err = tx.Exec(ctx, `UPDATE online_identity_sessions SET access_digest=$2,refresh_digest=$3,access_expires_at=$4 WHERE session_id=$1`, s.SessionID, s.AccessDigest, s.RefreshDigest, s.AccessExpiresAt); err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if _, err = tx.Exec(ctx, `INSERT INTO online_identity_refresh_credentials (digest,session_id) VALUES ($1,$2)`, s.RefreshDigest, s.SessionID); err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	if err = tx.Commit(ctx); err != nil {
		return identity.StoredSession{}, identityStorageError(err)
	}
	return s, nil
}

// Logout 接受当前或历史刷新摘要，即使过期/禁用/已撤销也可幂等撤销本会话，不影响其他会话。
func (r *OnlineIdentityRepository) Logout(ctx context.Context, digest string) error {
	if r.db == nil {
		return identity.ErrUnavailable
	}
	tx, err := r.db.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return identityStorageError(err)
	}
	defer tx.Rollback(ctx)
	s, _, _, err := lockRefreshSession(ctx, tx, digest)
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.ErrInvalidToken
	}
	if err != nil {
		return identityStorageError(err)
	}
	if _, err = tx.Exec(ctx, `UPDATE online_identity_sessions SET revoked = TRUE WHERE session_id=$1`, s.SessionID); err != nil {
		return identityStorageError(err)
	}
	return identityStorageError(tx.Commit(ctx))
}

// 原始pgx错误可能含SQL参数；领域边界统一安全分类，不串接驱动错误字符串。
func identityStorageError(err error) error {
	if err == nil {
		return nil
	}
	if errors.Is(err, context.Canceled) {
		return context.Canceled
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return context.DeadlineExceeded
	}
	return identity.ErrUnavailable
}
