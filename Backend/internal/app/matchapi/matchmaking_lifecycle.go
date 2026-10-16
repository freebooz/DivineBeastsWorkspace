// 本文件把真实队伍准备、原子报名、逐人候选回复与本人观察接入同一持久队列。
// 不在事务中调用控制面；只有持久编排器完成全员签票/提交后才允许发布迁移，不让Widget自行选地址。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"errors"
	"sort"
	"strings"
	"time"
)

// MatchmakingSettings由真实组合根从已登记内容配置取得；开发地图必须显式标记Development，不冒充正式英雄/资源验收。
type MatchmakingSettings struct {
	Enabled             bool     // 未完成内容装配时关闭新报名，但已有状态/取消仍可恢复。
	Development         bool     // 本地开发内容边界，不能用于宣布ProductionReady。
	MapID               string   // 已创建真实竞技地图的逻辑定义身份。
	ContentRevision     string   // 已保存/验证内容修订。
	ProjectRuleRevision int32    // 批准单局歼灭规则修订。
	HeroCatalogRevision int32    // 已发布英雄目录修订，和UE相同整数。
	AllowedRegions      []string // 有限已配置区域，不接受任意分区填充。
}

// ConfigureMatchmaking只能由组合根在开始服务前调用；配置不完整失败关闭，而不是从测试默认值猜正式地图。
func (s *Service) ConfigureMatchmaking(settings MatchmakingSettings) error {
	if settings.Enabled && (settings.MapID == "" || settings.ContentRevision == "" || settings.ProjectRuleRevision < 1 || len(settings.AllowedRegions) == 0) {
		return errors.New("MATCH_CONTENT_NOT_CONFIGURED: 地图、规则、内容和区域必须完整")
	}
	s.matchmakingSettings = &settings
	return nil
}

