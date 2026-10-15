//go:build productiondeps

// 本文件实现MatchService持久步骤仓储；PG行锁、数据库时钟和条件版本共同约束多副本租约。
// 操作秘密只在受限payload中保存，阶段Outbox与步骤同事务提交；不在SQL错误中拼接payload。
package postgres

import (
	"context"
	"divinebeasts/backend/internal/app/matchservice"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/jackc/pgx/v5"
	"time"
)

// MatchOperationStore复用正式连接池；生命周期归装配根，仓储不启动独立轮询或自动迁移。
type MatchOperationStore struct{ pool *Pool }

// NewMatchOperationStore要求真实PG连接池，生产环境不能回退内存成功。
func NewMatchOperationStore(pool *Pool) *MatchOperationStore {
	if pool == nil {
		panic("比赛步骤必须提供PostgreSQL连接池")
	}
	return &MatchOperationStore{pool: pool}
}

// GetOperation读取完整后端恢复状态；未找到与读取失败分开，不得把故障当作新操作。
func (s *MatchOperationStore) GetOperation(ctx context.Context, id string) (matchservice.Operation, bool, error) {
	var payload []byte
	err := s.pool.inner.QueryRow(ctx, `SELECT payload FROM match_operations WHERE operation_id=$1`, id).Scan(&payload)
	if errors.Is(err, pgx.ErrNoRows) {
		return matchservice.Operation{}, false, nil
	}
	if err != nil {
		return matchservice.Operation{}, false, err
	}
	var value matchservice.Operation
	if err = json.Unmarshal(payload, &value); err != nil {
		return value, false, errors.New("MATCH_OPERATION_CORRUPT: 持久步骤无法解码")
	}
	return value, true, nil
}

// lockedOperation仅在事务内读取行与PG时钟，避免不同进程系统时钟误判租约有效期。
func lockedOperation(ctx context.Context, tx pgx.Tx, id string) (matchservice.Operation, time.Time, error) {
	var payload []byte
	var now time.Time
	if err := tx.QueryRow(ctx, `SELECT payload,clock_timestamp() FROM match_operations WHERE operation_id=$1 FOR UPDATE`, id).Scan(&payload, &now); err != nil {
		return matchservice.Operation{}, now, err
	}
	var value matchservice.Operation
	err := json.Unmarshal(payload, &value)
	return value, now.UTC(), err
}

// writeOperation在已有行锁下保存权威元数据与JSON；发生阶段变化时同事务写入无秘密Outbox。
func writeOperation(ctx context.Context, tx pgx.Tx, value matchservice.Operation, emit bool) error {
	payload, err := json.Marshal(value)
	if err != nil {
		return err
	}
	if _, err = tx.Exec(ctx, `UPDATE match_operations SET version=$2,fence=$3,lease_owner=$4,lease_until=$5,stage=$6,payload=$7,updated_at=$8 WHERE operation_id=$1`, value.ID, value.Version, value.Fence, value.LeaseOwner, nullableTime(value.LeaseUntilUTC), value.Stage, payload, value.UpdatedAtUTC); err != nil {
		return err
	}
	if emit {
		event, err := json.Marshal(struct {
			OperationID string `json:"operationId"`
			Stage       string `json:"stage"`
			Version     int64  `json:"version"`
			Fence       int64  `json:"fence"`
		}{value.ID, value.Stage, value.Version, value.Fence})
		if err != nil {
			return err
		}
		_, err = tx.Exec(ctx, `INSERT INTO outbox_messages(message_id,topic,aggregate_id,payload,occurred_at) VALUES($1,'match.operation.changed',$2,$3,$4)`, fmt.Sprintf("match-operation:%s:%d", value.ID, value.Version), value.ID, event, value.UpdatedAtUTC)
		if err != nil {
			return err
		}
	}
	return nil
}

