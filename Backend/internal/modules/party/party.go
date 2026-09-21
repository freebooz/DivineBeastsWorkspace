// Package party（组队领域）实现赛前Party成员、队长、准备状态和Roster Lock（名单锁定）规则。
package party

import "errors"

const MaxMembers = 5 // MaxMembers（最大Party人数）对应当前最大5v5模式。

// Member（Party成员）表示一个组队成员的业务状态。
type Member struct {
	PlayerID    string // PlayerID（玩家ID）。
	DisplayName string // DisplayName（显示名称）。
	Ready       bool   // Ready（是否准备）。
	Online      bool   // Online（是否在线）。
}

// Party（组队）是Matchmaking（匹配）的不可拆分原子单元。
type Party struct {
	ID                  string            // ID（组队唯一ID）。
	LeaderPlayerID      string            // LeaderPlayerID（队长玩家ID）。
	Members             map[string]Member // Members（成员集合）。
	SelectedArenaModeID string            // SelectedArenaModeID（已选择竞技模式）。
	RosterLocked        bool              // RosterLocked（名单是否锁定）。
	Revision            int64             // Revision（Party修订版本）。
}

// New（创建Party）以创建者作为队长和首个成员。
func New(id string, leader Member) *Party {
	leader.Online = true
	return &Party{ID: id, LeaderPlayerID: leader.PlayerID, Members: map[string]Member{leader.PlayerID: leader}, Revision: 1}
}

// AddMember（添加成员）在未锁定且未满员时添加玩家。
func (p *Party) AddMember(member Member) error {
	if p.RosterLocked {
		return errors.New("PARTY_ROSTER_LOCKED: Party名单已锁定")
	}
	if len(p.Members) >= MaxMembers {
		return errors.New("PARTY_FULL: Party已满")
	}
	if member.PlayerID == "" {
		return errors.New("PlayerID不能为空")
	}
	if _, exists := p.Members[member.PlayerID]; exists {
		return nil
	}
	member.Online = true
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
		return errors.New("队长不能直接通过RemoveMember离队，应先转移队长或解散Party")
	}
	if _, exists := p.Members[playerID]; !exists {
		return nil
	}
	delete(p.Members, playerID)
	p.Revision++
	return nil
}

// SetReady（设置准备状态）修改指定成员的准备状态。
func (p *Party) SetReady(playerID string, ready bool) error {
	member, exists := p.Members[playerID]
	if !exists {
		return errors.New("Party成员不存在")
	}
	member.Ready = ready
	p.Members[playerID] = member
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
		return errors.New("ArenaModeID不能为空")
	}
	p.SelectedArenaModeID = arenaModeID
	p.Revision++
	return nil
}

// LockRoster（锁定名单）在ReadyCheck通过、正式开始Queueing前冻结成员与模式变更。
func (p *Party) LockRoster() { p.RosterLocked = true; p.Revision++ }

// UnlockRoster（解锁名单）在取消匹配或匹配失败后恢复编辑能力。
func (p *Party) UnlockRoster() { p.RosterLocked = false; p.Revision++ }