// CommandMatchmaking主体/会话/角色由Gateway验证；所有写入都带条件版本和主体域幂等键，重试不能扩大名单。
func (s *Service) CommandMatchmaking(ctx context.Context, actor matchmaking.ActorContext, command matchmaking.Command) (matchmaking.PlayerSnapshot, error) {
	if !validMatchID(actor.PlayerID) || !validMatchID(command.ClientRequestID) || command.ExpectedRevision < 0 {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_COMMAND_INVALID: 命令主体或版本无效")
	}
	if command.Action != "start" && command.Action != "cancel" && command.Action != "reply" && command.Action != "ready" {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_COMMAND_INVALID: 未注册命令")
	}
	if command.Action == "start" {
		cfg := s.matchmakingSettings
		if cfg == nil || !cfg.Enabled {
			return matchmaking.PlayerSnapshot{}, errors.New("MATCH_CONTENT_NOT_CONFIGURED: 竞技内容未装配")
		}
		regionAllowed := false
		for _, region := range cfg.AllowedRegions {
			regionAllowed = regionAllowed || region == command.RegionID
		}
		if !regionAllowed {
			return matchmaking.PlayerSnapshot{}, errors.New("MATCH_COMMAND_INVALID: 区域未配置")
		}
	}
	if (command.Action == "start" || command.Action == "reply") && (!validMatchID(actor.SessionID) || !validMatchID(actor.CharacterID) || !validMatchID(actor.HeroDefinitionID) || actor.CharacterRevision < 1) {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_CHARACTER_REQUIRED: 必须使用已验证会话和角色")
	}
	if len(command.PartyID) > 128 || len(command.ArenaModeID) > 128 || len(command.RegionID) > 128 || len(command.CandidateID) > 128 || len(command.TicketID) > 128 {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_COMMAND_INVALID: 字段超过限制")
	}
	repository, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 未装配匹配事务")
	}
	operationKey := scopedOperationKey(actor.PlayerID, "match-command/"+command.Action, command.ClientRequestID)
	fingerprint := fingerprintOf(command)
	// 身份在事务外生成，WATCH重试不产生新的副作用；只有start实际使用此ID。
	ticketID := ""
	if command.Action == "start" {
		ticketID = s.nextID("matchmaking")
	}
	err := repository.UpdateMatchmaking(ctx, []string{command.PartyID}, []string{actor.PlayerID}, func(tx *MatchmakingTransaction) error {
		now := time.Now().UTC()
		tx.Queue.Prune(now)
		if receipt, found := tx.Queue.Receipts[operationKey]; found {
			if receipt.Fingerprint != fingerprint {
				return errors.New("MATCH_IDEMPOTENCY_CONFLICT: 同一命令对应不同内容")
			}
			return nil
		}
		if len(tx.Queue.Receipts) >= matchmaking.MaxOperationReceipts {
			return errors.New("MATCH_QUEUE_CAPACITY: 命令恢复窗口已满")
		}
		affectedTicket := command.TicketID
		switch command.Action {
		case "start":
			if command.PartyID == "" && tx.PlayerParties[actor.PlayerID] != "" {
				return errors.New("MATCH_PARTY_REQUIRED: 已组队玩家必须整队报名")
			}
			members := []string{actor.PlayerID}
			phase := matchmaking.PhaseSearching
			var deadline time.Time
			if command.PartyID != "" {
				p := tx.Parties[command.PartyID]
				if p == nil {
					return ErrPartyNotFound
				}
				p.RenewPresence(actor.PlayerID, now)
				if p.LeaderPlayerID != actor.PlayerID {
					return errors.New("PARTY_NOT_LEADER: 只有队长可开始")
				}
				if p.Revision != command.ExpectedRevision {
					return errors.New("PARTY_REVISION_CONFLICT: 队伍已变化")
				}
				members = nil
				for player := range p.Members {
					if tx.PlayerParties[player] != p.ID {
						return errors.New("PARTY_INDEX_INVALID: 成员索引不一致")
					}
					members = append(members, player)
				}
				sort.Strings(members)
				if err := p.BeginReadyCheck(actor.PlayerID, command.ArenaModeID, ticketID, now); err != nil {
					return err
				}
				deadline = p.ReadyDeadlineUTC
				phase = matchmaking.PhaseReadyCheck
			} else if command.ExpectedRevision != 0 {
				return errors.New("MATCH_REVISION_CONFLICT: 单人首次报名版本必须为0")
			}
			ticket, err := matchmaking.NewTicket(matchmaking.CreateTicketRequest{TicketID: ticketID, GameID: "divine-beasts", ArenaModeID: command.ArenaModeID, PartyID: command.PartyID, PartyMemberIDs: members, Region: command.RegionID})
			if err != nil {
				return err
			}
			if err := tx.Queue.Enqueue(matchmaking.QueueEntry{Ticket: ticket, OwnerPlayerID: actor.PlayerID, ClientRequestID: command.ClientRequestID, RequestFingerprint: fingerprint, Revision: 1, Phase: phase, DeadlineUTC: deadline}, now); err != nil {
				return err
			}
			affectedTicket = ticketID
		case "cancel":
			if err := tx.Queue.Cancel(actor.PlayerID, command.TicketID, command.ExpectedRevision, now); err != nil {
				return err
			}
		case "reply":
			if existing, found := tx.Queue.Contexts[actor.PlayerID]; found {
				if candidate, known := tx.Queue.Candidates[command.CandidateID]; known {
					for _, slot := range candidate.Roster {
						if slot.PlayerID == actor.PlayerID && slot.Accepted && (existing.CharacterID != actor.CharacterID || existing.SessionID != actor.SessionID || existing.HeroDefinitionID != actor.HeroDefinitionID) {
							return errors.New("MATCH_CHARACTER_REQUIRED: 已确认角色或会话不能被覆盖")
						}
					}
				}
			}
			if err := tx.Queue.Reply(actor.PlayerID, command.CandidateID, command.ExpectedRevision, command.Accept, now); err != nil {
				return err
			}
			affectedTicket = tx.Queue.Reservations[actor.PlayerID]
		case "ready":
			entry, found := tx.Queue.Entries[command.TicketID]
			if !found {
				return errors.New("MATCH_NOT_FOUND: 准备会话不存在")
			}
			if entry.Phase != matchmaking.PhaseReadyCheck || entry.Revision != command.ExpectedRevision {
				return errors.New("MATCH_REVISION_CONFLICT: 准备会话已变化")
			}
			if tx.Queue.Reservations[actor.PlayerID] != entry.Ticket.TicketID {
				return errors.New("MATCH_FORBIDDEN: 本人不在准备名单")
			}
			p := tx.Parties[entry.Ticket.PartyID]
			if p == nil || p.ReadyCheckID != entry.Ticket.TicketID || !now.Before(entry.DeadlineUTC) {
				return errors.New("MATCH_DEADLINE_EXPIRED: 准备已失效")
			}
			if !command.Accept {
				if err := tx.Queue.Cancel(entry.OwnerPlayerID, entry.Ticket.TicketID, entry.Revision, now); err != nil {
					return err
				}
			} else if err := p.SetReady(actor.PlayerID, true); err != nil {
				return err
			}
		}
		if actor.SessionID != "" && actor.CharacterID != "" {
			tx.Queue.Contexts[actor.PlayerID] = actor
		}
		if p := tx.Parties[tx.PlayerParties[actor.PlayerID]]; p != nil {
			p.RenewPresence(actor.PlayerID, now)
		}
		if err := s.advanceMatchmaking(tx, now); err != nil {
			return err
		}
		tx.Queue.Receipts[operationKey] = matchmaking.QueueReceipt{Fingerprint: fingerprint, TicketID: affectedTicket, ExpiresAtUTC: now.Add(matchmaking.TerminalRetention)}
		return nil
	})
	if err != nil {
		return matchmaking.PlayerSnapshot{}, err
	}
	return s.ReadMatchmakingState(ctx, actor.PlayerID)
}

