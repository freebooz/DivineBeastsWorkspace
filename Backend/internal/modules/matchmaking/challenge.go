// 约战预备阶段属于同一持久队列；邀请只预留匹配资格，不冻结任一Party的编辑能力。
package matchmaking

import "time"

// Challenge保存服务端解析的两队版本；会话绑定属于内部值，禁止直接作为公网DTO。
type Challenge struct {
	TargetSessionID string    // 接受时已认证代次，最终确认不能覆盖已接受的队长上下文。
	ID              string    // 全局邀请身份，同时作为双方唯一资格预留身份。
	Revision        int64     // 条件回复修订。
	OwnerPlayerID   string    // 发起队长或单人。
	TargetPlayerID  string    // 解析后的接收队长或单人。
	OwnerSessionID  string    // 发起时已认证代次，旧会话邀请不能被新代次命令复用。
	ArenaModeID     string    // Shared已登记模式。
	RegionID        string    // 部署认可区域。
	PartyAID        string    // 无队伍单人为空。
	PartyBID        string    // 无队伍单人为空。
	PartyARevision  int64     // 邀请时完整队伍修订。
	PartyBRevision  int64     // 接受时在同一事务复核。
	TeamAPlayerIDs  []string  // 不可变甲队名单。
	TeamBPlayerIDs  []string  // 不可变乙队名单。
	Phase           string    // challengeInvitation或明确终态；接受后由Candidate拥有资格。
	DeadlineUTC     time.Time // 60秒权威邀请期限。
	UpdatedAtUTC    time.Time // 终态恢复保留窗口。
	ReasonCode      string    // 安全终止原因。
}

// PlayerIDs返回两队独立名单，供仓储WATCH和本人通知使用。
func (c Challenge) PlayerIDs() []string {
	return append(append([]string{}, c.TeamAPlayerIDs...), c.TeamBPlayerIDs...)
}

// HasRosterReservation只限制已准备/排队/候选/比赛资格；未接受邀请不能挟持目标队伍编辑。
func (q *QueueState) HasRosterReservation(player string) bool {
	id := q.Reservations[player]
	if c, found := q.Challenges[id]; found && c.Phase == PhaseChallengeInvitation {
		return false
	}
	return id != ""
}

// FinishChallenge仅结束预备邀请并释放属于本邀请的索引，不触碰任何已转换候选。
func (q *QueueState) FinishChallenge(id, reason string, now time.Time) {
	c, found := q.Challenges[id]
	if !found || c.Phase != PhaseChallengeInvitation {
		return
	}
	c.Phase = PhaseCancelled
	c.ReasonCode = reason
	c.Revision++
	c.UpdatedAtUTC = now
	q.Challenges[id] = c
	for _, player := range c.PlayerIDs() {
		if q.Reservations[player] == id {
			delete(q.Reservations, player)
		}
	}
	q.Revision++
}
