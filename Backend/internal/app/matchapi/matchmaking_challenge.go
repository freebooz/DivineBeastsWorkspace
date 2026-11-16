// 固定对手邀请是既有匹配事务的预备阶段；不调用控制面，不另建分配、签票或返回执行链。
package matchapi

import (
	"divinebeasts/backend/internal/modules/matchmaking"
	"errors"
	"sort"
	"time"
)

// challengeTeam从受WATCH保护的成员索引解析真实队长及整队；不能信任客户端传入的对手名单。
func challengeTeam(tx *MatchmakingTransaction, player, modeID string) (string, int64, string, []string, error) {
	mode, ok := matchmaking.ArenaModeByID(modeID)
	if !ok {
		return "", 0, "", nil, errors.New("MATCH_MODE_INVALID: 模式未注册")
	}
	id := tx.PlayerParties[player]
	if id == "" {
		if mode.TeamSize != 1 {
			return "", 0, "", nil, errors.New("MATCH_PARTY_NOT_READY: 约战必须两支完整队伍")
		}
		return "", 0, player, []string{player}, nil
	}
	p := tx.Parties[id]
	if p == nil {
		return "", 0, "", nil, errors.New("PARTY_INDEX_INVALID: 实际队伍不可读")
	}
	if _, found := p.Members[player]; !found {
		return "", 0, "", nil, errors.New("PARTY_INDEX_INVALID: 玩家不在索引队伍")
	}
	if p.RosterLocked || p.ReadyCheckID != "" {
		return "", 0, "", nil, errors.New("MATCH_PLAYER_RESERVED: 队伍已有准备或比赛")
	}
	if len(p.Members) != mode.TeamSize {
		return "", 0, "", nil, errors.New("MATCH_PARTY_NOT_READY: 约战双方必须满编")
	}
	members := make([]string, 0, len(p.Members))
	for id, m := range p.Members {
		if tx.PlayerParties[id] != p.ID {
			return "", 0, "", nil, errors.New("PARTY_INDEX_INVALID: 名单索引不一致")
		}
		if !m.Online {
			return "", 0, "", nil, errors.New("PARTY_MEMBER_UNAVAILABLE: 约战成员必须在线")
		}
		members = append(members, id)
	}
	sort.Strings(members)
	return p.ID, p.Revision, p.LeaderPlayerID, members, nil
}

func challengeTeamUnchanged(tx *MatchmakingTransaction, id string, revision int64, leader string, members []string) bool {
	if id == "" {
		return len(members) == 1 && members[0] == leader && tx.PlayerParties[leader] == ""
	}
	p := tx.Parties[id]
	if p == nil || p.Revision != revision || p.LeaderPlayerID != leader || p.RosterLocked || p.ReadyCheckID != "" || len(p.Members) != len(members) {
		return false
	}
	for _, player := range members {
		m, found := p.Members[player]
		if !found || !m.Online || tx.PlayerParties[player] != id {
			return false
		}
	}
	return true
}

// reconcileChallenges在名单/队长/认证变化或截止事件后释放邀请；双方均保持可编辑，接受时再次原子校验。
func reconcileChallenges(tx *MatchmakingTransaction, now time.Time) {
	for id, c := range tx.Queue.Challenges {
		if c.Phase != matchmaking.PhaseChallengeInvitation {
			continue
		}
		if !now.Before(c.DeadlineUTC) {
			tx.Queue.FinishChallenge(id, "MATCH_DEADLINE_EXPIRED", now)
			continue
		}
		if !challengeTeamUnchanged(tx, c.PartyAID, c.PartyARevision, c.OwnerPlayerID, c.TeamAPlayerIDs) || !challengeTeamUnchanged(tx, c.PartyBID, c.PartyBRevision, c.TargetPlayerID, c.TeamBPlayerIDs) {
			tx.Queue.FinishChallenge(id, "MATCH_REVISION_CONFLICT", now)
			continue
		}
		if actor, found := tx.Queue.Contexts[c.OwnerPlayerID]; found && actor.SessionID != c.OwnerSessionID {
			tx.Queue.FinishChallenge(id, "MATCH_CHARACTER_REQUIRED", now)
		}
	}
}

