// Package party（组队领域）实现平台普通队伍、邀请／申请、队长、准备和名单锁定的服务端领域规则。
// Match应用在仓储事务中调用；本包拥有聚合状态，不联网、不认证、不拥有跨队成员索引，不依赖UE或具体游戏资源。
package party

import (
	"errors"
	"math"
	"time"
)

const MaxMembers = 5 // MaxMembers（最大Party人数）对应当前最大5v5模式。

// Member（Party成员）表示一个组队成员的业务状态。
type Member struct {
	PresenceExpiresUTC time.Time // 受信连接观察租约截止；零值旧记录为未知，不永久视为在线，不向UI投影。
	OfflineSinceUTC    time.Time // 实际租约失效时刻，队长宽限由此计算；在线时为空。
	PlayerID           string    // PlayerID（玩家ID）。
	DisplayName        string    // DisplayName（显示名称）。
	Ready              bool      // Ready（是否准备）。
	Online             bool      // Online（是否在线）。
	JoinSequence       int64     // JoinSequence（本队内唯一加入序号）；旧记录0表示缺少历史顺序，不能据此自动选队长。
}

// Party（组队）是Matchmaking（匹配）的不可拆分原子单元。
type Party struct {
	ID                  string                       // ID（组队唯一ID）。
	LeaderPlayerID      string                       // LeaderPlayerID（队长玩家ID）。
	Members             map[string]Member            // Members（成员集合）。
	SelectedArenaModeID string                       // SelectedArenaModeID（已选择竞技模式）。
	RosterLocked        bool                         // RosterLocked（名单是否锁定）。
	Revision            int64                        // Revision（Party修订版本）。
	NextJoinSequence    int64                        // NextJoinSequence（下次加入使用的单调序号），只由服务端在同一事务内分配。
	MembershipRequests  map[string]MembershipRequest // MembershipRequests（有界邀请／申请）；不证明玩家跨队唯一归属。
	ReadyCheckID        string                       // 服务器当前限时准备会话，空表示普通准备标记不能直接授予匹配资格。
	ReadyDeadlineUTC    time.Time                    // 同一模式/名单的准备截止；只由后端判断到期。
}

// New（创建Party）以创建者作为队长和首个成员。
func New(id string, leader Member) *Party {
	leader.Online = true
	leader.PresenceExpiresUTC = time.Now().UTC().Add(PresenceLeaseLifetime)
	leader.JoinSequence = 1
	return &Party{ID: id, LeaderPlayerID: leader.PlayerID, Members: map[string]Member{leader.PlayerID: leader}, Revision: 1, NextJoinSequence: 2}
}

// AddMember（添加成员）在未锁定且未满员时添加玩家。
func (p *Party) AddMember(member Member) error {
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: Party名单已锁定")
	}
	if !validMembershipID(member.PlayerID) {
		return errors.New("PARTY_COMMAND_INVALID: PlayerID无效")
	}
	if _, exists := p.Members[member.PlayerID]; exists {
		return nil
	}
	if len(p.Members) >= MaxMembers {
		return errors.New("PARTY_FULL: Party已满")
	}
	if p.Members == nil || p.LeaderPlayerID == "" {
		return errors.New("PARTY_INVALID: 队伍已解散或记录无效")
	}
	// 旧序列化记录没有NextJoinSequence；只为新加入者分配比已知顺序更大的序号，不捏造旧成员的历史。
	nextJoinSequence := p.NextJoinSequence
	for _, current := range p.Members {
		if current.JoinSequence == math.MaxInt64 {
			return errors.New("PARTY_JOIN_ORDER_CONFLICT: 成员加入序号已耗尽")
		}
		if nextJoinSequence <= current.JoinSequence {
			nextJoinSequence = current.JoinSequence + 1
		}
	}
	if nextJoinSequence <= 0 {
		nextJoinSequence = 1
	}
	// 失败前不写聚合，且必须保留一个可表示的下一序号，不能让有符号整数回绕后复用历史顺序。
	if nextJoinSequence == math.MaxInt64 {
		return errors.New("PARTY_JOIN_ORDER_CONFLICT: 无法分配新的加入序号")
	}
	member.Online = true
	member.PresenceExpiresUTC = time.Now().UTC().Add(PresenceLeaseLifetime)
	member.Ready = false
	member.JoinSequence = nextJoinSequence
	p.NextJoinSequence = nextJoinSequence + 1
	p.invalidateReadiness()
	p.Members[member.PlayerID] = member
	p.Revision++
	return nil
}

