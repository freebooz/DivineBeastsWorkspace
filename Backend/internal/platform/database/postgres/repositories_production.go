//go:build productiondeps

package postgres

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/platform/outbox"
)

// PlayerRepository（玩家长期数据PostgreSQL仓储）实现playerdata.Repository。
type PlayerRepository struct{ pool *Pool }

// NewPlayerRepository（创建玩家资料仓储）绑定共享PostgreSQL连接池。
func NewPlayerRepository(pool *Pool) *PlayerRepository {
	if pool == nil {
		panic("PostgreSQL Pool不能为空")
	}
	return &PlayerRepository{pool: pool}
}

// Get（读取玩家资料）按PlayerID读取长期权威数据。
func (r *PlayerRepository) Get(ctx context.Context, playerID string) (playerdata.Profile, error) {
	var profile playerdata.Profile
	var ownedJSON []byte
	err := r.pool.inner.QueryRow(ctx, `
SELECT player_id, game_id, display_name, data_version, revision, tutorial_completed, default_world_id, owned_character_ids
FROM player_profiles
WHERE player_id = $1`, playerID).Scan(&profile.PlayerID, &profile.GameID, &profile.DisplayName, &profile.DataVersion, &profile.Revision, &profile.TutorialCompleted, &profile.DefaultWorldID, &ownedJSON)
	if errors.Is(err, pgx.ErrNoRows) {
		return playerdata.Profile{}, errors.New("玩家资料不存在")
	}
	if err != nil {
		return playerdata.Profile{}, fmt.Errorf("读取玩家资料失败: %w", err)
	}
	if err := json.Unmarshal(ownedJSON, &profile.OwnedCharacterIDs); err != nil {
		return playerdata.Profile{}, fmt.Errorf("解析OwnedCharacterIDs失败: %w", err)
	}
	return profile, nil
}

// Save（保存玩家资料）使用Revision做乐观并发控制。
func (r *PlayerRepository) Save(ctx context.Context, profile playerdata.Profile, expectedRevision int64) (playerdata.Profile, error) {
	ownedJSON, err := json.Marshal(profile.OwnedCharacterIDs)
	if err != nil {
		return playerdata.Profile{}, err
	}
	var saved playerdata.Profile
	var savedOwned []byte
	err = r.pool.inner.QueryRow(ctx, `
UPDATE player_profiles
SET display_name = $2,
    data_version = $3,
    tutorial_completed = $4,
    default_world_id = $5,
    owned_character_ids = $6,
    revision = revision + 1,
    updated_at = NOW()
WHERE player_id = $1 AND revision = $7
RETURNING player_id, game_id, display_name, data_version, revision, tutorial_completed, default_world_id, owned_character_ids`,
		profile.PlayerID, profile.DisplayName, profile.DataVersion, profile.TutorialCompleted, profile.DefaultWorldID, ownedJSON, expectedRevision,
	).Scan(&saved.PlayerID, &saved.GameID, &saved.DisplayName, &saved.DataVersion, &saved.Revision, &saved.TutorialCompleted, &saved.DefaultWorldID, &savedOwned)
	if errors.Is(err, pgx.ErrNoRows) {
		return playerdata.Profile{}, errors.New("PLAYER_DATA_CONFLICT: 玩家资料Revision冲突或不存在")
	}
	if err != nil {
		return playerdata.Profile{}, fmt.Errorf("保存玩家资料失败: %w", err)
	}
	if err := json.Unmarshal(savedOwned, &saved.OwnedCharacterIDs); err != nil {
		return playerdata.Profile{}, err
	}
	return saved, nil
}

// EnsureProfile（确保玩家资料存在）供Identity登录后的应用编排按需创建默认长期资料。
func (r *PlayerRepository) EnsureProfile(ctx context.Context, playerID, gameID string) error {
	_, err := r.pool.inner.Exec(ctx, `
INSERT INTO player_profiles (player_id, game_id, display_name, data_version, revision, tutorial_completed, default_world_id)
VALUES ($1, $2, $3, 1, 1, FALSE, 'World.OpenWorld.Hub')
ON CONFLICT (player_id) DO NOTHING`, playerID, gameID, "新玩家")
	return err
}

