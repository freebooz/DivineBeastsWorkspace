// 本文件定义匹配事务边界：票据、名单锁、主体回执和每人唯一预留在同一仓储提交。
// mutation只能变更队伍准备/锁及队列，禁止外部调用；生产由单逻辑Redis WATCH/EXEC实现，不回退多次SET补偿。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"encoding/json"
	"errors"
	"sort"
	"time"
)

// MatchmakingTransaction只在仓储回调中借用；调用方不能保留指针或改变成员归属，返回错误不提交。
type MatchmakingTransaction struct {
	Queue         *matchmaking.QueueState // 持久原子队列、预留与候选。
	Parties       map[string]*party.Party // 本事务所涉及的真实聚合，包括活动票据引用的队伍。
	PlayerParties map[string]string       // 受WATCH保护的成员唯一索引，禁止已组队玩家绕开名单单排。
}

// AtomicMatchmakingRepository与Party共用写域，禁止仅票据或仅内存实现“原子报名”。
type AtomicMatchmakingRepository interface {
	ReadMatchmaking(context.Context) (*matchmaking.QueueState, error)
	UpdateMatchmaking(context.Context, []string, []string, func(*MatchmakingTransaction) error) error
}

// ReadMatchmaking返回完整独立值，仅用于受控应用服务；公网只能按认证主体投影。
func (r *MemoryPartyRepository) ReadMatchmaking(ctx context.Context) (*matchmaking.QueueState, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	r.mu.RLock()
	defer r.mu.RUnlock()
	if r.queue == nil {
		return matchmaking.NewQueueState(), nil
	}
	return r.queue.Clone()
}

// UpdateMatchmaking用于测试/明确本地配置的同锁事务；与普通队伍Update共享r.mu，所以成员变更不能穿透报名。
func (r *MemoryPartyRepository) UpdateMatchmaking(ctx context.Context, partyIDs, players []string, mutation func(*MatchmakingTransaction) error) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	original := r.queue
	if original == nil {
		original = matchmaking.NewQueueState()
	}
	queue, err := original.Clone()
	if err != nil {
		return err
	}
	tx := &MatchmakingTransaction{Queue: queue, Parties: map[string]*party.Party{}, PlayerParties: map[string]string{}}
	for id, p := range r.items {
		tx.Parties[id] = p.Clone()
	}
	for player, id := range r.players {
		tx.PlayerParties[player] = id
	}
	before, _ := json.Marshal(original)
	if err := mutation(tx); err != nil {
		return err
	}
	if err := ctx.Err(); err != nil {
		return err
	}
	if err := ValidateMatchmakingTransaction(tx); err != nil {
		return err
	}
	changed := map[string]bool{}
	changedParties := map[string]*party.Party{}
	for id, next := range tx.Parties {
		previous := r.items[id]
		if previous == nil {
			return errors.New("MATCH_TRANSACTION_INVALID: 不能在报名事务创建队伍")
		}
		if !SamePartyMembers(previous, next) {
			return errors.New("MATCH_TRANSACTION_INVALID: 报名事务不能改变成员归属")
		}
		oldBytes, _ := json.Marshal(previous)
		newBytes, _ := json.Marshal(next)
		if string(oldBytes) != string(newBytes) {
			changedParties[id] = next.Clone()
			if previous.Revision != next.Revision {
				for player := range next.Members {
					changed[player] = true
				}
			}
		}
	}
	// 全部队伍及队列通过验证后才提交，禁止遍历中途发现错误留下部分已发布更新。
	for id, next := range changedParties {
		previous := r.items[id]
		r.items[id] = next
		r.publishPartyChangesLocked(previous, next)
	}
	after, _ := json.Marshal(queue)
	if string(before) != string(after) {
		// 预留被释放的玩家也要通知，不能只扫描新状态中的活跃成员。
		for _, state := range []*matchmaking.QueueState{original, queue} {
			for _, challenge := range state.Challenges {
				for _, player := range challenge.PlayerIDs() {
					changed[player] = true
				}
			}
			for _, entry := range state.Entries {
				for _, player := range entry.Ticket.PartyMemberIDs {
					changed[player] = true
				}
			}
		}
		r.queue = queue
		notification := &party.Party{Members: map[string]party.Member{}}
		for player := range changed {
			notification.Members[player] = party.Member{PlayerID: player}
		}
		r.publishPartyChangesLocked(nil, notification)
	}
	return nil
}