func createChallenge(tx *MatchmakingTransaction, actor matchmaking.ActorContext, command matchmaking.Command, id string, now time.Time) error {
	if !validMatchID(command.TargetPlayerID) || command.TargetPlayerID == actor.PlayerID {
		return errors.New("MATCH_COMMAND_INVALID: 约战目标无效")
	}
	aID, aRevision, owner, a, err := challengeTeam(tx, actor.PlayerID, command.ArenaModeID)
	if err != nil {
		return err
	}
	if owner != actor.PlayerID {
		return errors.New("PARTY_NOT_LEADER: 只有队长可发起约战")
	}
	if command.PartyID != aID {
		return errors.New("MATCH_PARTY_REQUIRED: 发起者队伍与真实索引不一致")
	}
	if command.ExpectedRevision != aRevision {
		return errors.New("PARTY_REVISION_CONFLICT: 发起队伍已变化")
	}
	bID, bRevision, target, b, err := challengeTeam(tx, command.TargetPlayerID, command.ArenaModeID)
	if err != nil {
		return err
	}
	if (aID != "" && aID == bID) || owner == target {
		return errors.New("MATCH_COMMAND_INVALID: 不能邀请本队")
	}
	if len(tx.Queue.Challenges) >= 4*matchmaking.MaxActiveTickets {
		return errors.New("MATCH_QUEUE_CAPACITY: 邀请恢复窗口已满")
	}
	players := append(append([]string{}, a...), b...)
	seen := map[string]bool{}
	for _, player := range players {
		if seen[player] || tx.Queue.Reservations[player] != "" {
			return errors.New("MATCH_PLAYER_RESERVED: 玩家已有邀请、报名或比赛")
		}
		seen[player] = true
	}
	c := matchmaking.Challenge{ID: id, Revision: 1, OwnerPlayerID: owner, TargetPlayerID: target, OwnerSessionID: actor.SessionID, ArenaModeID: command.ArenaModeID, RegionID: command.RegionID, PartyAID: aID, PartyBID: bID, PartyARevision: aRevision, PartyBRevision: bRevision, TeamAPlayerIDs: a, TeamBPlayerIDs: b, Phase: matchmaking.PhaseChallengeInvitation, DeadlineUTC: now.Add(matchmaking.ChallengeLifetime), UpdatedAtUTC: now}
	tx.Queue.Challenges[id] = c
	for _, player := range players {
		tx.Queue.Reservations[player] = id
	}
	tx.Queue.Revision++
	return nil
}