// MatchOutboxStore（比赛结果+Outbox PostgreSQL仓储）同时实现match.TransactionalResultStore和outbox.Store。
type MatchOutboxStore struct{ pool *Pool }

// NewMatchOutboxStore（创建比赛结果与Outbox仓储）共享同一连接池以支持真正数据库事务。
func NewMatchOutboxStore(pool *Pool) *MatchOutboxStore {
	if pool == nil {
		panic("PostgreSQL Pool不能为空")
	}
	return &MatchOutboxStore{pool: pool}
}

// Get（读取比赛结果）按MatchID返回幂等提交快照。
func (s *MatchOutboxStore) Get(ctx context.Context, matchID string) (match.StoredResult, bool, error) {
	var stored match.StoredResult
	var payload []byte
	err := s.pool.inner.QueryRow(ctx, `
SELECT result_id, payload
FROM match_results
WHERE match_id = $1`, matchID).Scan(&stored.ResultID, &payload)
	if errors.Is(err, pgx.ErrNoRows) {
		return match.StoredResult{}, false, nil
	}
	if err != nil {
		return match.StoredResult{}, false, fmt.Errorf("读取比赛结果失败: %w", err)
	}
	if err := json.Unmarshal(payload, &stored.Result); err != nil {
		return match.StoredResult{}, false, fmt.Errorf("解析比赛结果Payload失败: %w", err)
	}
	return stored, true, nil
}

// Insert（插入比赛结果）用于不需要Outbox的兼容调用；正式MatchService使用InsertWithEvent。
func (s *MatchOutboxStore) Insert(ctx context.Context, stored match.StoredResult) error {
	payload, err := json.Marshal(stored.Result)
	if err != nil {
		return err
	}
	_, err = s.pool.inner.Exec(ctx, `
INSERT INTO match_results (match_id, result_id, arena_mode_id, game_server_id, winning_team_id, started_at, ended_at, payload)
VALUES ($1,$2,$3,$4,$5,$6,$7,$8)`, stored.Result.MatchID, stored.ResultID, stored.Result.ArenaModeID, stored.Result.GameServerID, nullableString(stored.Result.WinningTeamID), nullableTime(stored.Result.StartedAt), nullableTime(stored.Result.EndedAt), payload)
	return err
}

// InsertWithEvent（事务提交比赛结果和Outbox事件）是Transactional Outbox的关键一致性边界。
func (s *MatchOutboxStore) InsertWithEvent(ctx context.Context, stored match.StoredResult, event match.IntegrationEvent) error {
	tx, err := s.pool.inner.Begin(ctx)
	if err != nil {
		return err
	}
	defer func() { _ = tx.Rollback(ctx) }()
	payload, err := json.Marshal(stored.Result)
	if err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `
INSERT INTO match_results (match_id, result_id, arena_mode_id, game_server_id, winning_team_id, started_at, ended_at, payload)
VALUES ($1,$2,$3,$4,$5,$6,$7,$8)`, stored.Result.MatchID, stored.ResultID, stored.Result.ArenaModeID, stored.Result.GameServerID, nullableString(stored.Result.WinningTeamID), nullableTime(stored.Result.StartedAt), nullableTime(stored.Result.EndedAt), payload); err != nil {
		return fmt.Errorf("写入比赛结果失败: %w", err)
	}
	if _, err := tx.Exec(ctx, `
INSERT INTO outbox_messages (message_id, topic, aggregate_id, payload, occurred_at)
VALUES ($1,$2,$3,$4,$5)`, event.ID, event.Topic, event.AggregateID, event.Payload, event.OccurredAt); err != nil {
		return fmt.Errorf("写入Outbox事件失败: %w", err)
	}
	return tx.Commit(ctx)
}