// SamePartyMembers阻止队列事务绕开成员唯一索引与邀请授权；准备/模式/锁定变化不改变成员身份。
func SamePartyMembers(before, after *party.Party) bool {
	if before == nil || after == nil || before.ID != after.ID || len(before.Members) != len(after.Members) {
		return false
	}
	for player := range before.Members {
		if _, found := after.Members[player]; !found {
			return false
		}
	}
	return true
}

// ValidateMatchmakingTransaction复核活动票据与预留一一对应，防止恢复或新命令写出幽灵锁；失败不会部分提交。
func ValidateMatchmakingTransaction(tx *MatchmakingTransaction) error {
	expected := map[string]string{}
	if tx.Queue == nil || tx.Queue.Entries == nil || tx.Queue.Reservations == nil || tx.Queue.Candidates == nil || tx.Queue.Receipts == nil || tx.Queue.Contexts == nil || tx.Queue.Challenges == nil || tx.Queue.Revision < 0 || len(tx.Queue.Entries) > 4*matchmaking.MaxActiveTickets || len(tx.Queue.Challenges) > 4*matchmaking.MaxActiveTickets {
		return errors.New("MATCH_TRANSACTION_INVALID: 队列缺失或超限")
	}
	for id, entry := range tx.Queue.Entries {
		if id != entry.Ticket.TicketID {
			return errors.New("MATCH_TRANSACTION_INVALID: 票据键与身份不一致")
		}
		if matchmaking.IsTerminal(entry.Phase) {
			continue
		}
		for _, player := range entry.Ticket.PartyMemberIDs {
			if expected[player] != "" {
				return errors.New("MATCH_PLAYER_RESERVED: 多条活动票据包含同一玩家")
			}
			expected[player] = id
		}
	}
	for id, c := range tx.Queue.Challenges {
		if id != c.ID || c.Revision < 1 {
			return errors.New("MATCH_TRANSACTION_INVALID: 邀请键或修订不一致")
		}
		if c.Phase != matchmaking.PhaseChallengeInvitation {
			continue
		}
		if _, exists := tx.Queue.Entries[id]; exists {
			return errors.New("MATCH_TRANSACTION_INVALID: 邀请与票据身份冲突")
		}
		mode, ok := matchmaking.ArenaModeByID(c.ArenaModeID)
		if !ok || len(c.TeamAPlayerIDs) != mode.TeamSize || len(c.TeamBPlayerIDs) != mode.TeamSize || c.RegionID == "" || c.DeadlineUTC.IsZero() {
			return errors.New("MATCH_TRANSACTION_INVALID: 邀请模式名单无效")
		}
		owner, target := false, false
		for i, members := range [][]string{c.TeamAPlayerIDs, c.TeamBPlayerIDs} {
			for _, player := range members {
				if player == "" || expected[player] != "" {
					return errors.New("MATCH_PLAYER_RESERVED: 邀请与活动资格重复")
				}
				expected[player] = id
				owner = owner || (i == 0 && player == c.OwnerPlayerID)
				target = target || (i == 1 && player == c.TargetPlayerID)
			}
		}
		if !owner || !target {
			return errors.New("MATCH_TRANSACTION_INVALID: 邀请队长不在各自名单")
		}
	}
	if len(expected) != len(tx.Queue.Reservations) {
		return errors.New("MATCH_TRANSACTION_INVALID: 玩家预留数量不一致")
	}
	for player, id := range expected {
		if tx.Queue.Reservations[player] != id {
			return errors.New("MATCH_TRANSACTION_INVALID: 玩家预留归属不一致")
		}
	}
	return nil
}

// unlockFinishedParties只在没有有效报名/比赛后释放名单；不依赖过期TTL或本地页面关闭。
func unlockFinishedParties(tx *MatchmakingTransaction) {
	active := map[string]bool{}
	known := map[string]bool{}
	for _, entry := range tx.Queue.Entries {
		if entry.Ticket.PartyID != "" {
			known[entry.Ticket.PartyID] = true
		}
		if !matchmaking.IsTerminal(entry.Phase) && entry.Ticket.PartyID != "" {
			active[entry.Ticket.PartyID] = true
		}
	}
	for id, p := range tx.Parties {
		if p.RosterLocked && known[id] && !active[id] {
			p.UnlockRoster()
		}
	}
}