// RemoveMember（移除成员）在未锁定时移除非队长成员。
func (p *Party) RemoveMember(playerID string) error {
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: Party名单已锁定")
	}
	if playerID == p.LeaderPlayerID {
		return errors.New("PARTY_COMMAND_INVALID: 队长应使用离队、转让或解散命令")
	}
	if _, exists := p.Members[playerID]; !exists {
		return nil
	}
	delete(p.Members, playerID)
	p.invalidateReadiness()
	p.Revision++
	return nil
}

// SetReady（设置准备状态）修改指定成员的准备状态。
func (p *Party) SetReady(playerID string, ready bool) error {
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 匹配期间不能修改准备")
	}
	member, exists := p.Members[playerID]
	if !exists {
		return errors.New("PARTY_NOT_MEMBER: Party成员不存在")
	}
	if ready && !member.Online {
		return errors.New("PARTY_MEMBER_UNAVAILABLE: 离线成员不能准备")
	}
	if p.ReadyCheckID != "" && !time.Now().UTC().Before(p.ReadyDeadlineUTC) {
		return errors.New("PARTY_READY_EXPIRED: 本轮准备已经到期")
	}
	if member.Ready == ready && !(!ready && p.ReadyCheckID != "") {
		return nil
	}
	member.Ready = ready
	p.Members[playerID] = member
	if !ready && p.ReadyCheckID != "" {
		p.ReadyCheckID = ""
		p.ReadyDeadlineUTC = time.Time{}
	}
	p.Revision++
	return nil
}

// SelectArenaMode（选择竞技模式）只允许队长在Roster未锁定时修改。
func (p *Party) SelectArenaMode(actorPlayerID, arenaModeID string) error {
	if actorPlayerID != p.LeaderPlayerID {
		return errors.New("PARTY_NOT_LEADER: 只有队长可以选择竞技模式")
	}
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 匹配期间不能修改竞技模式")
	}
	if arenaModeID == "" {
		return errors.New("PARTY_COMMAND_INVALID: ArenaModeID不能为空")
	}
	if p.SelectedArenaModeID == arenaModeID {
		return nil
	}
	p.SelectedArenaModeID = arenaModeID
	p.invalidateReadiness()
	p.Revision++
	return nil
}

// LockRoster（锁定名单）在ReadyCheck通过、正式开始Queueing前冻结成员与模式变更。
func (p *Party) LockRoster() {
	if !p.RosterLocked {
		p.RosterLocked = true
		p.Revision++
	}
}

// UnlockRoster（解锁名单）在取消匹配或匹配失败后恢复编辑能力。
func (p *Party) UnlockRoster() {
	if p.RosterLocked {
		p.RosterLocked = false
		p.invalidateReadiness()
		p.Revision++
	}
}

// invalidateReadiness（失效旧准备）只在名单／活动／队长结构变化时调用，防止旧确认被新队伍复用。
func (p *Party) invalidateReadiness() {
	p.ReadyCheckID = ""
	p.ReadyDeadlineUTC = time.Time{}
	for id, member := range p.Members {
		member.Ready = false
		p.Members[id] = member
	}
}

// BeginReadyCheck只能由队长对当前名单及模式发起；队长的开始命令表达本人同意，其他成员仍须分别准备。
// 调用方必须在匹配事务中同时为全员建立唯一预留；失败不能留下只有准备而没有队列记录的状态。
func (p *Party) BeginReadyCheck(actor, mode, id string, now time.Time) error {
	if id == "" {
		return errors.New("PARTY_COMMAND_INVALID: 准备会话身份不能为空")
	}
	if actor == "" || actor != p.LeaderPlayerID {
		return errors.New("PARTY_NOT_LEADER: 只有队长可发起准备")
	}
	member := p.Members[actor]
	if !member.Online {
		return errors.New("PARTY_MEMBER_UNAVAILABLE: 队长不在线")
	}
	if err := p.SelectArenaMode(actor, mode); err != nil {
		return err
	}
	p.invalidateReadiness()
	p.ReadyCheckID = id
	p.ReadyDeadlineUTC = now.Add(20 * time.Second)
	member.Ready = true
	p.Members[actor] = member
	p.Revision++
	return nil
}

