//go:build productiondeps

// 本文件为Match应用提供单逻辑Redis写域的普通队伍事务；同一EXEC提交名单、唯一成员索引、邀请收件箱和幂等回执。
// 不兼容跨槽Cluster事务；旧部署必须在写入停机后显式迁移成员索引，缺少迁移标记时失败关闭。
package redisstore

import (
	"context"
	"encoding/json"
	"errors"
	"strconv"
	"time"

	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"github.com/redis/go-redis/v9"
)

const (
	partyMemberPrefix    = "party-member:"                 // 玩家到队伍的唯一索引，与旧party:聚合键隔离。
	partyInboxPrefix     = "party-inbox:"                  // 按到期时间排序的本人邀请来源队伍，最多20项。
	partyOperationPrefix = "party-operation:"              // 已认证主体域命令回执；键已由应用层摘要。
	partyIndexSchemaKey  = "party-membership-index-schema" // 值1表示已完成停写索引迁移；不是自动成功标记。
)

// requirePartyIndex（生产写入前置）不支持的部署或未迁移旧数据不得默默退化到非原子SET。
func (r *PartyRepository) requirePartyIndex(ctx context.Context) error {
	if _, cluster := r.client.inner.(*redis.ClusterClient); cluster {
		return errors.New("PARTY_CLUSTER_UNSUPPORTED: 队伍事务要求单逻辑Redis写域")
	}
	version, err := r.client.inner.Get(ctx, partyIndexSchemaKey).Result()
	if errors.Is(err, redis.Nil) || (err == nil && version != "1") {
		return errors.New("PARTY_INDEX_MIGRATION_REQUIRED: 请先停写并迁移普通队伍成员索引")
	}
	return err
}

// GetByPlayerID（按唯一索引读队伍）复核成员；无队伍与存储失败分开返回，不根据客户端声明推断归属。
func (r *PartyRepository) GetByPlayerID(ctx context.Context, playerID string) (*party.Party, error) {
	if err := r.requirePartyIndex(ctx); err != nil {
		return nil, err
	}
	id, err := r.client.inner.Get(ctx, partyMemberPrefix+playerID).Result()
	if errors.Is(err, redis.Nil) {
		return nil, matchapi.ErrPartyNotFound
	}
	if err != nil {
		return nil, err
	}
	value, err := r.Get(ctx, id)
	if err != nil {
		return nil, err
	}
	if _, exists := value.Members[playerID]; !exists {
		return nil, errors.New("PARTY_INDEX_INVALID: 唯一成员索引与聚合不一致")
	}
	return value, nil
}

// Update（乐观事务更新）mutation只作用于副本，失败不提交；最多4次存储竞争重试，外部陈旧Revision立即冲突。
func (r *PartyRepository) Update(ctx context.Context, id string, revision int64, operationKey, fingerprint string, mutation func(*party.Party) error) (*party.Party, error) {
	if err := r.requirePartyIndex(ctx); err != nil {
		return nil, err
	}
	return r.updateParty(ctx, id, revision, operationKey, fingerprint, false, mutation)
}

// saveNewParty（只创建）保留旧Save端口身份，但拒绝无条件覆盖已有队伍；普通修改必须使用Update。
func (r *PartyRepository) saveNewParty(ctx context.Context, value *party.Party) error {
	if value == nil || value.ID == "" || len(value.Members) == 0 {
		return errors.New("PARTY_INVALID: 新队伍无效")
	}
	if err := r.requirePartyIndex(ctx); err != nil {
		return err
	}
	_, err := r.updateParty(ctx, value.ID, 0, "", "", true, func(next *party.Party) error { *next = *value.Clone(); return nil })
	return err
}