// Enqueue（单独写入Outbox事件）供非比赛业务复用。
func (s *MatchOutboxStore) Enqueue(ctx context.Context, message outbox.Message) error {
	_, err := s.pool.inner.Exec(ctx, `
INSERT INTO outbox_messages (message_id, topic, aggregate_id, payload, occurred_at)
VALUES ($1,$2,$3,$4,$5)
ON CONFLICT (message_id) DO NOTHING`, message.ID, message.Topic, message.AggregateID, message.Payload, message.OccurredAt)
	return err
}

// ClaimPending（租约领取一批待发布事件）通过CTE + FOR UPDATE SKIP LOCKED支持多副本Dispatcher并发。
func (s *MatchOutboxStore) ClaimPending(ctx context.Context, limit int, workerID string, leaseUntil time.Time) ([]outbox.Message, error) {
	rows, err := s.pool.inner.Query(ctx, `
WITH candidates AS (
    SELECT message_id
    FROM outbox_messages
    WHERE state = 'pending' OR (state = 'publishing' AND locked_until < NOW())
    ORDER BY occurred_at, message_id
    LIMIT $1
    FOR UPDATE SKIP LOCKED
)
UPDATE outbox_messages AS o
SET state = 'publishing', lock_owner = $2, locked_until = $3
FROM candidates c
WHERE o.message_id = c.message_id
RETURNING o.message_id, o.topic, o.aggregate_id, o.payload, o.occurred_at,
          o.state, o.attempts, o.published_at, o.lock_owner, o.locked_until`, limit, workerID, leaseUntil)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	result := make([]outbox.Message, 0)
	for rows.Next() {
		var item outbox.Message
		var publishedAt *time.Time
		var lockedUntil *time.Time
		if err := rows.Scan(&item.ID, &item.Topic, &item.AggregateID, &item.Payload, &item.OccurredAt, &item.State, &item.Attempts, &publishedAt, &item.LockOwner, &lockedUntil); err != nil {
			return nil, err
		}
		if publishedAt != nil {
			item.PublishedAt = publishedAt.UTC()
		}
		if lockedUntil != nil {
			item.LockedUntil = lockedUntil.UTC()
		}
		result = append(result, item)
	}
	return result, rows.Err()
}

// MarkPublished（确认消息已发布）要求仍由当前Worker持有租约。
func (s *MatchOutboxStore) MarkPublished(ctx context.Context, messageID, workerID string, publishedAt time.Time) error {
	command, err := s.pool.inner.Exec(ctx, `
UPDATE outbox_messages
SET state='published', published_at=$3, lock_owner='', locked_until=NULL
WHERE message_id=$1 AND state='publishing' AND lock_owner=$2`, messageID, workerID, publishedAt)
	if err != nil {
		return err
	}
	if command.RowsAffected() != 1 {
		return errors.New("OUTBOX_LEASE_LOST: 标记Published失败")
	}
	return nil
}

// MarkFailed（记录发布失败）增加Attempts并释放租约。
func (s *MatchOutboxStore) MarkFailed(ctx context.Context, messageID, workerID string) error {
	command, err := s.pool.inner.Exec(ctx, `
UPDATE outbox_messages
SET attempts=attempts+1, state='pending', lock_owner='', locked_until=NULL
WHERE message_id=$1 AND state='publishing' AND lock_owner=$2`, messageID, workerID)
	if err != nil {
		return err
	}
	if command.RowsAffected() != 1 {
		return errors.New("OUTBOX_LEASE_LOST: 标记发布失败状态失败")
	}
	return nil
}

func nullableString(value string) any {
	if value == "" {
		return nil
	}
	return value
}

func nullableTime(value time.Time) any {
	if value.IsZero() {
		return nil
	}
	return value.UTC()
}