// CancelReadyCheck在真实取消/超时事务中失效显示与普通准备；不解除仍有效的比赛名单锁。
func (p *Party) CancelReadyCheck() {
	if p.ReadyCheckID != "" {
		p.invalidateReadiness()
		p.Revision++
	}
}

// TransferLeadership（显式转让队长）只允许当前队长在可编辑状态转给在线队员；失败无状态修改。
func (p *Party) TransferLeadership(actorPlayerID, targetPlayerID string) error {
	if actorPlayerID != p.LeaderPlayerID {
		return errors.New("PARTY_NOT_LEADER: 只有队长可以转让")
	}
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 名单锁定时不能转让")
	}
	member, found := p.Members[targetPlayerID]
	if !found || !member.Online {
		return errors.New("PARTY_MEMBER_UNAVAILABLE: 目标不是在线成员")
	}
	if actorPlayerID == targetPlayerID {
		return nil
	}
	p.LeaderPlayerID = targetPlayerID
	p.invalidateReadiness()
	p.Revision++
	return nil
}

// Leave（成员离队）在可编辑状态离开；队长自动转最早加入者，最后一人离开视为解散。
// 本方法只改变队内数据；调用方必须在同一仓储事务释放玩家成员索引，禁止直接向客户端暴露领域对象写权限。
func (p *Party) Leave(playerID string) error {
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 应先取消整队匹配")
	}
	if _, found := p.Members[playerID]; !found {
		return errors.New("PARTY_NOT_MEMBER: 玩家不在此队")
	}
	if playerID != p.LeaderPlayerID {
		return p.RemoveMember(playerID)
	}
	var next Member
	joinSequences := make(map[int64]struct{}, len(p.Members))
	for id, member := range p.Members {
		if id == playerID {
			continue
		}
		if member.JoinSequence <= 0 {
			return errors.New("PARTY_JOIN_ORDER_MIGRATION_REQUIRED: 旧记录缺少加入顺序，请先显式转让")
		}
		if _, duplicate := joinSequences[member.JoinSequence]; duplicate {
			return errors.New("PARTY_JOIN_ORDER_CONFLICT: 成员加入序号重复")
		}
		joinSequences[member.JoinSequence] = struct{}{}
		if next.PlayerID == "" || member.JoinSequence < next.JoinSequence {
			next = member
		}
	}
	delete(p.Members, playerID)
	p.LeaderPlayerID = next.PlayerID
	p.invalidateReadiness()
	if len(p.Members) == 0 {
		p.cancelPendingMembershipRequests()
	}
	p.Revision++
	return nil
}

// Disband（解散普通队伍）只允许队长，先失效所有Pending请求；外部事务同时撤销全部成员索引。
func (p *Party) Disband(actorPlayerID string) error {
	if actorPlayerID != p.LeaderPlayerID || actorPlayerID == "" {
		return errors.New("PARTY_NOT_LEADER: 只有队长可以解散")
	}
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: 应先取消整队匹配")
	}
	p.Members = map[string]Member{}
	p.LeaderPlayerID = ""
	p.cancelPendingMembershipRequests()
	p.Revision++
	return nil
}

// Clone（复制聚合）隔离调用者与仓储保存对象；邀请和成员映射均须深复制，避免未提交修改直接泄漏。
func (p *Party) Clone() *Party {
	if p == nil {
		return nil
	}
	cloned := *p
	cloned.Members = make(map[string]Member, len(p.Members))
	for id, member := range p.Members {
		cloned.Members[id] = member
	}
	cloned.MembershipRequests = make(map[string]MembershipRequest, len(p.MembershipRequests))
	for id, request := range p.MembershipRequests {
		cloned.MembershipRequests[id] = request
	}
	return &cloned
}
