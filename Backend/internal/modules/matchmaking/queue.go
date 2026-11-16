// 本文件实现持久匹配队列纯领域状态：票据作为不可拆分单元，候选逐人确认后才允许分配。
// 应用层必须在仓储事务内调用；本包不持有进程互斥、不联网、不签票、不把队列状态当作准入成功。
package matchmaking

import (
	"encoding/json"
	"errors"
	"sort"
	"time"
)

const (
	PhaseIdle                = "idle"                // 本人没有有效报名，可单人直接开始。
	PhaseChallengeInvitation = "challengeInvitation" // 固定对手尚待接收队长同意，不进入随机队列。
	ChallengeLifetime        = 60 * time.Second      // 有界约战邀请期限。
	PhaseReadyCheck          = "readyCheck"          // 队伍正在对同一名单和模式限时准备。
	PhaseSearching           = "searching"           // 不可拆分票据已原子进入队列。
	PhaseConfirmation        = "confirmation"        // 已选出两队，等待每位玩家接受。
	PhaseAllocating          = "allocating"          // 全员接受，持久分配/签票步骤尚未全部完成。
	PhaseTransferReady       = "transferReady"       // 全部签票成功并提交；本人可从独立受保护端口取迁移。
	PhaseInMatch             = "inMatch"             // 目标服务器确认入场，仍保留本局预留。
	PhaseReturning           = "returning"           // 可信结果已确认，等个人安全返回。
	PhaseCompleted           = "completed"           // 本轮已结束并释放预留。
	PhaseCancelled           = "cancelled"           // 迁移提交前明确取消，不授予入场。
	PhaseFailed              = "failed"              // 明确失败；清理步骤完成后释放。
	MaxActiveTickets         = 1024                  // 单逻辑写域的有界容量；超限显式拒绝，不以无限扫描维持工作。
	ConfirmationLifetime     = 30 * time.Second      // 后端逐人接受期限；客户端显示不改变权威资格。
	AllocationLifetime       = 60 * time.Second      // 全员接受后分配/签票的总预算。
	TerminalRetention        = 15 * time.Minute      // 终态/命令回执恢复窗口，与已有票据幂等期限保持一致。
	QueueLifetime            = 15 * time.Minute      // 有限报名有效期，属于服务器资格期限，不能被UI当作预计匹配时间。
	MaxOperationReceipts     = 32 * MaxActiveTickets // 限制不同幂等键的恶意写入，命中已有回执的安全重试仍可读取。
)

// QueueEntry是持久报名原子单元；名单从后端Party快照取得，ClientRequestID不能替代Owner授权。
type QueueEntry struct {
	Ticket             Ticket    // 平台匹配票据；不可拆分成员及模式/区域。
	OwnerPlayerID      string    // 发起单人或整队报名的可信主体，整队取消必须是该主体。
	ClientRequestID    string    // 主体域内原逻辑报名身份，重试保留。
	RequestFingerprint string    // 标准化请求摘要，不含凭据。
	Revision           int64     // 本条报名修订，条件命令使用。
	Phase              string    // 明确阶段，不能将searching解释为成功分配。
	CandidateID        string    // 候选身份，尚未组成时为空。
	DeadlineUTC        time.Time // 准备/确认/分配的权威截止时间，无期限阶段为零。
	UpdatedAtUTC       time.Time // 终态回执窗口与恢复清理基准。
	ReasonCode         string    // 稳定原因，不保存任意内部错误或敏感信息。
}

// CandidateSlot保存不可变分队名单；Accepted只表达本人对该候选的明确同意。
type CandidateSlot struct {
	PlayerID string // 已认证公开玩家身份。
	TeamID   string // team-a或team-b，由不可拆分分队算法确定。
	Accepted bool   // 逐人确认，队长不能替其他人接受。
}

// Candidate是可恢复确认聚合；分配/签票结果归独立持久操作，不广播到公共UI。
type Candidate struct {
	ChallengeID  string          // 可选来源邀请，仅内部用于确认期间的队长认证代次校验。
	ID           string          // 全局候选身份，同时作为分配意图幂等种子。
	Revision     int64           // 名单与回复修订。
	GameID       string          // 同一候选只能属于一个游戏。
	ArenaModeID  string          // 明确模式，不根据枚举数值推人数。
	RegionID     string          // 队列分区，禁止跨区域混填。
	TicketIDs    []string        // 原子票据集合，不能在确认期间重新拆分。
	Roster       []CandidateSlot // 两队精确人数，最多10人。
	Phase        string          // confirmation/allocating/transferReady/inMatch/returning/终态。
	DeadlineUTC  time.Time       // 当前阶段服务器期限。
	UpdatedAtUTC time.Time       // 回收终态的可信时间基准。
}

