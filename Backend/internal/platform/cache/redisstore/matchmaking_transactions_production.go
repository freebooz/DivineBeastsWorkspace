//go:build productiondeps

// 本文件将匹配队列、队伍名单锁、玩家预留和幂等回执放在一个WATCH/EXEC；只有所有规则通过才发布事件。
// 首版只支持已有单逻辑Redis写域；队列无独立TTL，恢复器必须显式释放活动预留，不能让键自行过期留下幽灵锁。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"encoding/json"
	"errors"
	"github.com/redis/go-redis/v9"
	"sort"
	"time"
)

const matchmakingStateKey = "matchmaking:atomic-state:v1" // 受同一事务保护的有界队列；不能与旧request索引混用。

// ReadMatchmaking返回独立持久值；损坏状态不恢复为空成功，禁止因此重复给玩家报名。
func (r *PartyRepository) ReadMatchmaking(ctx context.Context) (*matchmaking.QueueState, error) {
	if err := r.requirePartyIndex(ctx); err != nil {
		return nil, err
	}
	value := matchmaking.NewQueueState()
	raw, err := r.client.inner.Get(ctx, matchmakingStateKey).Bytes()
	if errors.Is(err, redis.Nil) {
		return value, nil
	}
	if err != nil {
		return nil, err
	}
	if err := json.Unmarshal(raw, value); err != nil {
		return nil, err
	}
	if err := matchapi.ValidateMatchmakingTransaction(&matchapi.MatchmakingTransaction{Queue: value}); err != nil {
		return nil, err
	}
	return value, nil
}

// UpdateMatchmaking回调可在竞争时重试，必须纯内存无外部副作用；活动票据引用的队伍与全部成员索引也进入WATCH。
func (r *PartyRepository) UpdateMatchmaking(ctx context.Context, partyIDs, playerIDs []string, mutation func(*matchapi.MatchmakingTransaction) error) error {
	if err := r.requirePartyIndex(ctx); err != nil {
		return err
	}
	for attempt := 0; attempt < 8; attempt++ {
		err := r.client.inner.Watch(ctx, func(connection *redis.Tx) error {
			state := matchmaking.NewQueueState()
			raw, err := connection.Get(ctx, matchmakingStateKey).Bytes()
			if err != nil && !errors.Is(err, redis.Nil) {
				return err
			}
			if err == nil {
				if err := json.Unmarshal(raw, state); err != nil {
					return err
				}
			}
			before, err := state.Clone()
			if err != nil {
				return err
			}
			parties := map[string]bool{}
			players := map[string]bool{}
			for _, id := range partyIDs {
				if id != "" {
					parties[id] = true
				}
			}
			for _, id := range playerIDs {
				if id != "" {
					players[id] = true
				}
			}
			for _, entry := range state.Entries {
				if !matchmaking.IsTerminal(entry.Phase) {
					if entry.Ticket.PartyID != "" {
						parties[entry.Ticket.PartyID] = true
					}
					for _, id := range entry.Ticket.PartyMemberIDs {
						players[id] = true
					}
				}
			}
			tx := &matchapi.MatchmakingTransaction{Queue: state, Parties: map[string]*party.Party{}, PlayerParties: map[string]string{}}
			originals := map[string]*party.Party{}
			ids := []string{}
			for id := range parties {
				ids = append(ids, id)
			}
			sort.Strings(ids)
			for _, id := range ids {
				key := partyKeyPrefix + id
				if err := connection.Watch(ctx, key).Err(); err != nil {
					return err
				}
				raw, err := connection.Get(ctx, key).Bytes()
				if errors.Is(err, redis.Nil) {
					continue
				}
				if err != nil {
					return err
				}
				var p party.Party
				if err := json.Unmarshal(raw, &p); err != nil {
					return err
				}
				if p.ID != id {
					return errors.New("PARTY_INDEX_INVALID: 聚合身份不一致")
				}
				originals[id] = p.Clone()
				tx.Parties[id] = p.Clone()
				for player := range p.Members {
					players[player] = true
				}
			}
			for player := range players {
				key := partyMemberPrefix + player
				if err := connection.Watch(ctx, key).Err(); err != nil {
					return err
				}
				id, err := connection.Get(ctx, key).Result()
				if err != nil && !errors.Is(err, redis.Nil) {
					return err
				}
				tx.PlayerParties[player] = id
			}
			if err := mutation(tx); err != nil {
				return err
			}
			// go-redis的网络超时配置不代替应用提交边界；回调期间已取消的事务必须在发出EXEC前退出。
			if err := ctx.Err(); err != nil {
				return err
			}
			if err := matchapi.ValidateMatchmakingTransaction(tx); err != nil {
				return err
			}
			changedParties := map[string][]byte{}
			actors := map[string]bool{}
			actors[matchapi.MatchmakingWorkObserver] = true
			for id, next := range tx.Parties {
				previous := originals[id]
				if !matchapi.SamePartyMembers(previous, next) {
					return errors.New("MATCH_TRANSACTION_INVALID: 报名事务不能修改成员归属")
				}
				oldPayload, err := json.Marshal(previous)
				if err != nil {
					return err
				}
				newPayload, err := json.Marshal(next)
				if err != nil {
					return err
				}
				if string(oldPayload) != string(newPayload) {
					changedParties[id] = newPayload
					for player := range next.Members {
						actors[player] = true
					}
				}
			}
			previousPayload, err := json.Marshal(before)
			if err != nil {
				return err
			}
			nextPayload, err := json.Marshal(state)
			if err != nil {
				return err
			}
			queueChanged := string(previousPayload) != string(nextPayload)
			if !queueChanged && len(changedParties) == 0 {
				return nil
			}
			if queueChanged {
				for _, q := range []*matchmaking.QueueState{before, state} {
					for _, entry := range q.Entries {
						for _, actor := range entry.Ticket.PartyMemberIDs {
							actors[actor] = true
						}
					}
				}
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			_, err = connection.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				if queueChanged {
					pipe.Set(ctx, matchmakingStateKey, nextPayload, 0)
				}
				for id, payload := range changedParties {
					pipe.Set(ctx, partyKeyPrefix+id, payload, r.ttl)
					// 锁定队伍续租成员索引；与聚合相同期限，不能先写其中一部分。
					for player := range tx.Parties[id].Members {
						pipe.Expire(ctx, partyMemberPrefix+player, r.ttl)
					}
				}
				for actor := range actors {
					stream := partyChangesPrefix + actor
					pipe.XAdd(ctx, &redis.XAddArgs{Stream: stream, MaxLen: 256, Approx: true, Values: map[string]any{"kind": "matchmaking", "revision": state.Revision}})
					pipe.Publish(ctx, stream, state.Revision)
					pipe.Expire(ctx, stream, 24*time.Hour)
				}
				return nil
			})
			return err
		}, matchmakingStateKey)
		if !errors.Is(err, redis.TxFailedErr) {
			return err
		}
	}
	return errors.New("MATCH_REVISION_CONFLICT: 匹配事务竞争，请重新同步")
}

var _ matchapi.AtomicMatchmakingRepository = (*PartyRepository)(nil)
