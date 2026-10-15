// 本文件是普通队伍观察应用服务：一次完整快照后阻塞等待仓储事件或最近业务截止时间。
// 不使用周期Get轮询；仓储必须把事件与领域更新一并提交。失去连接只取消本地等待，不回滚业务。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"
)

// PartyChangeRepository（可恢复事件日志）CurrentCursor/WaitPartyChange属于同一持久写域；等待必须响应Context取消。
type PartyChangeRepository interface {
	CurrentPartyCursor(context.Context, string) (string, error)
	WaitPartyChange(context.Context, string, string, time.Duration) (string, bool, error)
}

// ValidObservationCursor校验有界Redis Stream兼容游标；空只用于首次读取，拒绝控制字符、符号和整数溢出。
func ValidObservationCursor(cursor string) bool {
	if cursor == "" {
		return true
	}
	if len(cursor) > 43 {
		return false
	}
	parts := strings.Split(cursor, "-")
	if len(parts) != 2 {
		return false
	}
	for _, part := range parts {
		if part == "" || (len(part) > 1 && part[0] == '0') {
			return false
		}
		for _, c := range part {
			if c < '0' || c > '9' {
				return false
			}
		}
		if _, err := strconv.ParseUint(part, 10, 64); err != nil {
			return false
		}
	}
	return true
}

// ObserveParty使用双读游标建立不丢事件的快照；最多20秒等待，每主体只保留一个活动观察。
// 收件箱到期属于真实服务器时钟事件，以一次deadline等待结束通知，不扫描业务状态。
func (s *Service) ObserveParty(ctx context.Context, actor, cursor string, wait time.Duration) (party.Observation, error) {
	if err := ctx.Err(); err != nil {
		return party.Observation{}, err
	}
	if strings.TrimSpace(actor) == "" || len(actor) > 128 || !ValidObservationCursor(cursor) || wait < 0 || wait > 20*time.Second {
		return party.Observation{}, errors.New("PARTY_COMMAND_INVALID: 观察主体、游标或等待期限无效")
	}
	events, ok := s.parties.(PartyChangeRepository)
	if !ok {
		return party.Observation{}, errors.New("PARTY_OBSERVATION_UNAVAILABLE: 仓储未装配事件日志")
	}
	// 先取消旧代次再安装新代次，结束时只移除自己，避免旧请求的defer删除新订阅。
	owned, cancel := context.WithCancel(ctx)
	token := &partyObserver{cancel: cancel}
	s.observerMu.Lock()
	if s.observers == nil {
		s.observers = map[string]*partyObserver{}
	}
	old := s.observers[actor]
	s.observers[actor] = token
	s.observerMu.Unlock()
	if old != nil {
		old.cancel()
	}
	defer func() {
		cancel()
		s.observerMu.Lock()
		if s.observers[actor] == token {
			delete(s.observers, actor)
		}
		s.observerMu.Unlock()
	}()
	read := func() (party.Observation, error) {
		for attempt := 0; attempt < 8; attempt++ {
			before, err := events.CurrentPartyCursor(owned, actor)
			if err != nil {
				return party.Observation{}, err
			}
			view, err := s.ReadPartyState(owned, actor)
			if err != nil {
				return party.Observation{}, err
			}
			inbox, err := s.GetPartyInvitations(owned, actor)
			if err != nil {
				return party.Observation{}, err
			}
			after, err := events.CurrentPartyCursor(owned, actor)
			if err != nil {
				return party.Observation{}, err
			}
			if before == after {
				return party.Observation{ScopeID: "party/" + actor, Cursor: after, ServerTimeUTC: time.Now().UTC(), Snapshot: view, Invitations: inbox}, nil
			}
		}
		return party.Observation{}, errors.New("PARTY_REVISION_CONFLICT: 观察快照持续变化，请重新同步")
	}
	initial, err := read()
	if err != nil {
		return party.Observation{}, err
	}
	if cursor == "" {
		initial.Changed = true
		return initial, nil
	}
	if cursor != initial.Cursor {
		_, resync, err := events.WaitPartyChange(owned, actor, cursor, 0)
		initial.ResyncRequired = resync
		initial.Changed = true
		return initial, err
	}
	if wait == 0 {
		return initial, nil
	}
	// 此时间只用于服务器产生过期通知；客户端没有据显示归零接受邀请的资格。
	expires := false
	for _, inv := range initial.Invitations {
		if remaining := time.Until(inv.ExpiresAtUTC); remaining > 0 && remaining <= wait {
			wait = remaining
			expires = true
		}
	}
	for _, request := range initial.Snapshot.Requests {
		if request.State == party.MembershipRequestPending {
			if remaining := time.Until(request.ExpiresAtUTC); remaining > 0 && remaining <= wait {
				wait = remaining
				expires = true
			}
		}
	}
	next, resync, err := events.WaitPartyChange(owned, actor, cursor, wait)
	if err != nil {
		return party.Observation{}, err
	}
	result, err := read()
	if err != nil {
		return party.Observation{}, err
	}
	result.ResyncRequired = resync
	result.Changed = expires || next != cursor
	return result, nil
}

