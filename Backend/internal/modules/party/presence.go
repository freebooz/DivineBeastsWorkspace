// 本文件维护已认证连接观察的服务端租约；普通旅行持续同一GI观察，不由Widget/自报Online改权威。
// 只有在线显示/准备失效和队长转移改变公开Revision；续租元数据必须静默持久化，避免观察事件反馈循环。
package party

import (
	"errors"
	"time"
)

const PresenceLeaseLifetime = 45 * time.Second // 覆盖20秒长等待及有界传输重试，不代替登录令牌有效性。
const LeaderOfflineGrace = 60 * time.Second    // 仅可编辑队伍允许最早在线成员继任。
// RenewPresence只供认证Gateway连接调用；成员重新上线失效此前准备，持续续租不改变公开Revision。
func (p *Party) RenewPresence(player string, now time.Time) {
	m, found := p.Members[player]
	if !found {
		return
	}
	if !m.Online {
		m.Online = true
		m.Ready = false
		p.Revision++
	}
	m.PresenceExpiresUTC = now.Add(PresenceLeaseLifetime)
	m.OfflineSinceUTC = time.Time{}
	p.Members[player] = m
}

// ReconcilePresence由服务端截止事件调用；锁定比赛仍保留名单，调用方负责先取消未提交队列再自动转队长。
func (p *Party) ReconcilePresence(now time.Time) bool {
	changed := false
	for id, m := range p.Members {
		if m.Online && (m.PresenceExpiresUTC.IsZero() || !now.Before(m.PresenceExpiresUTC)) {
			m.Online = false
			m.Ready = false
			m.OfflineSinceUTC = m.PresenceExpiresUTC
			if m.OfflineSinceUTC.IsZero() {
				m.OfflineSinceUTC = now
			}
			p.Members[id] = m
			changed = true
		}
	}
	if changed {
		p.invalidateReadiness()
		p.Revision++
	}
	return changed
}

// TransferOfflineLeadership只在解锁后执行；拒绝缺失或重复历史序号，不能按map/字典顺序猜加入顺序。
func (p *Party) TransferOfflineLeadership(now time.Time) error {
	leader, found := p.Members[p.LeaderPlayerID]
	if !found || leader.Online || leader.OfflineSinceUTC.IsZero() || now.Before(leader.OfflineSinceUTC.Add(LeaderOfflineGrace)) || p.RosterLocked {
		return nil
	}
	next := Member{}
	seen := map[int64]bool{}
	for _, m := range p.Members {
		if !m.Online {
			continue
		}
		if m.JoinSequence <= 0 || seen[m.JoinSequence] {
			return errors.New("PARTY_JOIN_ORDER_CONFLICT: 自动转让缺少唯一加入顺序")
		}
		seen[m.JoinSequence] = true
		if next.PlayerID == "" || m.JoinSequence < next.JoinSequence {
			next = m
		}
	}
	if next.PlayerID != "" {
		p.LeaderPlayerID = next.PlayerID
		p.invalidateReadiness()
		p.Revision++
	}
	return nil
}

// NextPresenceDeadline返回下一次真实业务截止，零表示没有待处理期限；供服务端单次事件等待，不给UI轮询。
func (p *Party) NextPresenceDeadline(now time.Time) time.Time {
	var next time.Time
	for id, m := range p.Members {
		d := m.PresenceExpiresUTC
		if !m.Online {
			if id != p.LeaderPlayerID || p.RosterLocked || m.OfflineSinceUTC.IsZero() {
				continue
			}
			d = m.OfflineSinceUTC.Add(LeaderOfflineGrace)
		}
		if d.After(now) && (next.IsZero() || d.Before(next)) {
			next = d
		}
	}
	return next
}