// QueueReceipt保存主体域逻辑命令结果；只记录非秘密状态，重试不能读取其他玩家结果。
type QueueReceipt struct {
	Fingerprint  string    // 同键异参必须拒绝。
	TicketID     string    // 已创建的票据/准备会话身份。
	ExpiresAtUTC time.Time // 有限恢复窗口；过期后由真实清理事务删除。
}

// QueueState是单逻辑写域的事务聚合；玩家预留与票据/候选同时更新，不能用TTL单独删除一个索引。
type QueueState struct {
	Revision     int64                   // 每次真实事务提交递增；只用于存储恢复，不替代每个聚合条件版本。
	Entries      map[string]QueueEntry   // 活动及有限保留终态报名。
	Reservations map[string]string       // 一人一个有效报名，覆盖准备、排队、确认和局内。
	Candidates   map[string]Candidate    // 持久候选及逐人回复。
	Receipts     map[string]QueueReceipt // 主体/操作/逻辑键摘要索引。
	Contexts     map[string]ActorContext // 逐人回复时取得的可信会话/角色，只供编排和本人迁移使用。
	Challenges   map[string]Challenge    // 与队列共用事务、唯一预留和事件日志的约战邀请。
}

// NewQueueState创建真实空队列，不构造玩家、模式可用性或Assignment。
func NewQueueState() *QueueState {
	return &QueueState{Entries: map[string]QueueEntry{}, Reservations: map[string]string{}, Candidates: map[string]Candidate{}, Receipts: map[string]QueueReceipt{}, Contexts: map[string]ActorContext{}, Challenges: map[string]Challenge{}}
}

// Clone深复制持久值，失败显式返回；仓储只能在全部规则成功后发布此副本。
func (q *QueueState) Clone() (*QueueState, error) {
	payload, err := json.Marshal(q)
	if err != nil {
		return nil, err
	}
	next := NewQueueState()
	if err = json.Unmarshal(payload, next); err != nil {
		return nil, err
	}
	return next, nil
}

// IsTerminal只识别明确终态；未知状态失败关闭，不能用非searching推断已经释放。
func IsTerminal(phase string) bool {
	return phase == PhaseCancelled || phase == PhaseCompleted || phase == PhaseFailed
}

// Prune只删除已释放的过期终态和回执；不按TTL擅自删除进行中玩家预留。
func (q *QueueState) Prune(now time.Time) {
	for id, challenge := range q.Challenges {
		if IsTerminal(challenge.Phase) && !challenge.UpdatedAtUTC.Add(TerminalRetention).After(now) {
			delete(q.Challenges, id)
		}
	}
	for id, entry := range q.Entries {
		if IsTerminal(entry.Phase) && !entry.UpdatedAtUTC.Add(TerminalRetention).After(now) {
			delete(q.Entries, id)
		}
	}
	for id, candidate := range q.Candidates {
		if IsTerminal(candidate.Phase) && !candidate.UpdatedAtUTC.Add(TerminalRetention).After(now) {
			delete(q.Candidates, id)
		}
	}
	for id, receipt := range q.Receipts {
		if !receipt.ExpiresAtUTC.After(now) {
			delete(q.Receipts, id)
		}
	}
	needed := map[string]bool{}
	for _, challenge := range q.Challenges {
		if challenge.Phase == PhaseChallengeInvitation {
			for _, player := range challenge.PlayerIDs() {
				needed[player] = true
			}
		}
	}
	for _, entry := range q.Entries {
		for _, player := range entry.Ticket.PartyMemberIDs {
			needed[player] = true
		}
	}
	for player := range q.Contexts {
		if !needed[player] {
			delete(q.Contexts, player)
		}
	}
}

