//go:build productiondeps

package postgres

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"errors"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/platform/apperror"
)

// OnlinePlayerRepository复用既有仓储及Save兼容接口，为Online提供分类读取和严格幂等建档。
// 只由玩家资料服务装配；身份服务/网关不得越过该服务直接调用本仓储。
type OnlinePlayerRepository struct{ *PlayerRepository }

// NewOnlinePlayerRepository绑定既有连接池，不建立连接、不执行迁移；pool不可为空。
// 准确装配为playerdata.NewService(postgres.NewOnlinePlayerRepository(pool))。
func NewOnlinePlayerRepository(pool *Pool) *OnlinePlayerRepository {
	return &OnlinePlayerRepository{PlayerRepository: NewPlayerRepository(pool)}
}

var _ playerdata.Repository = (*OnlinePlayerRepository)(nil)
var _ playerdata.IdempotentDisplayNameRepository = (*OnlinePlayerRepository)(nil)
var _ playerdata.ProfileInitializer = (*OnlinePlayerRepository)(nil)
var _ playerdata.RepositoryProbe = (*OnlinePlayerRepository)(nil)

const onlinePlayerOperation = "UpdateCurrentPlayerProfile"
const onlinePlayerColumns = `player_id, game_id, display_name, data_version, revision, tutorial_completed, default_world_id, owned_character_ids`

// Get仅查询已存在的资料，不建档；无资料与依赖不可用分别返回404/503对应错误。
func (r *OnlinePlayerRepository) Get(ctx context.Context, playerID string) (playerdata.Profile, error) {
	if err := ctx.Err(); err != nil {
		return playerdata.Profile{}, err
	}
	if !validOnlinePlayerIdentity(playerID) {
		return playerdata.Profile{}, onlinePlayerInvalid()
	}
	if r == nil || !r.PlayerRepository.onlinePlayerAvailable() {
		return playerdata.Profile{}, onlinePlayerUnavailable()
	}
	profile, err := scanOnlinePlayer(r.pool.inner.QueryRow(ctx, `SELECT `+onlinePlayerColumns+` FROM player_profiles WHERE player_id=$1`, playerID))
	return profile, onlinePlayerError(err)
}

// EnsureProfile只插入默认资料；同身份同游戏不覆盖已有名称、修订、教学或权益。
// 已存在身份绑定另一游戏时明确冲突，不能把初始化幂等当作跨游戏重新绑定权限。
func (r *OnlinePlayerRepository) EnsureProfile(ctx context.Context, playerID, gameID string) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !validOnlinePlayerIdentity(playerID) || !validOnlinePlayerIdentity(gameID) {
		return onlinePlayerInvalid()
	}
	if r == nil || !r.PlayerRepository.onlinePlayerAvailable() {
		return onlinePlayerUnavailable()
	}
	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return onlinePlayerError(err)
	}
	defer rollbackOnlinePlayer(tx)
	_, err = tx.Exec(ctx, `INSERT INTO player_profiles
		(player_id, game_id, display_name, data_version, revision, tutorial_completed, default_world_id)
		VALUES ($1,$2,'新玩家',1,1,FALSE,'World.OpenWorld.Hub') ON CONFLICT (player_id) DO NOTHING`, playerID, gameID)
	if err != nil {
		return onlinePlayerError(err)
	}
	var storedGameID string
	err = tx.QueryRow(ctx, `SELECT game_id FROM player_profiles WHERE player_id=$1 FOR UPDATE`, playerID).Scan(&storedGameID)
	if err != nil {
		return onlinePlayerError(err)
	}
	if storedGameID != gameID {
		return apperror.New("PLAYER_DATA_CONFLICT", "玩家资料已绑定其他游戏", false)
	}
	return commitOnlinePlayer(ctx, tx)
}