// ClaimOperation首次插入合法意图；重复领取必须同摘要，过期接管递增Fence，数据库时钟决定租约。
func (s *MatchOperationStore) ClaimOperation(ctx context.Context, proposal matchservice.Operation, owner string, _ time.Time, lease time.Duration) (matchservice.Operation, error) {
	if err := matchservice.ValidateOperationInput(proposal); err != nil {
		return matchservice.Operation{}, err
	}
	if owner == "" || lease <= 0 || lease > 30*time.Second {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_INVALID: 租约无效")
	}
	tx, err := s.pool.inner.Begin(ctx)
	if err != nil {
		return matchservice.Operation{}, err
	}
	defer tx.Rollback(context.Background())
	payload, err := json.Marshal(proposal)
	if err != nil {
		return matchservice.Operation{}, err
	}
	tag, err := tx.Exec(ctx, `INSERT INTO match_operations(operation_id,input_fingerprint,version,fence,stage,payload) VALUES($1,$2,1,1,$3,$4) ON CONFLICT(operation_id) DO NOTHING`, proposal.ID, proposal.InputFingerprint, proposal.Stage, payload)
	if err != nil {
		return matchservice.Operation{}, err
	}
	value, now, err := lockedOperation(ctx, tx, proposal.ID)
	if err != nil {
		return matchservice.Operation{}, err
	}
	if value.InputFingerprint != proposal.InputFingerprint {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_CONFLICT: 同身份异参")
	}
	if value.LeaseOwner != "" && value.LeaseOwner != owner && value.LeaseUntilUTC.After(now) {
		return matchservice.Operation{}, matchservice.ErrOperationBusy
	}
	if value.Version >= int64(^uint64(0)>>1) || value.Fence >= int64(^uint64(0)>>1) {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_VERSION_EXHAUSTED: 版本耗尽")
	}
	value.Version++
	value.Fence++
	value.LeaseOwner = owner
	value.LeaseUntilUTC = now.Add(lease)
	value.UpdatedAtUTC = now
	if err = writeOperation(ctx, tx, value, tag.RowsAffected() == 1); err != nil {
		return matchservice.Operation{}, err
	}
	if err = tx.Commit(ctx); err != nil {
		return matchservice.Operation{}, err
	}
	return value, nil
}

// SaveOperation在同一行锁下核对读取版本、栅栏、拥有者和未过期租约；任何失败均回滚步骤与事件。
func (s *MatchOperationStore) SaveOperation(ctx context.Context, next matchservice.Operation, _ time.Time, lease time.Duration) (matchservice.Operation, error) {
	if err := matchservice.ValidateOperationInput(next); err != nil {
		return matchservice.Operation{}, err
	}
	if lease <= 0 || lease > 30*time.Second {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_INVALID: 租约无效")
	}
	tx, err := s.pool.inner.Begin(ctx)
	if err != nil {
		return matchservice.Operation{}, err
	}
	defer tx.Rollback(context.Background())
	old, now, err := lockedOperation(ctx, tx, next.ID)
	if errors.Is(err, pgx.ErrNoRows) {
		return matchservice.Operation{}, matchservice.ErrOperationLeaseLost
	}
	if err != nil {
		return matchservice.Operation{}, err
	}
	if old.Version != next.Version || old.Fence != next.Fence || old.LeaseOwner != next.LeaseOwner || next.LeaseOwner == "" || !old.LeaseUntilUTC.After(now) {
		return matchservice.Operation{}, matchservice.ErrOperationLeaseLost
	}
	if old.InputFingerprint != next.InputFingerprint {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_CONFLICT: 不可变意图变化")
	}
	if err = matchservice.ValidateOperationTransition(old.Stage, next.Stage); err != nil {
		return matchservice.Operation{}, err
	}
	if next.Version >= int64(^uint64(0)>>1) {
		return matchservice.Operation{}, errors.New("MATCH_OPERATION_VERSION_EXHAUSTED: 版本耗尽")
	}
	next.Version++
	next.UpdatedAtUTC = now
	next.LeaseUntilUTC = now.Add(lease)
	if err = writeOperation(ctx, tx, next, old.Stage != next.Stage); err != nil {
		return matchservice.Operation{}, err
	}
	if err = tx.Commit(ctx); err != nil {
		return matchservice.Operation{}, err
	}
	return next, nil
}

// ReleaseOperation只解除同一Fence拥有者，保留完整步骤供恢复；旧拥有者释放请求不能影响后继。
func (s *MatchOperationStore) ReleaseOperation(ctx context.Context, id, owner string, fence int64) error {
	tx, err := s.pool.inner.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(context.Background())
	value, _, err := lockedOperation(ctx, tx, id)
	if err != nil {
		return err
	}
	if value.Fence != fence || value.LeaseOwner != owner || owner == "" {
		return matchservice.ErrOperationLeaseLost
	}
	value.LeaseOwner = ""
	value.LeaseUntilUTC = time.Time{}
	if err = writeOperation(ctx, tx, value, false); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

var _ matchservice.OperationStore = (*MatchOperationStore)(nil)