// Enqueue在同一事务建立全部玩家预留；任何重复/非法字段失败不改变队列，准备阶段也不能重复单排。
func (q *QueueState) Enqueue(entry QueueEntry, now time.Time) error {
	if entry.Ticket.TicketID == "" || entry.OwnerPlayerID == "" || entry.Revision != 1 || (entry.Phase != PhaseSearching && entry.Phase != PhaseReadyCheck) {
		return errors.New("MATCH_COMMAND_INVALID: 报名身份或初始阶段无效")
	}
	if _, exists := q.Entries[entry.Ticket.TicketID]; exists {
		return errors.New("MATCH_IDEMPOTENCY_CONFLICT: 票据身份已存在")
	}
	checked, err := NewTicket(CreateTicketRequest{TicketID: entry.Ticket.TicketID, GameID: entry.Ticket.GameID, ArenaModeID: entry.Ticket.ArenaModeID, PartyID: entry.Ticket.PartyID, PartyMemberIDs: entry.Ticket.PartyMemberIDs, Region: entry.Ticket.Region})
	if err != nil {
		return err
	}
	if checked.GameID == "" || checked.Region == "" || checked.PartySize != entry.Ticket.PartySize {
		return errors.New("MATCH_COMMAND_INVALID: 票据游戏、区域或人数无效")
	}
	ownerFound := false
	active := 0
	for _, current := range q.Entries {
		if !IsTerminal(current.Phase) {
			active++
		}
	}
	if active >= MaxActiveTickets || len(q.Entries) >= 4*MaxActiveTickets {
		return errors.New("MATCH_QUEUE_CAPACITY: 队列恢复窗口已满")
	}
	for _, player := range checked.PartyMemberIDs {
		if q.Reservations[player] != "" {
			return errors.New("MATCH_PLAYER_RESERVED: 玩家已有活动报名或比赛")
		}
		ownerFound = ownerFound || player == entry.OwnerPlayerID
	}
	if !ownerFound {
		return errors.New("MATCH_FORBIDDEN: 报名主体不在名单")
	}
	entry.UpdatedAtUTC = now.UTC()
	if entry.Phase == PhaseSearching && entry.DeadlineUTC.IsZero() {
		entry.DeadlineUTC = now.Add(QueueLifetime)
	}
	entry.Ticket.State = entry.Phase
	q.Entries[entry.Ticket.TicketID] = entry
	for _, player := range checked.PartyMemberIDs {
		q.Reservations[player] = entry.Ticket.TicketID
	}
	q.Revision++
	return nil
}

// FormCandidate以有界双背包按排队顺序选择精确两队；每个票据只放一侧，剩余票据保持原队列。
// 状态上限为6×6，每个队列最多1024原子票据；没有可行组合时不修改任何票据或构造假比赛。
func (q *QueueState) FormCandidate(id, modeID, region string, now time.Time) (Candidate, bool, error) {
	mode, ok := ArenaModeByID(modeID)
	if !ok || id == "" || region == "" {
		return Candidate{}, false, errors.New("MATCH_MODE_INVALID: 候选参数无效")
	}
	if _, found := q.Candidates[id]; found {
		return Candidate{}, false, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 候选身份已存在")
	}
	available := []QueueEntry{}
	for _, entry := range q.Entries {
		if entry.Phase == PhaseSearching && entry.Ticket.ArenaModeID == modeID && entry.Ticket.Region == region {
			available = append(available, entry)
		}
	}
	sort.Slice(available, func(i, j int) bool {
		if available[i].Ticket.CreatedAt.Equal(available[j].Ticket.CreatedAt) {
			return available[i].Ticket.TicketID < available[j].Ticket.TicketID
		}
		return available[i].Ticket.CreatedAt.Before(available[j].Ticket.CreatedAt)
	})
	if len(available) == 0 {
		return Candidate{}, false, nil
	}
	gameID := available[0].Ticket.GameID
	type formation struct{ a, b []string }
	var states [6][6]*formation
	states[0][0] = &formation{}
	for _, entry := range available {
		if entry.Ticket.GameID != gameID {
			continue
		}
		size := entry.Ticket.PartySize
		next := states
		for a := 0; a <= mode.TeamSize; a++ {
			for b := 0; b <= mode.TeamSize; b++ {
				old := states[a][b]
				if old == nil {
					continue
				}
				if a+size <= mode.TeamSize && next[a+size][b] == nil {
					next[a+size][b] = &formation{a: append(append([]string{}, old.a...), entry.Ticket.TicketID), b: append([]string{}, old.b...)}
				}
				if b+size <= mode.TeamSize && next[a][b+size] == nil {
					next[a][b+size] = &formation{a: append([]string{}, old.a...), b: append(append([]string{}, old.b...), entry.Ticket.TicketID)}
				}
			}
		}
		states = next
		if states[mode.TeamSize][mode.TeamSize] != nil {
			break
		}
	}
	picked := states[mode.TeamSize][mode.TeamSize]
	if picked == nil {
		return Candidate{}, false, nil
	}
	candidate, err := q.FormFixedCandidate(id, modeID, region, picked.a, picked.b, now)
	return candidate, err == nil, err
}