// updateParty（原子提交核心）读取和WATCH包含所有被改成员与收件箱；不得把回执或部分索引提前写出。
func (r *PartyRepository) updateParty(ctx context.Context, id string, revision int64, key, fingerprint string, creating bool, mutation func(*party.Party) error) (*party.Party, error) {
	var result *party.Party
	watchKeys := []string{partyKeyPrefix + id, matchmakingStateKey}
	if key != "" {
		watchKeys = append(watchKeys, partyOperationPrefix+key)
	}
	for attempt := 0; attempt < 4; attempt++ {
		err := r.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			if key != "" {
				raw, err := tx.Get(ctx, partyOperationPrefix+key).Bytes()
				if err == nil {
					var receipt matchapi.PartyOperationReceipt
					if err := json.Unmarshal(raw, &receipt); err != nil {
						return err
					}
					if receipt.Fingerprint != fingerprint || receipt.Result == nil {
						return errors.New("PARTY_IDEMPOTENCY_CONFLICT: 命令键已对应其他意图")
					}
					result = receipt.Result.Clone()
					return nil
				}
				if !errors.Is(err, redis.Nil) {
					return err
				}
			}
			current := &party.Party{ID: id}
			raw, err := tx.Get(ctx, partyKeyPrefix+id).Bytes()
			if err == nil {
				if creating {
					return errors.New("PARTY_EXISTS: 队伍身份已存在")
				}
				if err := json.Unmarshal(raw, current); err != nil {
					return err
				}
			} else if !errors.Is(err, redis.Nil) {
				return err
			} else if !creating {
				return matchapi.ErrPartyNotFound
			}
			if current.Revision != revision {
				return errors.New("PARTY_REVISION_CONFLICT: 队伍已变化")
			}
			next := current.Clone()
			if err := mutation(next); err != nil {
				return err
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			if next.ID != id || next.Revision < current.Revision || len(next.Members) > party.MaxMembers {
				return errors.New("PARTY_MUTATION_INVALID: 聚合身份、版本或容量无效")
			}
			members := map[string]bool{}
			inboxes := map[string]bool{}
			for player := range current.Members {
				members[player] = true
			}
			for player := range next.Members {
				members[player] = true
			}
			for _, request := range current.MembershipRequests {
				if request.Kind == party.MembershipInvitation {
					inboxes[request.PlayerID] = true
				}
			}
			for _, request := range next.MembershipRequests {
				if request.Kind == party.MembershipInvitation {
					inboxes[request.PlayerID] = true
				}
			}
			keys := []string{}
			for player := range members {
				keys = append(keys, partyMemberPrefix+player)
			}
			for player := range inboxes {
				keys = append(keys, partyInboxPrefix+player)
			}
			if len(keys) != 0 {
				if err := tx.Watch(ctx, keys...).Err(); err != nil {
					return err
				}
			}
			for player := range current.Members {
				owner, err := tx.Get(ctx, partyMemberPrefix+player).Result()
				if err != nil {
					return err
				}
				if owner != id {
					return errors.New("PARTY_INDEX_INVALID: 旧成员索引归属不一致，禁止删除其他队伍索引")
				}
			}
			for player := range next.Members {
				if _, retained := current.Members[player]; !retained {
					queueRaw, queueErr := tx.Get(ctx, matchmakingStateKey).Bytes()
					if queueErr != nil && !errors.Is(queueErr, redis.Nil) {
						return queueErr
					}
					if queueErr == nil {
						queue := matchmaking.NewQueueState()
						if err := json.Unmarshal(queueRaw, queue); err != nil {
							return err
						}
						if queue.Reservations[player] != "" {
							return errors.New("MATCH_PLAYER_RESERVED: 目标玩家已有活动匹配")
						}
					}
				}
				owner, err := tx.Get(ctx, partyMemberPrefix+player).Result()
				if err != nil && !errors.Is(err, redis.Nil) {
					return err
				}
				if owner != "" && owner != id {
					// 过期聚合的旧索引可被覆盖；先WATCH原聚合，避免并发续租时错误抢占。
					if err := tx.Watch(ctx, partyKeyPrefix+owner).Err(); err != nil {
						return err
					}
					exists, err := tx.Exists(ctx, partyKeyPrefix+owner).Result()
					if err != nil {
						return err
					}
					if exists != 0 {
						return errors.New("PARTY_ALREADY_MEMBER: 玩家已在其他队伍")
					}
				}
			}
			now := time.Now().UTC()
			deadlines := map[string]time.Time{}
			for _, request := range next.MembershipRequests {
				if request.Kind == party.MembershipInvitation && request.State == party.MembershipRequestPending && now.Before(request.ExpiresAtUTC) {
					deadlines[request.PlayerID] = request.ExpiresAtUTC
				}
			}
			for player := range deadlines {
				entries, err := tx.ZRangeByScore(ctx, partyInboxPrefix+player, &redis.ZRangeBy{Min: strconv.FormatInt(now.UnixMilli(), 10), Max: "+inf"}).Result()
				if err != nil {
					return err
				}
				present := false
				for _, partyID := range entries {
					if partyID == id {
						present = true
					}
				}
				if !present && len(entries) >= 20 {
					return errors.New("PARTY_REQUEST_LIMIT: 目标玩家的待处理邀请过多")
				}
			}
			payload, err := json.Marshal(next)
			if err != nil {
				return err
			}
			var receipt []byte
			if key != "" {
				receipt, err = json.Marshal(matchapi.PartyOperationReceipt{Fingerprint: fingerprint, Result: next})
				if err != nil {
					return err
				}
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				// 与名单、收件箱同一EXEC提交通知；新旧参与者都能观察离队、拒绝和取消，避免只通知当前成员。
				observers := map[string]bool{}
				observers[matchapi.MatchmakingWorkObserver] = true
				for player := range members {
					observers[player] = true
				}
				for _, aggregate := range []*party.Party{current, next} {
					for _, request := range aggregate.MembershipRequests {
						observers[request.PlayerID] = true
						observers[request.RequestedByPlayerID] = true
					}
				}
				if current.Revision != next.Revision {
					for player := range observers {
						if player == "" {
							continue
						}
						stream := partyChangesPrefix + player
						pipe.XAdd(ctx, &redis.XAddArgs{Stream: stream, MaxLen: 256, Approx: true, Values: map[string]any{"partyId": id, "revision": next.Revision}})
						pipe.Publish(ctx, stream, next.Revision)
						pipe.Expire(ctx, stream, r.ttl)
					}
				}
				pipe.Set(ctx, partyKeyPrefix+id, payload, r.ttl)
				for player := range members {
					if _, retained := next.Members[player]; retained {
						pipe.Set(ctx, partyMemberPrefix+player, id, r.ttl)
					} else {
						pipe.Del(ctx, partyMemberPrefix+player)
					}
				}
				for player := range inboxes {
					inbox := partyInboxPrefix + player
					pipe.ZRemRangeByScore(ctx, inbox, "-inf", strconv.FormatInt(now.UnixMilli(), 10))
					if deadline, pending := deadlines[player]; pending {
						pipe.ZAdd(ctx, inbox, redis.Z{Score: float64(deadline.UnixMilli()), Member: id})
						pipe.Expire(ctx, inbox, 2*party.ApplicationLifetime)
					} else {
						pipe.ZRem(ctx, inbox, id)
					}
				}
				if key != "" {
					pipe.Set(ctx, partyOperationPrefix+key, receipt, 15*time.Minute)
				}
				return nil
			})
			if err == nil {
				result = next.Clone()
			}
			return err
		}, watchKeys...)
		if !errors.Is(err, redis.TxFailedErr) {
			return result, err
		}
	}
	return nil, errors.New("PARTY_REVISION_CONFLICT: 并发竞争，请读取最新状态后重试")
}

