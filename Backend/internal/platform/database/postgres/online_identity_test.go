//go:build productiondeps

package postgres

import (
	"context"
	"errors"
	"strings"
	"testing"
	"time"

	"divinebeasts/backend/internal/modules/identity"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
)

// 仅用于仓储事务边界测试；不冒充真实PostgreSQL行锁和重启持久化证明。
type identityRow func(...any) error

func (r identityRow) Scan(dest ...any) error { return r(dest...) }

type identityTxDouble struct {
	pgx.Tx
	rows          []identityRow
	execSQL       []string
	committed     bool
	commitErr     error
	readCommitted bool
}

func (t *identityTxDouble) QueryRow(context.Context, string, ...any) pgx.Row {
	if len(t.rows) == 0 {
		return identityRow(func(...any) error { return errors.New("unexpected query") })
	}
	r := t.rows[0]
	t.rows = t.rows[1:]
	return r
}
func (t *identityTxDouble) Exec(_ context.Context, sql string, _ ...any) (pgconn.CommandTag, error) {
	t.execSQL = append(t.execSQL, sql)
	return pgconn.NewCommandTag("UPDATE 1"), nil
}
func (t *identityTxDouble) Commit(context.Context) error   { t.committed = true; return t.commitErr }
func (t *identityTxDouble) Rollback(context.Context) error { return nil }

type identityPoolDouble struct{ tx *identityTxDouble }

func (p identityPoolDouble) Begin(context.Context) (pgx.Tx, error) { return p.tx, nil }
func (p identityPoolDouble) BeginTx(_ context.Context, options pgx.TxOptions) (pgx.Tx, error) {
	p.tx.readCommitted = options.IsoLevel == pgx.ReadCommitted
	return p.tx, nil
}
func (p identityPoolDouble) QueryRow(ctx context.Context, sql string, a ...any) pgx.Row {
	return p.tx.QueryRow(ctx, sql, a...)
}

func lockedSessionRow(now time.Time, revoked, disabled bool) identityRow {
	return func(d ...any) error {
		*d[0].(*string) = "game"
		*d[1].(*string) = "player"
		*d[2].(*string) = "session"
		*d[3].(*string) = "device"
		*d[4].(*time.Time) = now.Add(time.Minute)
		*d[5].(*time.Time) = now.Add(time.Hour)
		*d[6].(*string) = strings.Repeat("a", 64)
		*d[7].(*string) = strings.Repeat("b", 64)
		*d[8].(*bool) = revoked
		*d[9].(*bool) = disabled
		return nil
	}
}

// 重放必须在返回401错误之前提交撤销；误用defer rollback吞掉撤销会使本测试失败。
func TestOnlineIdentityReplayCommitsRevocation(t *testing.T) {
	now := time.Now().UTC()
	tx := &identityTxDouble{rows: []identityRow{lockedSessionRow(now, false, false), func(d ...any) error { *d[0].(*bool) = true; return nil }}}
	r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
	_, err := r.Rotate(context.Background(), strings.Repeat("b", 64), identity.TokenRotation{Now: now, AccessTTL: time.Minute, AccessDigest: strings.Repeat("c", 64), RefreshDigest: strings.Repeat("d", 64)})
	if !errors.Is(err, identity.ErrInvalidToken) || !tx.committed || len(tx.execSQL) != 1 || !strings.Contains(tx.execSQL[0], "revoked = TRUE") {
		t.Fatal("重放应持久撤销再失败")
	}
}

func TestOnlineIdentityRotationCommitsAllWritesAndCapsExpiry(t *testing.T) {
	now := time.Now().UTC()
	tx := &identityTxDouble{rows: []identityRow{lockedSessionRow(now, false, false), func(d ...any) error { *d[0].(*bool) = false; return nil }}}
	r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
	fresh, err := r.Rotate(context.Background(), strings.Repeat("b", 64), identity.TokenRotation{Now: now, AccessTTL: 2 * time.Hour, AccessDigest: strings.Repeat("c", 64), RefreshDigest: strings.Repeat("d", 64)})
	if err != nil || !tx.committed || len(tx.execSQL) != 3 || !fresh.AccessExpiresAt.Equal(now.Add(time.Hour)) || fresh.AccessToken != "" {
		t.Fatal("消费历史/更新会话/插入新摘要须同事务，期限封顶")
	}
}

func TestOnlineIdentityCommitFailureNeverSucceeds(t *testing.T) {
	now := time.Now().UTC()
	tx := &identityTxDouble{commitErr: errors.New("db unavailable"), rows: []identityRow{lockedSessionRow(now, false, false)}}
	r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
	if err := r.Logout(context.Background(), strings.Repeat("b", 64)); !errors.Is(err, identity.ErrUnavailable) {
		t.Fatal("提交失败不能声称退出成功")
	}
}

func TestOnlineIdentityLogoutHistoricalCredentialIsIdempotent(t *testing.T) {
	for _, revoked := range []bool{false, true} {
		tx := &identityTxDouble{rows: []identityRow{lockedSessionRow(time.Now(), revoked, false)}}
		r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
		if err := r.Logout(context.Background(), strings.Repeat("b", 64)); err != nil || !tx.committed {
			t.Fatal("已撤销或历史凭据必须幂等退出")
		}
	}
}

// 显式事务隔离保证锁后第二条SELECT看见已提交消费历史，而非依赖连接默认设置。
func TestOnlineIdentityPinsReadCommitted(t *testing.T) {
	tx := &identityTxDouble{rows: []identityRow{lockedSessionRow(time.Now(), false, false)}}
	r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
	if err := r.Logout(context.Background(), strings.Repeat("b", 64)); err != nil {
		t.Fatal(err)
	}
	if !tx.readCommitted {
		t.Fatal("身份事务必须显式READ COMMITTED")
	}
}

// 用驱动等待模拟阻塞：有效期在查询/行锁等待中耗尽，不得用请求开始时间放行。
func TestOnlineIdentityExpiryIncludesDatabaseWait(t *testing.T) {
	for _, operation := range []string{"authenticate", "refresh"} {
		t.Run(operation, func(t *testing.T) {
			now := time.Now()
			row := lockedSessionRow(now, false, false)
			tx := &identityTxDouble{rows: []identityRow{func(d ...any) error {
				if err := row(d...); err != nil {
					return err
				}
				*d[4].(*time.Time) = now.Add(time.Millisecond)
				*d[5].(*time.Time) = now.Add(time.Millisecond)
				time.Sleep(5 * time.Millisecond)
				return nil
			}, func(d ...any) error { *d[0].(*bool) = false; return nil }}}
			r := &OnlineIdentityRepository{db: identityPoolDouble{tx}}
			var err error
			if operation == "authenticate" {
				_, err = r.Authenticate(context.Background(), strings.Repeat("a", 64), now)
			} else {
				_, err = r.Rotate(context.Background(), strings.Repeat("b", 64), identity.TokenRotation{Now: now, AccessTTL: time.Minute, AccessDigest: strings.Repeat("c", 64), RefreshDigest: strings.Repeat("d", 64)})
			}
			if !errors.Is(err, identity.ErrTokenExpired) {
				t.Fatal("存储等待期间过期必须拒绝")
			}
		})
	}
}