// FormFixedCandidate是随机选队与已同意约战共用的唯一候选构造器；先复核精确两队及预留，再一次发布。
func (q *QueueState) FormFixedCandidate(id, modeID, region string, teamA, teamB []string, now time.Time) (Candidate, error) {
	mode, ok := ArenaModeByID(modeID)
	if !ok || id == "" || region == "" {
		return Candidate{}, errors.New("MATCH_MODE_INVALID: 候选参数无效")
	}
	if _, exists := q.Candidates[id]; exists {
		return Candidate{}, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 候选身份已存在")
	}
	seen := map[string]bool{}
	gameID := ""
	for _, tickets := range [][]string{teamA, teamB} {
		count := 0
		for _, ticketID := range tickets {
			entry, found := q.Entries[ticketID]
			if !found || entry.Phase != PhaseSearching || entry.Ticket.ArenaModeID != modeID || entry.Ticket.Region != region || !now.Before(entry.DeadlineUTC) {
				return Candidate{}, errors.New("MATCH_PHASE_CONFLICT: 候选票据不可用")
			}
			if gameID == "" {
				gameID = entry.Ticket.GameID
			}
			if entry.Ticket.GameID != gameID {
				return Candidate{}, errors.New("MATCH_MODE_INVALID: 候选游戏不一致")
			}
			for _, player := range entry.Ticket.PartyMemberIDs {
				if seen[player] || q.Reservations[player] != ticketID {
					return Candidate{}, errors.New("MATCH_PLAYER_RESERVED: 候选名单或预留重复")
				}
				seen[player] = true
				count++
			}
		}
		if count != mode.TeamSize {
			return Candidate{}, errors.New("MATCH_PARTY_NOT_READY: 候选必须组成两支完整队伍")
		}
	}
	candidate := Candidate{ID: id, Revision: 1, GameID: gameID, ArenaModeID: modeID, RegionID: region, Phase: PhaseConfirmation, DeadlineUTC: now.Add(ConfirmationLifetime), UpdatedAtUTC: now}
	for index, tickets := range [][]string{teamA, teamB} {
		team := []string{"team-a", "team-b"}[index]
		for _, ticketID := range tickets {
			entry := q.Entries[ticketID]
			candidate.TicketIDs = append(candidate.TicketIDs, ticketID)
			for _, player := range entry.Ticket.PartyMemberIDs {
				candidate.Roster = append(candidate.Roster, CandidateSlot{PlayerID: player, TeamID: team})
			}
			entry.Phase = PhaseConfirmation
			entry.Ticket.State = entry.Phase
			entry.CandidateID = id
			entry.DeadlineUTC = candidate.DeadlineUTC
			entry.Revision++
			entry.UpdatedAtUTC = now
			q.Entries[ticketID] = entry
		}
	}
	q.Candidates[id] = candidate
	q.Revision++
	return candidate, nil
}

// Reply仅允许名单本人对未到期版本回复；全员接受才转Allocating，拒绝则原子终止整个候选并释放所有预留。
func (q *QueueState) Reply(actor, id string, revision int64, accept bool, now time.Time) error {
	c, found := q.Candidates[id]
	if !found {
		return errors.New("MATCH_NOT_FOUND: 候选不存在")
	}
	index := -1
	for i, slot := range c.Roster {
		if slot.PlayerID == actor {
			index = i
			break
		}
	}
	if index < 0 {
		return errors.New("MATCH_FORBIDDEN: 不在候选名单")
	}
	if c.Phase != PhaseConfirmation {
		return errors.New("MATCH_PHASE_CONFLICT: 候选不再接受回复")
	}
	if c.Revision != revision {
		return errors.New("MATCH_REVISION_CONFLICT: 候选已变化")
	}
	if !now.Before(c.DeadlineUTC) {
		return errors.New("MATCH_DEADLINE_EXPIRED: 确认已经到期")
	}
	if !accept {
		q.finishCandidate(id, PhaseCancelled, "MATCH_DECLINED", now)
		return nil
	}
	if c.Roster[index].Accepted {
		return nil
	}
	c.Roster[index].Accepted = true
	c.Revision++
	c.UpdatedAtUTC = now
	complete := true
	for _, slot := range c.Roster {
		complete = complete && slot.Accepted
	}
	if complete {
		c.Phase = PhaseAllocating
		c.DeadlineUTC = now.Add(AllocationLifetime)
		for _, ticketID := range c.TicketIDs {
			entry := q.Entries[ticketID]
			entry.Phase = PhaseAllocating
			entry.Ticket.State = entry.Phase
			entry.DeadlineUTC = c.DeadlineUTC
			entry.Revision++
			entry.UpdatedAtUTC = now
			q.Entries[ticketID] = entry
		}
	}
	q.Candidates[id] = c
	q.Revision++
	return nil
}