// advanceMatchmaking在真实写事件/恢复中消费队列；准备名单或模式变化即取消，全部准备才锁队并组成候选。
func (s *Service) advanceMatchmaking(tx *MatchmakingTransaction, now time.Time) error {
	tx.Queue.Expire(now)
	for _, p := range tx.Parties {
		p.ReconcilePresence(now)
	}
	for id, entry := range tx.Queue.Entries {
		p := tx.Parties[entry.Ticket.PartyID]
		if p == nil || matchmaking.IsTerminal(entry.Phase) {
			continue
		}
		if entry.Phase != matchmaking.PhaseReadyCheck && entry.Phase != matchmaking.PhaseSearching && entry.Phase != matchmaking.PhaseConfirmation {
			continue
		}
		unavailable := false
		for _, player := range entry.Ticket.PartyMemberIDs {
			m, ok := p.Members[player]
			unavailable = unavailable || !ok || !m.Online
		}
		if unavailable {
			if err := tx.Queue.Cancel(entry.OwnerPlayerID, id, entry.Revision, now); err != nil {
				return err
			}
		}
	}

	for id, entry := range tx.Queue.Entries {
		if entry.Phase != matchmaking.PhaseReadyCheck {
			continue
		}
		p := tx.Parties[entry.Ticket.PartyID]
		valid := p != nil && p.ReadyCheckID == id && p.SelectedArenaModeID == entry.Ticket.ArenaModeID && len(p.Members) == len(entry.Ticket.PartyMemberIDs)
		allReady := valid
		if valid {
			for _, player := range entry.Ticket.PartyMemberIDs {
				member, found := p.Members[player]
				valid = valid && found
				allReady = allReady && found && member.Online && member.Ready
			}
		}
		if !valid {
			if err := tx.Queue.Cancel(entry.OwnerPlayerID, id, entry.Revision, now); err != nil {
				return err
			}
			continue
		}
		if allReady {
			p.LockRoster()
			// 准备已全部提交，关闭准备弹窗；保留成员Ready事实供名单展示，不再沿用过期准备身份。
			p.ReadyCheckID = ""
			p.ReadyDeadlineUTC = time.Time{}
			entry.Phase = matchmaking.PhaseSearching
			entry.Ticket.State = entry.Phase
			entry.Revision++
			entry.DeadlineUTC = now.Add(15 * time.Minute)
			entry.UpdatedAtUTC = now
			tx.Queue.Entries[id] = entry
			tx.Queue.Revision++
		}
	}
	// 固定排序只决定FIFO候选处理顺序；分队仍以模式显式人数和不可拆分票据为准。
	partitions := map[string]matchmaking.Ticket{}
	keys := []string{}
	for _, entry := range tx.Queue.Entries {
		if entry.Phase == matchmaking.PhaseSearching {
			key := entry.Ticket.ArenaModeID + "\n" + entry.Ticket.Region
			if _, found := partitions[key]; !found {
				keys = append(keys, key)
				partitions[key] = entry.Ticket
			}
		}
	}
	sort.Strings(keys)
	formed := 0
	for _, key := range keys {
		ticket := partitions[key]
		for formed < 32 {
			id := "candidate-" + fingerprintOf([]any{ticket.ArenaModeID, ticket.Region, tx.Queue.Revision})[:32]
			_, ok, err := tx.Queue.FormCandidate(id, ticket.ArenaModeID, ticket.Region, now)
			if err != nil {
				return err
			}
			if !ok {
				break
			}
			formed++
		}
	}
	unlockFinishedParties(tx)
	for _, p := range tx.Parties {
		if err := p.TransferOfflineLeadership(now); err != nil {
			return err
		}
	}
	for id, p := range tx.Parties {
		if p.ReadyCheckID != "" && tx.Queue.Reservations[p.LeaderPlayerID] == "" {
			p.CancelReadyCheck()
			tx.Parties[id] = p
		}
	}
	return nil
}

