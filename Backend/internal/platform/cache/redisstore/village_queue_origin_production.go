//go:build productiondeps

package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"encoding/json"
	"errors"
	"github.com/redis/go-redis/v9"
	"time"
)

const villageQueueOriginPrefix = "gameserver:village-queue-origin:"

// SaveVillageQueueOrigin原子锁定首次报告，且禁止当前准入已变时由旧Village补写。
func (s *ArenaLifecycleStore) SaveVillageQueueOrigin(ctx context.Context, id string, record gameservercontrol.VillageQueueOriginRecord, retention time.Duration) error {
	if _, cluster := s.client.inner.(*redis.ClusterClient); cluster {
		return errors.New("ARENA_CLUSTER_UNSUPPORTED: 排队位置要求单逻辑写域")
	}
	key := villageQueueOriginPrefix + id
	presenceKey := playerPresencePrefix + record.Presence.PlayerID
	payload, err := json.Marshal(record)
	if err != nil {
		return err
	}
	for attempt := 0; attempt < 8; attempt++ {
		err = s.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			raw, err := tx.Get(ctx, presenceKey).Bytes()
			if err != nil {
				return err
			}
			var current gameservercontrol.PlayerPresence
			if err = json.Unmarshal(raw, &current); err != nil {
				return err
			}
			if !record.MatchesPresence(current) {
				return errors.New("VILLAGE_QUEUE_ORIGIN_FORBIDDEN: 源准入已被替换")
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				pipe.SetNX(ctx, key, payload, retention)
				return nil
			})
			return err
		}, presenceKey)
		if !errors.Is(err, redis.TxFailedErr) {
			return err
		}
	}
	return errors.New("VILLAGE_QUEUE_ORIGIN_CONFLICT: 当前准入写入竞争")
}

func (s *ArenaLifecycleStore) GetVillageQueueOrigin(ctx context.Context, id string) (gameservercontrol.VillageQueueOriginRecord, bool, error) {
	var record gameservercontrol.VillageQueueOriginRecord
	raw, err := s.client.inner.Get(ctx, villageQueueOriginPrefix+id).Bytes()
	if errors.Is(err, redis.Nil) {
		return record, false, nil
	}
	if err != nil {
		return record, false, err
	}
	err = json.Unmarshal(raw, &record)
	return record, err == nil, err
}

var _ gameservercontrol.VillageQueueOriginStore = (*ArenaLifecycleStore)(nil)