// partyObserver只拥有一次HTTP/RPC等待的取消，认证和队伍状态由领域与仓储拥有。
type partyObserver struct{ cancel context.CancelFunc }

// partyMemoryChanges是测试/本地仓储事件日志；revision形成Redis兼容游标，channel关闭唤醒全部观察者。
type partyMemoryChanges struct {
	revision uint64
	changed  chan struct{}
}

// publishPartyChangesLocked必须在提交持有r.mu时调用；同时唤醒旧/新成员和请求双方，离队者也能收到清空快照。
func (r *MemoryPartyRepository) publishPartyChangesLocked(previous, next *party.Party) {
	actors := map[string]bool{}
	for _, value := range []*party.Party{previous, next} {
		if value == nil {
			continue
		}
		for id := range value.Members {
			actors[id] = true
		}
		for _, request := range value.MembershipRequests {
			actors[request.PlayerID] = true
			actors[request.RequestedByPlayerID] = true
		}
	}
	if r.changes == nil {
		r.changes = map[string]*partyMemoryChanges{}
	}
	for actor := range actors {
		if actor == "" {
			continue
		}
		entry := r.changes[actor]
		if entry == nil {
			entry = &partyMemoryChanges{changed: make(chan struct{})}
			r.changes[actor] = entry
		}
		entry.revision++
		close(entry.changed)
		entry.changed = make(chan struct{})
	}
}

// CurrentPartyCursor返回真实事件版本；从未变化的本人作用域使用0-0，而非伪造事件。
func (r *MemoryPartyRepository) CurrentPartyCursor(ctx context.Context, actor string) (string, error) {
	if err := ctx.Err(); err != nil {
		return "", err
	}
	r.mu.RLock()
	defer r.mu.RUnlock()
	if v := r.changes[actor]; v != nil {
		return fmt.Sprintf("%d-0", v.revision), nil
	}
	return "0-0", nil
}

// WaitPartyChange不周期唤醒；在锁内登记通知后才解锁，消除检查和订阅间隙。未知游标要求完整重同步。
func (r *MemoryPartyRepository) WaitPartyChange(ctx context.Context, actor, cursor string, wait time.Duration) (string, bool, error) {
	if err := ctx.Err(); err != nil {
		return "", false, err
	}
	r.mu.Lock()
	if r.changes == nil {
		r.changes = map[string]*partyMemoryChanges{}
	}
	v := r.changes[actor]
	if v == nil {
		v = &partyMemoryChanges{changed: make(chan struct{})}
		r.changes[actor] = v
	}
	current := fmt.Sprintf("%d-0", v.revision)
	changed := v.changed
	revision := v.revision
	r.mu.Unlock()
	if current != cursor {
		wanted, err := strconv.ParseUint(strings.Split(cursor, "-")[0], 10, 64)
		return current, err != nil || wanted > revision || revision-wanted > 256, nil
	}
	if wait <= 0 {
		return current, false, nil
	}
	timer := time.NewTimer(wait)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return "", false, ctx.Err()
	case <-changed:
		next, err := r.CurrentPartyCursor(ctx, actor)
		return next, false, err
	case <-timer.C:
		return current, false, nil
	}
}