// Probe真实读取资料与本切片幂等表的列结构和权限；缺表、缺列、连接失败均不能报就绪。
// 不建档、不写表、不执行迁移；调用者应传入有截止时间的上下文。
func (r *PlayerRepository) Probe(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !r.onlinePlayerAvailable() {
		return onlinePlayerUnavailable()
	}
	rows, err := r.pool.inner.Query(ctx, `SELECT
		p.player_id,p.game_id,p.display_name,p.data_version,p.revision,p.tutorial_completed,p.default_world_id,p.owned_character_ids,p.updated_at,
		i.player_id,i.operation,i.idempotency_key,i.canonical_request,i.response_snapshot,i.created_at
		FROM player_profiles p CROSS JOIN online_profile_idempotency i LIMIT 0`)
	if err != nil {
		return onlinePlayerError(err)
	}
	rows.Close()
	return onlinePlayerError(rows.Err())
}

// UpdateDisplayNameIdempotent在单个PG事务内执行白名单写入并持久化完整响应快照。
// 主体+操作+键限定结果，规范请求包含trim后的名称和期望修订。先重放再检查修订，
// 因而响应丢失后的重试不会再次更新，也不会被已经增加的revision错误拒绝。
// advisory事务锁解决尚无幂等行时的同键竞争；哈希碰撞只增加等待，不合并结果身份。
func (r *PlayerRepository) UpdateDisplayNameIdempotent(ctx context.Context, playerID, displayName string, expectedRevision int64, key string) (playerdata.Profile, error) {
	if err := ctx.Err(); err != nil {
		return playerdata.Profile{}, err
	}
	name, err := playerdata.NormalizeDisplayNameUpdate(playerID, displayName, expectedRevision, key)
	if err != nil {
		return playerdata.Profile{}, err
	}
	if !r.onlinePlayerAvailable() {
		return playerdata.Profile{}, onlinePlayerUnavailable()
	}
	request, err := json.Marshal(struct {
		DisplayName      string `json:"displayName"`
		ExpectedRevision int64  `json:"expectedRevision"`
	}{name, expectedRevision})
	if err != nil {
		return playerdata.Profile{}, onlinePlayerUnavailable()
	}
	// 显式READ COMMITTED使等待竞争者提交后，后续SELECT取得最新已提交幂等结果。
	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return playerdata.Profile{}, onlinePlayerError(err)
	}
	defer rollbackOnlinePlayer(tx)
	if _, err = tx.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`, onlinePlayerLockID(playerID, key)); err != nil {
		return playerdata.Profile{}, onlinePlayerError(err)
	}
	var previousRequest, snapshot []byte
	err = tx.QueryRow(ctx, `SELECT canonical_request,response_snapshot FROM online_profile_idempotency
		WHERE player_id=$1 AND operation=$2 AND idempotency_key=$3`, playerID, onlinePlayerOperation, key).Scan(&previousRequest, &snapshot)
	if err == nil {
		if !bytes.Equal(previousRequest, request) {
			return playerdata.Profile{}, apperror.New("IDEMPOTENCY_CONFLICT", "幂等键已绑定不同的资料更新请求", false)
		}
		var replay playerdata.Profile
		if json.Unmarshal(snapshot, &replay) != nil || replay.PlayerID != playerID {
			return playerdata.Profile{}, onlinePlayerUnavailable()
		}
		// 只读重放无需再次提交；defer释放事务锁，持久结果完全来自首次成功事务。
		return replay, nil
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return playerdata.Profile{}, onlinePlayerError(err)
	}
	profile, err := scanOnlinePlayer(tx.QueryRow(ctx, `UPDATE player_profiles
		SET display_name=$2, revision=revision+1, updated_at=NOW()
		WHERE player_id=$1 AND revision=$3 AND revision<9223372036854775807
		RETURNING `+onlinePlayerColumns, playerID, name, expectedRevision))
	if errors.Is(err, pgx.ErrNoRows) {
		var exists bool
		if lookupErr := tx.QueryRow(ctx, `SELECT EXISTS(SELECT 1 FROM player_profiles WHERE player_id=$1)`, playerID).Scan(&exists); lookupErr != nil {
			return playerdata.Profile{}, onlinePlayerError(lookupErr)
		}
		if !exists {
			return playerdata.Profile{}, onlinePlayerNotFound()
		}
		return playerdata.Profile{}, apperror.New("PLAYER_DATA_CONFLICT", "玩家资料修订冲突或已达修订上限", false)
	}
	if err != nil {
		return playerdata.Profile{}, onlinePlayerError(err)
	}
	snapshot, err = json.Marshal(profile)
	if err != nil {
		return playerdata.Profile{}, onlinePlayerUnavailable()
	}
	_, err = tx.Exec(ctx, `INSERT INTO online_profile_idempotency
		(player_id,operation,idempotency_key,canonical_request,response_snapshot) VALUES($1,$2,$3,$4,$5::jsonb)`,
		playerID, onlinePlayerOperation, key, request, string(snapshot))
	if err != nil {
		return playerdata.Profile{}, onlinePlayerError(err)
	}
	if err = commitOnlinePlayer(ctx, tx); err != nil {
		return playerdata.Profile{}, err
	}
	return profile, nil
}

func (r *PlayerRepository) onlinePlayerAvailable() bool {
	return r != nil && r.pool != nil && r.pool.inner != nil
}
func validOnlinePlayerIdentity(value string) bool {
	return utf8.ValidString(value) && strings.TrimSpace(value) != "" && !strings.ContainsRune(value, 0)
}
func onlinePlayerInvalid() error {
	return apperror.New("INVALID_REQUEST", "玩家资料请求参数非法", false)
}
func onlinePlayerUnavailable() error {
	return apperror.New("SERVICE_UNAVAILABLE", "玩家资料持久服务暂不可用", true)
}
func onlinePlayerNotFound() error {
	return apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
}
func onlinePlayerError(err error) error {
	if err == nil {
		return nil
	}
	if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
		return err
	}
	if errors.Is(err, pgx.ErrNoRows) {
		return onlinePlayerNotFound()
	}
	// 不把SQL、连接配置或原始请求拼入公开错误；事务失败统一由调用者按原键安全恢复。
	return onlinePlayerUnavailable()
}
func scanOnlinePlayer(row pgx.Row) (playerdata.Profile, error) {
	var profile playerdata.Profile
	var ownedJSON []byte
	err := row.Scan(&profile.PlayerID, &profile.GameID, &profile.DisplayName, &profile.DataVersion, &profile.Revision,
		&profile.TutorialCompleted, &profile.DefaultWorldID, &ownedJSON)
	if err != nil {
		return playerdata.Profile{}, err
	}
	if err = json.Unmarshal(ownedJSON, &profile.OwnedCharacterIDs); err != nil {
		return playerdata.Profile{}, err
	}
	return profile, nil
}
func onlinePlayerLockID(playerID, key string) int64 {
	// JSON数组避免分隔符注入；操作身份参与锁键，锁的碰撞不会改变表的完整主键比较。
	encoded, _ := json.Marshal([3]string{playerID, onlinePlayerOperation, key})
	digest := sha256.Sum256(encoded)
	return int64(binary.BigEndian.Uint64(digest[:8]))
}
func rollbackOnlinePlayer(tx pgx.Tx) {
	// 请求已取消也要释放连接/锁；使用独立且有界的清理上下文，不启动后台无限重试。
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_ = tx.Rollback(ctx) // 已提交/已回滚时pgx返回ErrTxClosed；连接故障时驱动丢弃连接。
}
func commitOnlinePlayer(ctx context.Context, tx pgx.Tx) error {
	if err := tx.Commit(ctx); err != nil {
		// COMMIT响应丢失无法证明未提交；不返回成功或声称回滚，原键重试可读取持久结果。
		return apperror.New("SERVICE_UNAVAILABLE", "资料事务提交结果未确认，请按原参数重试或查询；幂等更新必须保留原键", true)
	}
	return nil
}