// AdvanceMatchmaking由事件驱动消费者在启动/队伍写通知/期限到达时调用；正常请求也使用同一状态迁移。
func (s *Service) AdvanceMatchmaking(ctx context.Context) error {
	repo, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return errors.New("MATCH_ATOMIC_STORE_REQUIRED: 未装配事务")
	}
	return repo.UpdateMatchmaking(ctx, nil, nil, func(tx *MatchmakingTransaction) error { return s.advanceMatchmaking(tx, time.Now().UTC()) })
}

// ReadMatchmakingState只投影本人预留或最近终态；不能通过TicketID枚举别人，失败不转换为idle。
func (s *Service) ReadMatchmakingState(ctx context.Context, actor string) (matchmaking.PlayerSnapshot, error) {
	if !validMatchID(actor) {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_COMMAND_INVALID: 主体无效")
	}
	repo, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return matchmaking.PlayerSnapshot{}, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 未装配事务")
	}
	q, err := repo.ReadMatchmaking(ctx)
	if err != nil {
		return matchmaking.PlayerSnapshot{}, err
	}
	now := time.Now().UTC()
	snapshot := matchmaking.PlayerSnapshot{State: matchmaking.PhaseIdle, ServerTimeUTC: now, Participants: []matchmaking.ParticipantSnapshot{}}
	if cfg := s.matchmakingSettings; cfg != nil {
		snapshot.ServiceReady = cfg.Enabled && s.matchmakingRuntime != nil
		snapshot.Development = cfg.Development
		if len(cfg.AllowedRegions) > 0 {
			snapshot.RegionID = cfg.AllowedRegions[0]
		}
	}
	id := q.Reservations[actor]
	if id == "" {
		var latest time.Time
		for candidateID, entry := range q.Entries {
			if !matchmaking.IsTerminal(entry.Phase) || entry.UpdatedAtUTC.Before(latest) {
				continue
			}
			for _, player := range entry.Ticket.PartyMemberIDs {
				if player == actor {
					id = candidateID
					latest = entry.UpdatedAtUTC
				}
			}
		}
	}
	entry, found := q.Entries[id]
	if !found {
		return snapshot, nil
	}
	snapshot.State = entry.Phase
	snapshot.TicketID = id
	snapshot.PartyID = entry.Ticket.PartyID
	snapshot.ArenaModeID = entry.Ticket.ArenaModeID
	snapshot.RegionID = entry.Ticket.Region
	snapshot.CandidateID = entry.CandidateID
	snapshot.Revision = entry.Revision
	snapshot.ReasonCode = entry.ReasonCode
	if !entry.DeadlineUTC.IsZero() {
		deadline := entry.DeadlineUTC
		snapshot.DeadlineUTC = &deadline
	}
	snapshot.CanCancel = actor == entry.OwnerPlayerID && (entry.Phase == matchmaking.PhaseSearching || entry.Phase == matchmaking.PhaseReadyCheck || entry.Phase == matchmaking.PhaseConfirmation)
	snapshot.CanReady = entry.Phase == matchmaking.PhaseReadyCheck && now.Before(entry.DeadlineUTC)
	if c, found := q.Candidates[entry.CandidateID]; found {
		snapshot.CandidateRevision = c.Revision
		for _, slot := range c.Roster {
			snapshot.Participants = append(snapshot.Participants, matchmaking.ParticipantSnapshot{PlayerID: slot.PlayerID, TeamID: slot.TeamID, Ready: true, Accepted: slot.Accepted})
			if slot.PlayerID == actor {
				snapshot.CanReply = c.Phase == matchmaking.PhaseConfirmation && !slot.Accepted && now.Before(c.DeadlineUTC)
			}
		}
	} else {
		var readyPlayers = map[string]bool{}
		if entry.Ticket.PartyID != "" {
			p, err := s.parties.Get(ctx, entry.Ticket.PartyID)
			if err != nil {
				return matchmaking.PlayerSnapshot{}, err
			}
			for player, member := range p.Members {
				readyPlayers[player] = member.Ready
			}
		}
		for _, player := range entry.Ticket.PartyMemberIDs {
			snapshot.Participants = append(snapshot.Participants, matchmaking.ParticipantSnapshot{PlayerID: player, Ready: readyPlayers[player]})
		}
	}
	return snapshot, nil
}

// validMatchID只接受有界非空逻辑身份，不标准化幂等内容，也不允许控制字符进入索引。
func validMatchID(value string) bool {
	if value == "" || len(value) > 128 || strings.TrimSpace(value) != value {
		return false
	}
	for _, c := range value {
		if c < 32 || c == 127 {
			return false
		}
	}
	return true
}
