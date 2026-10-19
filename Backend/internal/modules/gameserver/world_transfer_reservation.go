// 世界返回意图先持久选定目标，再以TicketID预留；未知签票响应不能再次加计容量或自动换实例。
// Registry拥有本进程在线容量，可信Boot变化使旧目标失效；共享意图与秘密由控制应用仓储拥有。
package gameserver

import (
	"errors"
	"sort"
	"time"
)

// worldHeartbeatLease限制世界分配使用的控制面心跳有效期。超过30秒或从未心跳的实例
// 即使旧状态仍为Ready/Active也不可新签票；已有会话不会因此被此选择逻辑主动断开。
const worldHeartbeatLease = 30 * time.Second

// hasFreshWorldHeartbeat使用本次请求的控制面时钟，保持预览和预留的时间边界一致。
func hasFreshWorldHeartbeat(s Instance, now time.Time) bool {
	return !s.LastHeartbeat.IsZero() && now.Sub(s.LastHeartbeat) <= worldHeartbeatLease
}

type worldTransferReservation struct {
	ServerID, BootID       string
	Slots                  int
	ExpiresAt, RetainUntil time.Time
	Finished               bool
}

// expireWorldTransfersLocked只回收过期且未消费的本意图预留；墓碑覆盖10分钟重放窗口，不误减其他预留。
func (r *Registry) expireWorldTransfersLocked(now time.Time) {
	for id, v := range r.worldTransfers {
		if !v.Finished && !now.Before(v.ExpiresAt) {
			if s, ok := r.instances[v.ServerID]; ok && s.ServerBootID == v.BootID {
				s.ReservedPlayers = max(0, s.ReservedPlayers-v.Slots)
				r.instances[v.ServerID] = s
			}
			v.Finished = true
			r.worldTransfers[id] = v
		}
		if !now.Before(v.RetainUntil) {
			delete(r.worldTransfers, id)
		}
	}
}

// PreviewWorldAllocation仅选择快照，绝不占容量；可被Redis WATCH重试，之后必须条件确认同Boot同目标。
func (r *Registry) PreviewWorldAllocation(req AllocationRequest, now time.Time) (Instance, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.expireWorldTransfersLocked(now)
	if req.RoleID != RoleVillage && req.RoleID != RoleOpenWorld || req.RequiredCapacity <= 0 {
		return Instance{}, errors.New("WORLD_ASSIGNMENT_INVALID: 非常驻世界或容量无效")
	}
	candidates := []Instance{}
	for _, s := range r.instances {
		if s.RoleID == req.RoleID && s.RegionID == req.RegionID && s.ExperienceID == req.ExperienceID && s.WorldID == req.WorldID && (s.Status == StatusReady || s.Status == StatusActive) && hasFreshWorldHeartbeat(s, now) && s.Capacity-s.CurrentPlayers-s.ReservedPlayers >= req.RequiredCapacity {
			candidates = append(candidates, s)
		}
	}
	if len(candidates) == 0 {
		return Instance{}, errors.New("GAME_SERVER_NO_CAPACITY: 当前没有可用世界容量")
	}
	sort.Slice(candidates, func(i, j int) bool {
		a, b := candidates[i], candidates[j]
		fa, fb := a.Capacity-a.CurrentPlayers-a.ReservedPlayers, b.Capacity-b.CurrentPlayers-b.ReservedPlayers
		if fa == fb {
			return a.ID < b.ID
		}
		return fa < fb
	})
	return candidates[0], nil
}

// ReserveWorldTransfer确认已持久的原目标；同意图仅首次增加容量，目标不足/换Boot必须显式新逻辑请求。
func (r *Registry) ReserveWorldTransfer(id, server, boot string, slots int, now, expires time.Time) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.expireWorldTransfersLocked(now)
	s, ok := r.instances[server]
	if !ok || s.ServerBootID != boot || s.RoleID == RoleMainArena || slots <= 0 || (s.Status != StatusReady && s.Status != StatusActive) {
		return errors.New("WORLD_ASSIGNMENT_CONFLICT: 原目标不可准入")
	}
	// 预览不锁住后续心跳；持久意图可能延后恢复，因此确认容量前必须重新验证原目标存活。
	if !hasFreshWorldHeartbeat(s, now) {
		return errors.New("GAME_SERVER_HEARTBEAT_EXPIRED: 原目标心跳已过期")
	}
	if v, ok := r.worldTransfers[id]; ok {
		if v.ServerID != server || v.BootID != boot || v.Slots != slots {
			return errors.New("TRANSFER_IDEMPOTENCY_CONFLICT: 预留意图变化")
		}
		return nil
	}
	if !now.Before(expires) {
		return errors.New("TRANSFER_TICKET_EXPIRED: 原返回意图已过期")
	}
	if len(r.worldTransfers) >= 65536 || s.Capacity-s.CurrentPlayers-s.ReservedPlayers < slots {
		return errors.New("GAME_SERVER_NO_CAPACITY: 原目标容量不足")
	}
	s.ReservedPlayers += slots
	s.Status = StatusActive
	r.instances[server] = s
	r.worldTransfers[id] = worldTransferReservation{ServerID: server, BootID: boot, Slots: slots, ExpiresAt: expires, RetainUntil: now.Add(10 * time.Minute)}
	return nil
}

// FinishWorldTransfer释放本人一次性预留，保留幂等墓碑；true表示命中了该意图，false由旧兼容流程自行处理。
func (r *Registry) FinishWorldTransfer(id string) bool {
	r.mu.Lock()
	defer r.mu.Unlock()
	v, ok := r.worldTransfers[id]
	if !ok {
		return false
	}
	if !v.Finished {
		if s, ok := r.instances[v.ServerID]; ok && s.ServerBootID == v.BootID {
			s.ReservedPlayers = max(0, s.ReservedPlayers-v.Slots)
			r.instances[v.ServerID] = s
		}
		v.Finished = true
		r.worldTransfers[id] = v
	}
	return true
}
