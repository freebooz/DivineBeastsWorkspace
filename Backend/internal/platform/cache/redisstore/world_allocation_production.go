//go:build productiondeps

// 世界返回目标在签票前首次持久化；共享Redis WATCH只运行无副作用选择，绝不重复容量分配。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"encoding/json"
	"errors"
	"github.com/redis/go-redis/v9"
	"time"
)

func (s *ArenaLifecycleStore) GetOrCreateWorldAllocation(ctx context.Context, id, fp string, now time.Time, create func() (gameservercontrol.WorldAllocationRecord, error)) (gameservercontrol.WorldAllocationRecord, error) {
	var result gameservercontrol.WorldAllocationRecord
	if _, cluster := s.client.inner.(*redis.ClusterClient); cluster {
		return result, errors.New("WORLD_ALLOCATION_CLUSTER_UNSUPPORTED: 返回目标要求单逻辑写域")
	}
	key := "gameserver:world-transfer:" + id
	for attempt := 0; attempt < 8; attempt++ {
		err := s.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			raw, err := tx.Get(ctx, key).Bytes()
			if err == nil {
				if err = json.Unmarshal(raw, &result); err != nil {
					return err
				}
				if result.Fingerprint != fp {
					return errors.New("TRANSFER_IDEMPOTENCY_CONFLICT: 世界返回意图变化")
				}
				if !now.Before(result.ExpiresAtUTC) {
					return errors.New("TRANSFER_TICKET_EXPIRED: 世界返回意图过期")
				}
				return nil
			}
			if !errors.Is(err, redis.Nil) {
				return err
			}
			result, err = create()
			if err != nil {
				return err
			}
			if err = ctx.Err(); err != nil {
				return err
			}
			raw, err = json.Marshal(result)
			if err != nil {
				return err
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error { pipe.Set(ctx, key, raw, 10*time.Minute); return nil })
			return err
		}, key)
		if !errors.Is(err, redis.TxFailedErr) {
			return result, err
		}
	}
	return result, errors.New("WORLD_ALLOCATION_CONFLICT: 世界返回意图竞争")
}

var _ gameservercontrol.WorldAllocationStore = (*ArenaLifecycleStore)(nil)