func replyChallenge(tx *MatchmakingTransaction, actor matchmaking.ActorContext, command matchmaking.Command, now time.Time) error {
	c, found := tx.Queue.Challenges[command.ChallengeID]
	if !found {
		return errors.New("MATCH_NOT_FOUND: 约战邀请不存在")
	}
	if (command.Action == "challengeCancel" && c.OwnerPlayerID != actor.PlayerID) || (command.Action == "challengeReply" && c.TargetPlayerID != actor.PlayerID) {
		return errors.New("MATCH_FORBIDDEN: 仅邀请本人队长可操作")
	}
	if c.Phase != matchmaking.PhaseChallengeInvitation {
		return errors.New("MATCH_PHASE_CONFLICT: 邀请已经终结")
	}
	if c.Revision != command.ExpectedRevision {
		return errors.New("MATCH_REVISION_CONFLICT: 邀请已变化")
	}
	if !now.Before(c.DeadlineUTC) {
		return errors.New("MATCH_DEADLINE_EXPIRED: 邀请已到期")
	}
	if command.Action == "challengeCancel" && actor.SessionID != c.OwnerSessionID {
		return errors.New("MATCH_CHARACTER_REQUIRED: 发起会话已经变化")
	}
	if command.Action == "challengeCancel" || !command.Accept {
		reason := "MATCH_DECLINED"
		if command.Action == "challengeCancel" {
			reason = "MATCH_CANCELLED"
		}
		tx.Queue.FinishChallenge(c.ID, reason, now)
		return nil
	}
	if !challengeTeamUnchanged(tx, c.PartyAID, c.PartyARevision, c.OwnerPlayerID, c.TeamAPlayerIDs) || !challengeTeamUnchanged(tx, c.PartyBID, c.PartyBRevision, c.TargetPlayerID, c.TeamBPlayerIDs) {
		return errors.New("MATCH_REVISION_CONFLICT: 邀请名单或队长已变化")
	}
	for _, player := range c.PlayerIDs() {
		if tx.Queue.Reservations[player] != c.ID {
			return errors.New("MATCH_PLAYER_RESERVED: 邀请资格已变化")
		}
	}
	// 同一事务撤销邀请索引并创建双方原子票据；任何失败仓储均回滚全部变化。
	for _, player := range c.PlayerIDs() {
		delete(tx.Queue.Reservations, player)
	}
	teamTickets := []string{c.ID + "-a", c.ID + "-b"}
	for i, members := range [][]string{c.TeamAPlayerIDs, c.TeamBPlayerIDs} {
		partyID := []string{c.PartyAID, c.PartyBID}[i]
		owner := []string{c.OwnerPlayerID, c.TargetPlayerID}[i]
		ticket, err := matchmaking.NewTicket(matchmaking.CreateTicketRequest{TicketID: teamTickets[i], GameID: "divine-beasts", ArenaModeID: c.ArenaModeID, PartyID: partyID, PartyMemberIDs: members, Region: c.RegionID})
		if err != nil {
			return err
		}
		if err := tx.Queue.Enqueue(matchmaking.QueueEntry{Ticket: ticket, OwnerPlayerID: owner, ClientRequestID: c.ID, Revision: 1, Phase: matchmaking.PhaseSearching}, now); err != nil {
			return err
		}
		if p := tx.Parties[partyID]; p != nil {
			if err := p.SelectArenaMode(owner, c.ArenaModeID); err != nil {
				return err
			}
			p.LockRoster()
		}
	}
	candidate, err := tx.Queue.FormFixedCandidate(c.ID+"-candidate", c.ArenaModeID, c.RegionID, teamTickets[:1], teamTickets[1:], now)
	if err != nil {
		return err
	}
	candidate.ChallengeID = c.ID
	tx.Queue.Candidates[candidate.ID] = candidate
	c.TargetSessionID = actor.SessionID
	c.Phase = matchmaking.PhaseCompleted
	c.Revision++
	c.UpdatedAtUTC = now
	tx.Queue.Challenges[c.ID] = c
	return nil
}

func projectChallenge(snapshot *matchmaking.PlayerSnapshot, c matchmaking.Challenge, actor string, now time.Time) {
	snapshot.State = c.Phase
	snapshot.Revision = c.Revision
	snapshot.ArenaModeID = c.ArenaModeID
	snapshot.RegionID = c.RegionID
	snapshot.ReasonCode = c.ReasonCode
	if c.Phase != matchmaking.PhaseChallengeInvitation {
		return
	}
	deadline := c.DeadlineUTC
	snapshot.DeadlineUTC = &deadline
	canReply := actor == c.TargetPlayerID && now.Before(c.DeadlineUTC)
	canCancel := actor == c.OwnerPlayerID && now.Before(c.DeadlineUTC)
	snapshot.Challenge = &matchmaking.ChallengeSnapshot{ChallengeID: c.ID, Revision: c.Revision, OwnerPlayerID: c.OwnerPlayerID, TargetPlayerID: c.TargetPlayerID, ArenaModeID: c.ArenaModeID, RegionID: c.RegionID, DeadlineUTC: c.DeadlineUTC, CanReply: canReply, CanCancel: canCancel, TeamAPlayerIDs: append([]string{}, c.TeamAPlayerIDs...), TeamBPlayerIDs: append([]string{}, c.TeamBPlayerIDs...)}
	for i, members := range [][]string{c.TeamAPlayerIDs, c.TeamBPlayerIDs} {
		for _, player := range members {
			snapshot.Participants = append(snapshot.Participants, matchmaking.ParticipantSnapshot{PlayerID: player, TeamID: []string{"team-a", "team-b"}[i]})
		}
	}
}