// enqueueAtomically校验可信名单与同模式准备，然后在单次事务创建票据、锁和所有预留；重试只返回原票据。
func (s *Service) enqueueAtomically(ctx context.Context, actor string, input CreateMatchmakingTicketInput) (matchmaking.Ticket, error) {
	repository, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return matchmaking.Ticket{}, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 未装配原子报名仓储")
	}
	requestKey := scopedOperationKey(actor, "matchmaking", input.ClientRequestID)
	fingerprint := fingerprintOf(input)
	current, err := repository.ReadMatchmaking(ctx)
	if err != nil {
		return matchmaking.Ticket{}, err
	}
	if receipt, found := current.Receipts[requestKey]; found && receipt.ExpiresAtUTC.After(time.Now()) {
		if receipt.Fingerprint != fingerprint {
			return matchmaking.Ticket{}, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 同键对应不同报名参数")
		}
		if entry, exists := current.Entries[receipt.TicketID]; exists {
			return entry.Ticket, nil
		}
	}
	ticketID := s.nextID("matchmaking")
	var result matchmaking.Ticket
	err = repository.UpdateMatchmaking(ctx, []string{input.PartyID}, []string{actor}, func(tx *MatchmakingTransaction) error {
		now := time.Now().UTC()
		tx.Queue.Prune(now)
		if receipt, found := tx.Queue.Receipts[requestKey]; found {
			if receipt.Fingerprint != fingerprint {
				return errors.New("MATCH_IDEMPOTENCY_CONFLICT: 同键对应不同报名参数")
			}
			entry, exists := tx.Queue.Entries[receipt.TicketID]
			if !exists {
				return errors.New("MATCH_STATE_INVALID: 幂等回执没有报名")
			}
			result = entry.Ticket
			return nil
		}
		memberIDs := []string{actor}
		if input.PartyID == "" {
			if tx.PlayerParties[actor] != "" {
				return errors.New("MATCH_PARTY_REQUIRED: 已组队玩家必须使用整队报名")
			}
		} else {
			p := tx.Parties[input.PartyID]
			if p == nil {
				return ErrPartyNotFound
			}
			if p.LeaderPlayerID != actor {
				return errors.New("PARTY_NOT_LEADER: 只有队长可开始匹配")
			}
			if p.RosterLocked {
				return errors.New("PARTY_ROSTER_LOCKED: 名单已锁定")
			}
			if len(p.Members) > 1 {
				if p.SelectedArenaModeID != input.ArenaModeID {
					return errors.New("MATCH_PARTY_NOT_READY: 必须对本模式重新准备")
				}
				for _, member := range p.Members {
					if !member.Ready || !member.Online {
						return errors.New("MATCH_PARTY_NOT_READY: 必须全部成员准备且在线")
					}
				}
			} else if err := p.SelectArenaMode(actor, input.ArenaModeID); err != nil {
				return err
			}
			memberIDs = memberIDs[:0]
			for player := range p.Members {
				if tx.PlayerParties[player] != p.ID {
					return errors.New("PARTY_INDEX_INVALID: 名单索引不一致")
				}
				memberIDs = append(memberIDs, player)
			}
			sort.Strings(memberIDs)
		}
		ticket, err := matchmaking.NewTicket(matchmaking.CreateTicketRequest{TicketID: ticketID, GameID: "divine-beasts", ArenaModeID: input.ArenaModeID, PartyID: input.PartyID, PartyMemberIDs: memberIDs, Region: input.RegionID})
		if err != nil {
			return err
		}
		ticket.RequestFingerprint = fingerprint
		if err := tx.Queue.Enqueue(matchmaking.QueueEntry{Ticket: ticket, OwnerPlayerID: actor, ClientRequestID: input.ClientRequestID, RequestFingerprint: fingerprint, Revision: 1, Phase: matchmaking.PhaseSearching}, now); err != nil {
			return err
		}
		if p := tx.Parties[input.PartyID]; p != nil {
			p.LockRoster()
		}
		tx.Queue.Receipts[requestKey] = matchmaking.QueueReceipt{Fingerprint: fingerprint, TicketID: ticketID, ExpiresAtUTC: now.Add(matchmaking.TerminalRetention)}
		result = ticket
		return nil
	})
	return result, err
}
