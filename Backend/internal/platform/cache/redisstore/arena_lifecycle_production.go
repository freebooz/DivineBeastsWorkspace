//go:build productiondeps

// 本文件保存竞技控制面事实与本人准入位置；单逻辑Redis WATCH/EXEC维护比赛/实例索引与防旧条件。
// 数据只在可信服务间读取，48小时恢复窗口不替代队列/结果的显式清理，也不包含迁移签名秘密。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"encoding/json"
	"errors"
	"github.com/redis/go-redis/v9"
	"time"
)

const arenaRecordPrefix = "gameserver:arena:"
const arenaServerIndexPrefix = "gameserver:arena-server:"
const playerPresencePrefix = "gameserver:presence:"

// ArenaLifecycleStore与签票/Epoch共享同一逻辑Redis写域，不支持跨槽Cluster原子提交。
type ArenaLifecycleStore struct{ client *Client }

func NewArenaLifecycleStore(client *Client) *ArenaLifecycleStore {
	return &ArenaLifecycleStore{client: client}
}

// GetArenaByMatch完整读取已确认事实；损坏记录/存储不可用不得解释为未分配。
func (s *ArenaLifecycleStore) GetArenaByMatch(ctx context.Context, id string) (gameservercontrol.ArenaAssignmentRecord, bool, error) {
	var value gameservercontrol.ArenaAssignmentRecord
	raw, err := s.client.inner.Get(ctx, arenaRecordPrefix+id).Bytes()
	if errors.Is(err, redis.Nil) {
		return value, false, nil
	}
	if err != nil {
		return value, false, err
	}
	if err := json.Unmarshal(raw, &value); err != nil {
		return value, false, err
	}
	if err := gameservercontrol.ValidateArenaRecord(value); err != nil {
		return value, false, err
	}
	return value, true, nil
}

// GetArenaByServer只通过条件维护的实例索引恢复，不扫描并任意选择同名比赛。
func (s *ArenaLifecycleStore) GetArenaByServer(ctx context.Context, id string) (gameservercontrol.ArenaAssignmentRecord, bool, error) {
	matchID, err := s.client.inner.Get(ctx, arenaServerIndexPrefix+id).Result()
	if errors.Is(err, redis.Nil) {
		return gameservercontrol.ArenaAssignmentRecord{}, false, nil
	}
	if err != nil {
		return gameservercontrol.ArenaAssignmentRecord{}, false, err
	}
	value, found, err := s.GetArenaByMatch(ctx, matchID)
	if err == nil && found && value.Assignment.GameServerID != id {
		return value, false, errors.New("ARENA_INDEX_INVALID: 实例索引归属不一致")
	}
	return value, found, err
}

// SaveArena在同一事务维持不可变意图与实例排他；取消只停止准入，直到released才允许复用实例索引。
func (s *ArenaLifecycleStore) SaveArena(ctx context.Context, value gameservercontrol.ArenaAssignmentRecord) error {
	if _, cluster := s.client.inner.(*redis.ClusterClient); cluster {
		return errors.New("ARENA_CLUSTER_UNSUPPORTED: 生命周期要求单逻辑写域")
	}
	if err := gameservercontrol.ValidateArenaRecord(value); err != nil {
		return err
	}
	key := arenaRecordPrefix + value.Assignment.Arena.MatchID
	index := arenaServerIndexPrefix + value.Assignment.GameServerID
	payload, err := json.Marshal(value)
	if err != nil {
		return err
	}
	for attempt := 0; attempt < 8; attempt++ {
		err = s.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			raw, err := tx.Get(ctx, key).Bytes()
			if err == nil {
				var old gameservercontrol.ArenaAssignmentRecord
				if err := json.Unmarshal(raw, &old); err != nil {
					return err
				}
				if err := gameservercontrol.ValidateArenaRecordTransition(old, value); err != nil {
					return err
				}
			} else if !errors.Is(err, redis.Nil) {
				return err
			}
			previous, err := tx.Get(ctx, index).Result()
			if err != nil && !errors.Is(err, redis.Nil) {
				return err
			}
			if value.Assignment.GameServerID != "" && previous != "" && previous != value.Assignment.Arena.MatchID {
				otherKey := arenaRecordPrefix + previous
				if err := tx.Watch(ctx, otherKey).Err(); err != nil {
					return err
				}
				raw, err := tx.Get(ctx, otherKey).Bytes()
				if err != nil {
					return err
				}
				var other gameservercontrol.ArenaAssignmentRecord
				if err := json.Unmarshal(raw, &other); err != nil {
					return err
				}
				if other.State != "released" {
					return errors.New("ARENA_SERVER_ALREADY_BOUND: 实例尚未迁出和回收")
				}
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				pipe.Set(ctx, key, payload, 48*time.Hour)
				if value.State == "released" {
					if previous == value.Assignment.Arena.MatchID {
						pipe.Del(ctx, index)
					}
				} else if value.Assignment.GameServerID != "" {
					pipe.Set(ctx, index, value.Assignment.Arena.MatchID, 48*time.Hour)
				}
				return nil
			})
			return err
		}, key, index)
		if !errors.Is(err, redis.TxFailedErr) {
			return err
		}
	}
	return errors.New("ARENA_REVISION_CONFLICT: 控制事实写入竞争")
}

// GetPresence不向公开客户端提供任意PlayerID查询，只由控制面认证上下文/Match锁定名单使用。
func (s *ArenaLifecycleStore) GetPresence(ctx context.Context, id string) (gameservercontrol.PlayerPresence, bool, error) {
	var value gameservercontrol.PlayerPresence
	raw, err := s.client.inner.Get(ctx, playerPresencePrefix+id).Bytes()
	if errors.Is(err, redis.Nil) {
		return value, false, nil
	}
	if err != nil {
		return value, false, err
	}
	if err := json.Unmarshal(raw, &value); err != nil {
		return value, false, err
	}
	return value, true, nil
}

// SavePresence原子拒绝旧准入覆盖新位置；首次写入也必须检查完整会话/Boot/Epoch。
func (s *ArenaLifecycleStore) SavePresence(ctx context.Context, value gameservercontrol.PlayerPresence) error {
	if _, cluster := s.client.inner.(*redis.ClusterClient); cluster {
		return errors.New("ARENA_CLUSTER_UNSUPPORTED: 本人位置要求单逻辑写域")
	}
	key := playerPresencePrefix + value.PlayerID
	payload, err := json.Marshal(value)
	if err != nil {
		return err
	}
	for attempt := 0; attempt < 8; attempt++ {
		err = s.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			var old gameservercontrol.PlayerPresence
			raw, err := tx.Get(ctx, key).Bytes()
			if err == nil {
				if err := json.Unmarshal(raw, &old); err != nil {
					return err
				}
			} else if !errors.Is(err, redis.Nil) {
				return err
			}
			if err := gameservercontrol.ValidatePresenceTransition(old, value); err != nil {
				return err
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error { pipe.Set(ctx, key, payload, 48*time.Hour); return nil })
			return err
		}, key)
		if !errors.Is(err, redis.TxFailedErr) {
			return err
		}
	}
	return errors.New("PLAYER_PRESENCE_CONFLICT: 本人准入位置写入竞争")
}

var _ gameservercontrol.ArenaLifecycleStore = (*ArenaLifecycleStore)(nil)