// GetInvitations（真实邀请收件箱）仅返回本人未到期邀请，过期聚合被跳过，其他Redis错误必须上报。
func (r *PartyRepository) GetInvitations(ctx context.Context, playerID string, now time.Time) ([]matchapi.PartyInvitationView, error) {
	if err := r.requirePartyIndex(ctx); err != nil {
		return nil, err
	}
	ids, err := r.client.inner.ZRangeByScore(ctx, partyInboxPrefix+playerID, &redis.ZRangeBy{Min: strconv.FormatInt(now.UnixMilli(), 10), Max: "+inf", Count: 20}).Result()
	if err != nil {
		return nil, err
	}
	result := []matchapi.PartyInvitationView{}
	for _, id := range ids {
		value, err := r.Get(ctx, id)
		if errors.Is(err, matchapi.ErrPartyNotFound) {
			continue
		}
		if err != nil {
			return nil, err
		}
		for _, request := range value.MembershipRequests {
			if request.Kind == party.MembershipInvitation && request.PlayerID == playerID && request.State == party.MembershipRequestPending && now.Before(request.ExpiresAtUTC) {
				result = append(result, matchapi.PartyInvitationView{PartyID: id, RequestID: request.ID, InviterPlayerID: request.RequestedByPlayerID, MemberCount: len(value.Members), Revision: value.Revision, ExpiresAtUTC: request.ExpiresAtUTC})
			}
		}
	}
	return result, nil
}

var _ matchapi.AtomicPartyRepository = (*PartyRepository)(nil)