// Cancel在迁移提交前仅允许报名发起者整队取消；已开始签票/分配必须走可恢复撤销，不能立即释放可能生效的票据。
func (q *QueueState) Cancel(actor, id string, revision int64, now time.Time) error {
	entry, found := q.Entries[id]
	if !found {
		return errors.New("MATCH_NOT_FOUND: 报名不存在")
	}
	if entry.OwnerPlayerID != actor {
		return errors.New("MATCH_FORBIDDEN: 只有报名主体可整队取消")
	}
	if entry.Revision != revision {
		return errors.New("MATCH_REVISION_CONFLICT: 报名已变化")
	}
	if IsTerminal(entry.Phase) {
		return nil
	}
	if entry.Phase != PhaseReadyCheck && entry.Phase != PhaseSearching && entry.Phase != PhaseConfirmation {
		return errors.New("MATCH_PHASE_CONFLICT: 迁移处理中，请读取最新状态")
	}
	if entry.CandidateID != "" {
		q.finishCandidate(entry.CandidateID, PhaseCancelled, "MATCH_CANCELLED", now)
	} else {
		q.finishEntry(id, PhaseCancelled, "MATCH_CANCELLED", now)
		q.Revision++
	}
	return nil
}

// Expire仅推进尚未发生外部副作用的准备/确认；分配期限由持久操作恢复器撤销后终结，防止仍有效签票时提前解锁。
func (q *QueueState) Expire(now time.Time) {
	for id, challenge := range q.Challenges {
		if challenge.Phase == PhaseChallengeInvitation && !now.Before(challenge.DeadlineUTC) {
			q.FinishChallenge(id, "MATCH_DEADLINE_EXPIRED", now)
		}
	}
	for id, entry := range q.Entries {
		if entry.Phase == PhaseSearching && !now.Before(entry.DeadlineUTC) {
			q.finishEntry(id, PhaseCancelled, "MATCH_QUEUE_EXPIRED", now)
			q.Revision++
			continue
		}
		if entry.Phase == PhaseReadyCheck && !now.Before(entry.DeadlineUTC) {
			q.finishEntry(id, PhaseCancelled, "MATCH_READY_TIMEOUT", now)
			q.Revision++
		}
	}
	for id, c := range q.Candidates {
		if c.Phase == PhaseConfirmation && !now.Before(c.DeadlineUTC) {
			q.finishCandidate(id, PhaseCancelled, "MATCH_CONFIRMATION_TIMEOUT", now)
		}
	}
	q.Prune(now)
}

// FinishCandidate供受信持久步骤在结果/撤销已确认后调用，不对公网暴露。释放预留时核对原票据身份。
func (q *QueueState) FinishCandidate(id, phase, reason string, now time.Time) error {
	if !IsTerminal(phase) {
		return errors.New("MATCH_COMMAND_INVALID: 只能提交明确终态")
	}
	if _, found := q.Candidates[id]; !found {
		return errors.New("MATCH_NOT_FOUND: 候选不存在")
	}
	q.finishCandidate(id, phase, reason, now)
	return nil
}
func (q *QueueState) finishCandidate(id, phase, reason string, now time.Time) {
	c, found := q.Candidates[id]
	if !found || IsTerminal(c.Phase) {
		return
	}
	c.Phase = phase
	c.Revision++
	c.UpdatedAtUTC = now
	q.Candidates[id] = c
	for _, ticketID := range c.TicketIDs {
		q.finishEntry(ticketID, phase, reason, now)
	}
	q.Revision++
}
func (q *QueueState) finishEntry(id, phase, reason string, now time.Time) {
	entry := q.Entries[id]
	entry.Phase = phase
	entry.Ticket.State = phase
	entry.ReasonCode = reason
	entry.Revision++
	entry.UpdatedAtUTC = now
	q.Entries[id] = entry
	for _, player := range entry.Ticket.PartyMemberIDs {
		if q.Reservations[player] == id {
			delete(q.Reservations, player)
		}
	}
}
