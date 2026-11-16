// 本文件提供匹配快照的可恢复事件观察及队列恢复消费者；传输等待不授予Widget业务权威。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"errors"
	"time"
)

// MatchmakingWorkObserver只供内部消费者订阅；与本人数据日志共享同一提交域，不是可认证玩家身份。
const MatchmakingWorkObserver = "$matchmaking-worker"

// ObserveMatchmaking在同一持久日志前后读取本人投影，覆盖首读与续接间隙；截止时间到达由服务端真实迁移收敛。
func (s *Service) ObserveMatchmaking(ctx context.Context, actor, cursor string, wait time.Duration) (matchmaking.Observation, error) {
	if !validMatchID(actor) || !ValidObservationCursor(cursor) || wait < 0 || wait > 20*time.Second {
		return matchmaking.Observation{}, errors.New("MATCH_COMMAND_INVALID: 观察参数无效")
	}
	events, ok := s.parties.(PartyChangeRepository)
	if !ok {
		return matchmaking.Observation{}, errors.New("MATCH_OBSERVATION_UNAVAILABLE: 未装配持久事件")
	}
	owned, cancel := context.WithCancel(ctx)
	token := &partyObserver{cancel: cancel}
	key := "matchmaking/" + actor
	s.observerMu.Lock()
	if s.observers == nil {
		s.observers = map[string]*partyObserver{}
	}
	previous := s.observers[key]
	s.observers[key] = token
	s.observerMu.Unlock()
	if previous != nil {
		previous.cancel()
	}
	defer func() {
		cancel()
		s.observerMu.Lock()
		if s.observers[key] == token {
			delete(s.observers, key)
		}
		s.observerMu.Unlock()
	}()
	read := func() (matchmaking.Observation, error) {
		for attempt := 0; attempt < 8; attempt++ {
			before, err := events.CurrentPartyCursor(owned, actor)
			if err != nil {
				return matchmaking.Observation{}, err
			}
			state, err := s.ReadMatchmakingState(owned, actor)
			if err != nil {
				return matchmaking.Observation{}, err
			}
			after, err := events.CurrentPartyCursor(owned, actor)
			if err != nil {
				return matchmaking.Observation{}, err
			}
			if before == after {
				return matchmaking.Observation{ScopeID: "matchmaking/" + actor, Cursor: after, Snapshot: state}, nil
			}
		}
		return matchmaking.Observation{}, errors.New("MATCH_REVISION_CONFLICT: 状态持续变化，请重新同步")
	}
	first, err := read()
	if err != nil {
		return matchmaking.Observation{}, err
	}
	if cursor == "" {
		first.Changed = true
		return first, nil
	}
	if cursor != first.Cursor {
		_, resync, err := events.WaitPartyChange(owned, actor, cursor, 0)
		first.ResyncRequired = resync
		first.Changed = true
		return first, err
	}
	if wait == 0 {
		return first, nil
	}
	deadlineReached := false
	if first.Snapshot.DeadlineUTC != nil && !matchmaking.IsTerminal(first.Snapshot.State) {
		remaining := time.Until(*first.Snapshot.DeadlineUTC)
		if remaining <= 0 {
			deadlineReached = true
			wait = 0
		} else if remaining < wait {
			wait = remaining
			deadlineReached = true
		}
	}
	next, resync, err := events.WaitPartyChange(owned, actor, cursor, wait)
	if err != nil {
		return matchmaking.Observation{}, err
	}
	if deadlineReached {
		if err := s.AdvanceMatchmaking(owned); err != nil {
			return matchmaking.Observation{}, err
		}
	}
	result, err := read()
	if err != nil {
		return matchmaking.Observation{}, err
	}
	result.ResyncRequired = resync
	result.Changed = next != cursor || deadlineReached
	return result, nil
}

// RunMatchmaking在启动时恢复队列，随后由真实提交事件或最近期限唤醒；控制面副作用只能通过注入的持久步骤执行。
// 单次日志等待最多20秒是断线恢复上限；UI没有常驻Tick/定时GET。任何分配失败都交给onError记录并有界退避。
func (s *Service) RunMatchmaking(ctx context.Context, allocate func(context.Context, matchmaking.Candidate) error, onError func(error)) error {
	repository, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok || allocate == nil || onError == nil {
		return errors.New("MATCH_WORKER_NOT_CONFIGURED: 消费者依赖必须完整")
	}
	events, ok := s.parties.(PartyChangeRepository)
	if !ok {
		return errors.New("MATCH_WORKER_NOT_CONFIGURED: 缺少持久事件")
	}
	for ctx.Err() == nil {
		cursor, err := events.CurrentPartyCursor(ctx, MatchmakingWorkObserver)
		if err != nil {
			return err
		}
		retryAfter := 20 * time.Second
		if err := s.AdvanceMatchmaking(ctx); err != nil {
			onError(err)
			retryAfter = time.Second
		}
		state, err := repository.ReadMatchmaking(ctx)
		if err != nil {
			return err
		}
		for _, candidate := range state.Candidates {
			if candidate.Phase == matchmaking.PhaseAllocating || candidate.Phase == matchmaking.PhaseTransferReady || candidate.Phase == matchmaking.PhaseInMatch || candidate.Phase == matchmaking.PhaseReturning {
				if err := allocate(ctx, candidate); err != nil {
					onError(err)
					retryAfter = time.Second
				}
			}
		}
		now := time.Now().UTC()
		for _, challenge := range state.Challenges {
			if challenge.Phase == matchmaking.PhaseChallengeInvitation {
				remaining := challenge.DeadlineUTC.Sub(now)
				if remaining > 0 && remaining < retryAfter {
					retryAfter = remaining
				}
			}
		}
		for _, entry := range state.Entries {
			if matchmaking.IsTerminal(entry.Phase) || entry.DeadlineUTC.IsZero() {
				continue
			}
			remaining := entry.DeadlineUTC.Sub(now)
			if remaining > 0 && remaining < retryAfter {
				retryAfter = remaining
			}
		}
		if _, _, err := events.WaitPartyChange(ctx, MatchmakingWorkObserver, cursor, retryAfter); err != nil {
			if ctx.Err() != nil {
				return ctx.Err()
			}
			return err
		}
	}
	return ctx.Err()
}
