// 本文件仅供可信比赛编排器读取固定候选、提交已持久化的阶段；不提供公网绕过确认的命令。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"errors"
	"time"
)

// ReadCandidateContext只读取已确认候选对应的可信角色/原队伍，编排器不能从UI正文构建权威名单。
func (s *Service) ReadCandidateContext(ctx context.Context, id string) (matchmaking.Candidate, map[string]matchmaking.ActorContext, map[string]string, error) {
	repo, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return matchmaking.Candidate{}, nil, nil, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 缺少事务仓储")
	}
	queue, err := repo.ReadMatchmaking(ctx)
	if err != nil {
		return matchmaking.Candidate{}, nil, nil, err
	}
	candidate, found := queue.Candidates[id]
	if !found {
		return matchmaking.Candidate{}, nil, nil, errors.New("MATCH_NOT_FOUND: 候选不存在")
	}
	contexts := map[string]matchmaking.ActorContext{}
	parties := map[string]string{}
	for _, slot := range candidate.Roster {
		value, found := queue.Contexts[slot.PlayerID]
		if !found {
			return matchmaking.Candidate{}, nil, nil, errors.New("MATCH_CHARACTER_REQUIRED: 已确认角色上下文缺失")
		}
		contexts[slot.PlayerID] = value
		entry := queue.Entries[queue.Reservations[slot.PlayerID]]
		parties[slot.PlayerID] = entry.Ticket.PartyID
	}
	return candidate, contexts, parties, nil
}

// CommitCandidatePhase只接受持久编排器的向前投影；终态必须由其先完成撤销/迁出，才释放全部预留和队伍锁。
func (s *Service) CommitCandidatePhase(ctx context.Context, id, phase, reason string, deadline time.Time) error {
	repo, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return errors.New("MATCH_ATOMIC_STORE_REQUIRED: 缺少事务仓储")
	}
	return repo.UpdateMatchmaking(ctx, nil, nil, func(tx *MatchmakingTransaction) error {
		current, found := tx.Queue.Candidates[id]
		if !found {
			return errors.New("MATCH_NOT_FOUND: 候选不存在")
		}
		if current.Phase == phase {
			return nil
		}
		// PG已提交而投影响应丢失时可跨过中间展示阶段；不倒退或重开终态。
		rank := map[string]int{matchmaking.PhaseAllocating: 1, matchmaking.PhaseTransferReady: 2, matchmaking.PhaseInMatch: 3, matchmaking.PhaseReturning: 4, matchmaking.PhaseCompleted: 5, matchmaking.PhaseFailed: 5}
		if rank[phase] == 0 || rank[current.Phase] == 0 || rank[phase] <= rank[current.Phase] || matchmaking.IsTerminal(current.Phase) {
			return errors.New("MATCH_PHASE_CONFLICT: 编排投影不能倒退")
		}
		now := time.Now().UTC()
		if matchmaking.IsTerminal(phase) {
			if err := tx.Queue.FinishCandidate(id, phase, reason, now); err != nil {
				return err
			}
			unlockFinishedParties(tx)
			for _, p := range tx.Parties {
				if p.ReadyCheckID != "" && tx.Queue.Reservations[p.LeaderPlayerID] == "" {
					p.CancelReadyCheck()
				}
			}
			return nil
		}
		current.Phase = phase
		current.Revision++
		current.DeadlineUTC = deadline
		current.UpdatedAtUTC = now
		tx.Queue.Candidates[id] = current
		for _, ticketID := range current.TicketIDs {
			entry := tx.Queue.Entries[ticketID]
			entry.Phase = phase
			entry.Ticket.State = phase
			entry.Revision++
			entry.DeadlineUTC = deadline
			entry.ReasonCode = reason
			entry.UpdatedAtUTC = now
			tx.Queue.Entries[ticketID] = entry
		}
		tx.Queue.Revision++
		return nil
	})
}
