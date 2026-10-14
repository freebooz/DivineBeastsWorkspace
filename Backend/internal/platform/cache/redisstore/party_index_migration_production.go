//go:build productiondeps

// 本文件实现显式停写维护窗口内的普通队伍唯一索引迁移，不在正常请求路径自动扫描或选择重复队伍。
// 所有旧版本Match写进程必须先停止；失败不发布版本标记，新命令因此保持关闭，修复后可重跑。
package redisstore

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"divinebeasts/backend/internal/modules/party"
	"github.com/redis/go-redis/v9"
)

// MigrateMembershipIndex（显式停写迁移）只供运维装配调用，maxParties为有限扫描上限。
// 已发布schema幂等返回0；重复成员或变动聚合立即失败，不能自行挑一个队伍或虚构历史加入顺序。
func (r *PartyRepository) MigrateMembershipIndex(ctx context.Context, maxParties int) (int, error) {
	if _, cluster := r.client.inner.(*redis.ClusterClient); cluster || maxParties <= 0 || maxParties > 100000 {
		return 0, errors.New("PARTY_MIGRATION_INVALID: 需要单写域和1至100000的扫描上限")
	}
	if version, err := r.client.inner.Get(ctx, partyIndexSchemaKey).Result(); err == nil && version == "1" {
		return 0, nil
	} else if err != nil && !errors.Is(err, redis.Nil) {
		return 0, err
	}
	var nonce [24]byte
	if _, err := rand.Read(nonce[:]); err != nil {
		return 0, err
	}
	owner := hex.EncodeToString(nonce[:])
	const lockKey = "party-membership-index-migration-lock"
	locked, err := r.client.inner.SetNX(ctx, lockKey, owner, 5*time.Minute).Result()
	if err != nil {
		return 0, err
	}
	if !locked {
		return 0, errors.New("PARTY_MIGRATION_BUSY: 已有迁移正在执行")
	}
	// 延迟释放只删除本调用者的锁；上下文取消后使用有界独立上下文，不删除后继调用者锁。
	defer func() {
		cleanup, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		_ = r.client.inner.Eval(cleanup, `if redis.call('GET',KEYS[1])==ARGV[1] then return redis.call('DEL',KEYS[1]) end return 0`, []string{lockKey}, owner).Err()
	}()
	records := map[string][]byte{}
	memberOwners := map[string]string{}
	var cursor uint64
	for {
		keys, next, err := r.client.inner.Scan(ctx, cursor, partyKeyPrefix+"*", 100).Result()
		if err != nil {
			return 0, err
		}
		for _, key := range keys {
			if _, seen := records[key]; seen {
				continue
			}
			if len(records) >= maxParties {
				return 0, errors.New("PARTY_MIGRATION_LIMIT: 现存队伍超过扫描上限")
			}
			payload, err := r.client.inner.Get(ctx, key).Bytes()
			if errors.Is(err, redis.Nil) {
				continue
			}
			if err != nil {
				return 0, err
			}
			var value party.Party
			if err := json.Unmarshal(payload, &value); err != nil {
				return 0, err
			}
			if value.ID == "" || key != partyKeyPrefix+value.ID || len(value.Members) > party.MaxMembers {
				return 0, errors.New("PARTY_MIGRATION_CORRUPT: 聚合身份或成员容量无效")
			}
			for id, member := range value.Members {
				if id == "" || id != member.PlayerID {
					return 0, errors.New("PARTY_MIGRATION_CORRUPT: 成员身份不一致")
				}
				if previous := memberOwners[id]; previous != "" && previous != value.ID {
					return 0, fmt.Errorf("PARTY_MIGRATION_DUPLICATE: 玩家同时属于队伍%s和%s", previous, value.ID)
				}
				memberOwners[id] = value.ID
			}
			records[key] = payload
		}
		cursor = next
		if cursor == 0 {
			break
		}
	}
	for key, payload := range records {
		err := r.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			lockOwner, err := tx.Get(ctx, lockKey).Result()
			if err != nil {
				return err
			}
			if lockOwner != owner {
				return errors.New("PARTY_MIGRATION_LEASE_LOST: 迁移锁已失效")
			}
			current, err := tx.Get(ctx, key).Bytes()
			if err != nil {
				return err
			}
			if string(current) != string(payload) {
				return errors.New("PARTY_MIGRATION_SOURCE_CHANGED: 源队伍已变化，旧写进程未停写")
			}
			ttl, err := tx.PTTL(ctx, key).Result()
			if err != nil {
				return err
			}
			if ttl <= 0 {
				return errors.New("PARTY_MIGRATION_TTL_INVALID: 队伍需具备有限有效期")
			}
			var value party.Party
			if err := json.Unmarshal(payload, &value); err != nil {
				return err
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				for id := range value.Members {
					pipe.Set(ctx, partyMemberPrefix+id, value.ID, ttl)
				}
				pipe.Ping(ctx)
				return nil
			})
			return err
		}, lockKey, key)
		if err != nil {
			return 0, err
		}
	}
	err = r.client.inner.Watch(ctx, func(tx *redis.Tx) error {
		current, err := tx.Get(ctx, lockKey).Result()
		if err != nil {
			return err
		}
		if current != owner {
			return errors.New("PARTY_MIGRATION_LEASE_LOST: 迁移锁已失效")
		}
		_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error { pipe.Set(ctx, partyIndexSchemaKey, "1", 0); return nil })
		return err
	}, lockKey)
	if err != nil {
		return 0, err
	}
	return len(records), nil
}

// RequireMembershipIndex（启动检查）未完成维护迁移就拒绝启用正式组队写入，不把空索引误认成所有人无队伍。
func (r *PartyRepository) RequireMembershipIndex(ctx context.Context) error {
	return r.requirePartyIndex(ctx)
}

// ConfigureMembershipIndex（显式维护开关）只有值offline-migrate才执行迁移，其他非空值拒绝，默认只检查。
func (r *PartyRepository) ConfigureMembershipIndex(ctx context.Context, mode string) error {
	if mode = strings.TrimSpace(mode); mode == "offline-migrate" {
		_, err := r.MigrateMembershipIndex(ctx, 100000)
		return err
	} else if mode != "" {
		return errors.New("PARTY_MIGRATION_MODE_INVALID: 只允许显式offline-migrate维护模式")
	}
	return r.RequireMembershipIndex(ctx)
}
