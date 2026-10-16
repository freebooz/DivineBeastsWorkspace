// 本文件实现真实匹配编排：先持久意图，再调用控制面；全员签票提交之前不向客户端发布任何迁移秘密。
// 作用域归MatchService，PG租约和唯一请求身份隔离并发；Redis队列仅投影阶段，不承担签票权威。
package matchservice

import (
	"context"
	"crypto/sha256"
	"divinebeasts/backend/internal/app/gameservercontrol"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/hex"
	"encoding/json"
	"errors"
	"strings"
	"time"
)

// CandidateStore只供可信编排读固定名单、发布已持久步骤；没有签票秘密进入其事件投影。
type CandidateStore interface {
	ReadCandidateContext(context.Context, string) (matchmaking.Candidate, map[string]matchmaking.ActorContext, map[string]string, error)
	CommitCandidatePhase(context.Context, string, string, string, time.Time) error
}

// RuntimeControl使用有Context的真实控制面调用；所有网络请求必须受工作步骤租约截止时间约束。
type RuntimeControl interface {
	AllocateMainArenaContext(context.Context, gameservercontrol.AllocateMainArenaInput) (gameservercontract.Assignment, error)
	IssueTransferContext(context.Context, gameservercontrol.IssueTransferInput) (servertransfer.Ticket, error)
	GetArenaStatus(context.Context, string) (gameservercontrol.ArenaStatus, error)
	CancelArena(context.Context, string) error
	FinalizeArena(context.Context, string) error
	RevokeTransfer(context.Context, string) error
	AllocateWorldTransfer(context.Context, gameservercontrol.AllocateWorldTransferInput) (gameservercontrol.WorldTransferResult, error)
}

// RuntimeConfig只能由正式内容/区域配置装配；Development标记保留到运维配置，不能据此宣布正式资源完成。
type RuntimeConfig struct {
	MapID               string
	ContentRevision     string
	RuleRevision        int32
	HeroCatalogRevision int32
}

// Runtime以持久Operation为唯一跨服务步骤权威；对象不保存进程级玩家或当前比赛缓存。
type Runtime struct {
	operations OperationStore
	queue      CandidateStore
	control    RuntimeControl
	config     RuntimeConfig
	now        func() time.Time
	nextID     func(string) string
}

// NewRuntime拒绝缺失仓储、真实控制面或内容；不存在成功的空实现/自动内存生产回退。
func NewRuntime(store OperationStore, queue CandidateStore, control RuntimeControl, cfg RuntimeConfig, nextID func(string) string) (*Runtime, error) {
	if store == nil || queue == nil || control == nil || nextID == nil || cfg.MapID == "" || cfg.ContentRevision == "" || cfg.RuleRevision < 1 || cfg.HeroCatalogRevision < 1 {
		return nil, errors.New("MATCH_RUNTIME_INVALID: 真实依赖或内容缺失")
	}
	return &Runtime{store, queue, control, cfg, func() time.Time { return time.Now().UTC() }, nextID}, nil
}

// stableStepID对固定意图生成有限长度身份，不包含票据秘密；重启和未知响应重试仍为同一次副作用。
func stableStepID(parts ...string) string {
	hash := sha256.Sum256([]byte(strings.Join(parts, "\x00")))
	return "step-" + hex.EncodeToString(hash[:20])
}

// proposal只在首次处理时读取已确认名单；已有记录始终恢复原上下文，不用新快照重签其他角色。
func (r *Runtime) proposal(ctx context.Context, id string) (Operation, error) {
	old, found, err := r.operations.GetOperation(ctx, id)
	if err != nil {
		return Operation{}, err
	}
	if found {
		return old, nil
	}
	candidate, contexts, parties, err := r.queue.ReadCandidateContext(ctx, id)
	if err != nil {
		return Operation{}, err
	}
	if candidate.Phase != matchmaking.PhaseAllocating {
		return Operation{}, errors.New("MATCH_PHASE_CONFLICT: 尚未全员确认")
	}
	now := r.now()
	value := Operation{ID: id, MatchID: stableStepID("match", id), Candidate: candidate, Parties: parties, Contexts: contexts, MapID: r.config.MapID, ContentRevision: r.config.ContentRevision, RuleRevision: r.config.RuleRevision, HeroCatalogRevision: r.config.HeroCatalogRevision, Stage: "allocating", TicketIDs: map[string]string{}, Tickets: map[string]servertransfer.Ticket{}, Returns: map[string]ReturnIntent{}, DeadlineUTC: candidate.DeadlineUTC, CreatedAtUTC: now, UpdatedAtUTC: now}
	for _, slot := range candidate.Roster {
		value.TicketIDs[slot.PlayerID] = stableStepID("arena-entry", id, slot.PlayerID)
	}
	value.InputFingerprint, err = OperationInputFingerprint(value)
	return value, err
}

// ProcessCandidate启动/真实通知/恢复期限时执行；忙租约由另一工作者负责，失败不发布固定成功或释放有效票据。
func (r *Runtime) ProcessCandidate(ctx context.Context, candidate matchmaking.Candidate) error {
	proposal, err := r.proposal(ctx, candidate.ID)
	if err != nil {
		return err
	}
	value, err := r.operations.ClaimOperation(ctx, proposal, r.nextID("match-worker"), r.now(), 20*time.Second)
	if errors.Is(err, ErrOperationBusy) {
		return nil
	}
	if err != nil {
		return err
	}
	stepContext, stopStep := context.WithDeadline(ctx, value.LeaseUntilUTC)
	defer stopStep()
	ctx = stepContext
	defer func() {
		cleanup, cancel := context.WithTimeout(context.WithoutCancel(ctx), 2*time.Second)
		defer cancel()
		_ = r.operations.ReleaseOperation(cleanup, value.ID, value.LeaseOwner, value.Fence)
	}()
	// 每轮最多覆盖分配、逐人签票及一次状态汇合；每次外部调用前都续租并受该租约截止时间约束。
	save := func() error {
		var e error
		value, e = r.operations.SaveOperation(ctx, value, r.now(), 20*time.Second)
		return e
	}
	if (value.Stage == "allocating" || value.Stage == "signing") && !r.now().Before(value.DeadlineUTC) {
		value.Stage = "rollingBack"
		value.FaultCode = "MATCH_ALLOCATION_TIMEOUT"
		if err = save(); err != nil {
			return err
		}
	}
	if value.Stage == "allocating" {
		roster := []gameservercontract.RosterPlayer{}

		for _, slot := range value.Candidate.Roster {
			actor := value.Contexts[slot.PlayerID]
			roster = append(roster, gameservercontract.RosterPlayer{PlayerID: slot.PlayerID, CharacterID: actor.CharacterID, TeamID: slot.TeamID, PartyID: value.Parties[slot.PlayerID], SlotIndex: len(roster), HeroDefinitionID: actor.HeroDefinitionID})
		}
		call, cancel := context.WithDeadline(ctx, minDeadline(value.LeaseUntilUTC, value.DeadlineUTC))
		assignment, e := r.control.AllocateMainArenaContext(call, gameservercontrol.AllocateMainArenaInput{WorldID: value.MapID, ProjectRuleRevision: value.RuleRevision, ContentRevision: value.ContentRevision, HeroCatalogRevision: value.HeroCatalogRevision, MatchID: value.MatchID, ArenaModeID: value.Candidate.ArenaModeID, MapID: value.MapID, RegionID: value.Candidate.RegionID, Roster: roster})
		cancel()
		if e != nil {
			return e
		}
		if e = assignment.Validate(); e != nil {
			return e
		}
		value.Assignment = &assignment
		value.Stage = "signing"
		if err = save(); err != nil {
			return err
		}
	}
	if value.Stage == "signing" {
		for _, slot := range value.Candidate.Roster {
			if _, found := value.Tickets[slot.PlayerID]; found {
				continue
			}
			if !r.now().Before(value.DeadlineUTC) {
				return errors.New("MATCH_ALLOCATION_TIMEOUT: 全员签票预算已尽")
			}
			if err = save(); err != nil {
				return err
			}
			actor := value.Contexts[slot.PlayerID]
			call, cancel := context.WithDeadline(ctx, minDeadline(value.LeaseUntilUTC, value.DeadlineUTC))
			ticket, e := r.control.IssueTransferContext(call, gameservercontrol.IssueTransferInput{TicketID: value.TicketIDs[slot.PlayerID], AssignmentID: "match:" + value.MatchID, GameID: value.Candidate.GameID, PlayerID: slot.PlayerID, SessionID: actor.SessionID, SourceGameServerID: actor.SourceGameServerID, DestinationGameServerID: value.Assignment.GameServerID, DestinationWorldID: value.MapID, DestinationExperienceID: gameservercontract.ExperienceMainArenaMain, MatchID: value.MatchID, TTL: 60 * time.Second})
			cancel()
			if e != nil {
				return e
			}
			if ticket.TicketID != value.TicketIDs[slot.PlayerID] || ticket.PlayerID != slot.PlayerID || ticket.SessionID != actor.SessionID || ticket.MatchID != value.MatchID || ticket.DestinationGameServerID != value.Assignment.GameServerID || ticket.Signature == "" {
				return errors.New("MATCH_TICKET_INVALID: 控制面返回不一致票据")
			}
			value.Tickets[slot.PlayerID] = ticket
			if err = save(); err != nil {
				return err
			}
		}
		value.Stage = "committed"
		value.DeadlineUTC = r.now().Add(60 * time.Second)
		for _, ticket := range value.Tickets {
			expires := ticket.ExpiresAt
			if expires.Before(value.DeadlineUTC) {
				value.DeadlineUTC = expires
			}
		}
		if err = save(); err != nil {
			return err
		}
	}
	if err = r.project(ctx, value); err != nil {
		return err
	}
	if value.Stage == "committed" || value.Stage == "inMatch" || value.Stage == "returning" {
		if err = save(); err != nil {
			return err
		}
		call, cancel := context.WithDeadline(ctx, value.LeaseUntilUTC)
		status, e := r.control.GetArenaStatus(call, value.MatchID)
		cancel()
		if e != nil {
			return e
		}
		if status.Record.Result != nil || status.Record.State == "resultCommitted" {
			value.Result = status.Record.Result
			value.Stage = "returning"
			value.DeadlineUTC = time.Time{}
			if err = save(); err != nil {
				return err
			}
		} else if value.Stage != "returning" && (status.Record.State == "cancelled" || !status.ServerAlive || ((value.Stage == "committed" || value.Stage == "inMatch") && !value.DeadlineUTC.IsZero() && !r.now().Before(value.DeadlineUTC))) {
			value.Stage = "rollingBack"
			value.FaultCode = "MATCH_ENTRY_FAILED"
			if err = save(); err != nil {
				return err
			}
		} else if value.Stage == "committed" && allAdmitted(value, status.Presences) {
			value.Stage = "inMatch"
			// 单局20分钟加两分钟可信结果传输恢复预算；超时进入故障清理，绝不伪造正常胜负。
			value.DeadlineUTC = r.now().Add(22 * time.Minute)
			if err = save(); err != nil {
				return err
			}
		}
		if value.Stage == "returning" && (!status.ServerAlive || (status.CurrentPlayers == 0 && !status.HeartbeatAtUTC.Before(status.Record.UpdatedAtUTC))) {
			if err = r.project(ctx, value); err != nil {
				return err
			}
			if err = r.control.FinalizeArena(ctx, value.MatchID); err != nil {
				return err
			}
			value.Stage = "completed"
			if err = save(); err != nil {
				return err
			}
		}
	}
	if value.Stage == "rollingBack" {
		// 即使签票响应丢失也撤销全部已持久意图，不能只撤销收到回执的子集。
		for _, ticketID := range value.TicketIDs {
			if err = save(); err != nil {
				return err
			}
			call, cancel := context.WithDeadline(ctx, value.LeaseUntilUTC)
			e := r.control.RevokeTransfer(call, ticketID)
			cancel()
			if e != nil {
				return e
			}
		}
		if err = r.control.CancelArena(ctx, value.MatchID); err != nil {
			return err
		}
		status, e := r.control.GetArenaStatus(ctx, value.MatchID)
		if e != nil {
			return e
		}
		if status.ServerAlive && status.CurrentPlayers > 0 {
			value.Stage = "returning"
			value.DeadlineUTC = time.Time{}
		} else {
			if err = r.control.FinalizeArena(ctx, value.MatchID); err != nil {
				return err
			}
			value.Stage = "cancelled"
		}
		if err = save(); err != nil {
			return err
		}
	}
	return r.project(ctx, value)
}

// project在PG提交后发布无秘密阶段；崩溃后可重复，队列事务的严格前进检查拒绝迟到倒退。
func (r *Runtime) project(ctx context.Context, value Operation) error {
	phase := map[string]string{"allocating": matchmaking.PhaseAllocating, "signing": matchmaking.PhaseAllocating, "committed": matchmaking.PhaseTransferReady, "inMatch": matchmaking.PhaseInMatch, "returning": matchmaking.PhaseReturning, "completed": matchmaking.PhaseCompleted, "cancelled": matchmaking.PhaseFailed}[value.Stage]
	if phase == "" {
		return nil
	}
	return r.queue.CommitCandidatePhase(ctx, value.ID, phase, value.FaultCode, value.DeadlineUTC)
}

func minDeadline(a, b time.Time) time.Time {
	if b.Before(a) {
		return b
	}
	return a
}
func allAdmitted(value Operation, presences []gameservercontrol.PlayerPresence) bool {
	seen := map[string]bool{}
	for _, p := range presences {
		actor, found := value.Contexts[p.PlayerID]
		// 只认可确认时会话或本操作已持久签发的恢复票；新会话不能凭自报Session绕过入场汇合。
		validSession := found && p.SessionID == actor.SessionID && p.TicketID == value.TicketIDs[p.PlayerID]
		if intent, ok := value.Reconnects[p.PlayerID]; found && ok && intent.CharacterID == actor.CharacterID && intent.Migration.TransferTicket != "" && p.TicketID == intent.TicketID && p.SessionID == intent.SessionID {
			validSession = true
		}
		if validSession && p.MatchID == value.MatchID && p.GameServerID == value.Assignment.GameServerID && p.ServerBootID != "" && p.ServerBootID == value.Tickets[p.PlayerID].DestinationServerBootID {
			seen[p.PlayerID] = true
		}
	}
	return len(seen) == len(value.Candidate.Roster)
}

// migrationForTicket只供已认证本人Session消费，不能把序列化票据写入UI、观察事件或日志。
func migrationForTicket(ticket servertransfer.Ticket, character, mapID, region, role string) (matchmaking.Migration, error) {
	payload, err := json.Marshal(ticket)
	if err != nil {
		return matchmaking.Migration{}, err
	}
	return matchmaking.Migration{AssignmentID: ticket.AssignmentID, GameServerID: ticket.DestinationGameServerID, ServerRoleID: role, ExperienceID: ticket.DestinationExperienceID, WorldID: ticket.DestinationWorldID, MapID: mapID, RegionID: region, TicketID: ticket.TicketID, CharacterID: character, SessionID: ticket.SessionID, GameSessionID: ticket.GameSessionID, ServerBootID: ticket.DestinationServerBootID, ProtocolVersion: ticket.DestinationProtocolVersion, SessionEpoch: ticket.SessionEpoch, Endpoint: ticket.DestinationEndpoint, TransferTicket: string(payload)}, nil
}

// GetMigration严格按本人、锁定角色和已提交步骤取票；返回必须重新分配来源体验，不复用历史地址。
func (r *Runtime) GetMigration(ctx context.Context, actor matchmaking.ActorContext, request matchmaking.MigrationRequest) (matchmaking.Migration, error) {
	proposal, err := r.proposal(ctx, request.CandidateID)
	if err != nil {
		return matchmaking.Migration{}, err
	}
	locked, member := proposal.Contexts[actor.PlayerID]
	if !member || locked.CharacterID != actor.CharacterID || locked.HeroDefinitionID != actor.HeroDefinitionID {
		return matchmaking.Migration{}, errors.New("MATCH_FORBIDDEN: 不是本局锁定角色")
	}
	value, err := r.operations.ClaimOperation(ctx, proposal, r.nextID("migration"), r.now(), 20*time.Second)
	if err != nil {
		return matchmaking.Migration{}, err
	}
	stepContext, stopStep := context.WithDeadline(ctx, value.LeaseUntilUTC)
	defer stopStep()
	ctx = stepContext
	defer func() {
		cleanup, cancel := context.WithTimeout(context.WithoutCancel(ctx), 2*time.Second)
		defer cancel()
		_ = r.operations.ReleaseOperation(cleanup, value.ID, value.LeaseOwner, value.Fence)
	}()
	if request.Direction == "arena" {
		if value.Stage != "committed" && value.Stage != "inMatch" {
			return matchmaking.Migration{}, errors.New("MATCH_PHASE_CONFLICT: 尚未全员提交")
		}
		call, cancel := context.WithDeadline(ctx, value.LeaseUntilUTC)
		status, statusErr := r.control.GetArenaStatus(call, value.MatchID)
		cancel()
		if statusErr != nil {
			return matchmaking.Migration{}, statusErr
		}
		if !status.ServerAlive || status.Record.State != "active" || status.Record.ServerBootID == "" {
			return matchmaking.Migration{}, errors.New("MATCH_MIGRATION_EXPIRED: 原竞技实例已结束或失效")
		}
		ticket, found := value.Tickets[actor.PlayerID]
		admitted := false
		for _, presence := range status.Presences {
			if presence.PlayerID == actor.PlayerID && presence.MatchID == value.MatchID && presence.ServerBootID == status.Record.ServerBootID {
				admitted = true
			}
		}
		if found && !admitted && ticket.SessionID == actor.SessionID && r.now().Before(ticket.ExpiresAt) {
			return migrationForTicket(ticket, actor.CharacterID, value.MapID, value.Candidate.RegionID, gameservercontract.RoleMainArena)
		}
		if value.Stage == "committed" && !r.now().Before(value.DeadlineUTC) {
			return matchmaking.Migration{}, errors.New("MATCH_MIGRATION_EXPIRED: 首次全员入场预算已尽")
		}
		// 新认证会话只能恢复同一锁定角色。新票不授予重生：原服务器仍校验原连接、60秒窗口和淘汰状态。
		if value.Reconnects == nil {
			value.Reconnects = map[string]ReturnIntent{}
		}
		intent, exists := value.Reconnects[actor.PlayerID]
		if exists && intent.ClientRequestID == request.ClientRequestID {
			if intent.SessionID != actor.SessionID || intent.CharacterID != actor.CharacterID {
				return matchmaking.Migration{}, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 恢复会话发生变化")
			}
			if intent.Migration.TransferTicket != "" {
				if ticketExpired(intent.Migration.TransferTicket, r.now()) {
					return matchmaking.Migration{}, errors.New("MATCH_MIGRATION_EXPIRED: 恢复票据已过期，请明确重试")
				}
				return intent.Migration, nil
			}
		} else {
			if exists && intent.TicketID != "" {
				if err = r.control.RevokeTransfer(ctx, intent.TicketID); err != nil {
					return matchmaking.Migration{}, err
				}
			}
			if found {
				if err = r.control.RevokeTransfer(ctx, ticket.TicketID); err != nil {
					return matchmaking.Migration{}, err
				}
			}
			intent = ReturnIntent{ClientRequestID: request.ClientRequestID, SessionID: actor.SessionID, CharacterID: actor.CharacterID, TicketID: stableStepID("reconnect", value.ID, actor.PlayerID, request.ClientRequestID)}
			value.Reconnects[actor.PlayerID] = intent
			value, err = r.operations.SaveOperation(ctx, value, r.now(), 20*time.Second)
			if err != nil {
				return matchmaking.Migration{}, err
			}
		}
		call, cancel = context.WithDeadline(ctx, value.LeaseUntilUTC)
		fresh, issueErr := r.control.IssueTransferContext(call, gameservercontrol.IssueTransferInput{TicketID: intent.TicketID, AssignmentID: "match:" + value.MatchID, GameID: value.Candidate.GameID, PlayerID: actor.PlayerID, SessionID: actor.SessionID, SourceGameServerID: locked.SourceGameServerID, DestinationGameServerID: value.Assignment.GameServerID, DestinationWorldID: value.MapID, DestinationExperienceID: gameservercontract.ExperienceMainArenaMain, MatchID: value.MatchID, TTL: 60 * time.Second})
		cancel()
		if issueErr != nil {
			return matchmaking.Migration{}, issueErr
		}
		migration, encodeErr := migrationForTicket(fresh, actor.CharacterID, value.MapID, value.Candidate.RegionID, gameservercontract.RoleMainArena)
		if encodeErr != nil {
			return matchmaking.Migration{}, encodeErr
		}
		intent.Migration = migration
		value.Reconnects[actor.PlayerID] = intent
		_, err = r.operations.SaveOperation(ctx, value, r.now(), 20*time.Second)
		return migration, err
	}
	if request.Direction != "return" || value.Stage != "returning" {
		return matchmaking.Migration{}, errors.New("MATCH_PHASE_CONFLICT: 可信结果或故障清理尚未确认")
	}
	intent, found := value.Returns[actor.PlayerID]
	if found && intent.ClientRequestID == request.ClientRequestID {
		if intent.SessionID != actor.SessionID || intent.CharacterID != actor.CharacterID {
			return matchmaking.Migration{}, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 返回身份变化")
		}
		if intent.Migration.TransferTicket != "" {
			if ticketExpired(intent.Migration.TransferTicket, r.now()) {
				return matchmaking.Migration{}, errors.New("MATCH_MIGRATION_EXPIRED: 返回票据过期，请使用新的逻辑重试")
			}
			return intent.Migration, nil
		}
	}
	if !found || intent.ClientRequestID != request.ClientRequestID {
		if found && intent.TicketID != "" {
			if err = r.control.RevokeTransfer(ctx, intent.TicketID); err != nil {
				return matchmaking.Migration{}, err
			}
		}
		intent = ReturnIntent{ClientRequestID: request.ClientRequestID, SessionID: actor.SessionID, CharacterID: actor.CharacterID, TicketID: stableStepID("return", value.ID, actor.PlayerID, request.ClientRequestID)}
		value.Returns[actor.PlayerID] = intent
		value, err = r.operations.SaveOperation(ctx, value, r.now(), 20*time.Second)
		if err != nil {
			return matchmaking.Migration{}, err
		}
	}
	experience := locked.SourceExperienceID
	role, known := gameservercontract.RoleForExperience(experience)
	if !known || role == gameservercontract.RoleMainArena {
		return matchmaking.Migration{}, errors.New("MATCH_RETURN_NOT_CONFIGURED: 来源体验没有合法安全返回规则")
	}
	worldID, known := safeWorldForExperience(experience)
	if !known {
		return matchmaking.Migration{}, errors.New("MATCH_RETURN_NOT_CONFIGURED: 来源体验缺少世界定义")
	}
	call, cancel := context.WithDeadline(ctx, value.LeaseUntilUTC)
	result, err := r.control.AllocateWorldTransfer(call, gameservercontrol.AllocateWorldTransferInput{World: gameservercontrol.AllocateWorldInput{ExperienceID: experience, WorldID: worldID, RegionID: value.Candidate.RegionID, PlayerSlots: 1}, Transfer: gameservercontrol.IssueTransferInput{TicketID: intent.TicketID, GameID: value.Candidate.GameID, PlayerID: actor.PlayerID, SessionID: actor.SessionID, SourceGameServerID: value.Assignment.GameServerID, TTL: 60 * time.Second}})
	cancel()
	if err != nil {
		return matchmaking.Migration{}, err
	}
	migration, err := migrationForTicket(result.Ticket, actor.CharacterID, worldID, result.Assignment.RegionID, role)
	if err != nil {
		return matchmaking.Migration{}, err
	}
	intent.Migration = migration
	value.Returns[actor.PlayerID] = intent
	value, err = r.operations.SaveOperation(ctx, value, r.now(), 20*time.Second)
	return migration, err
}

func ticketExpired(payload string, now time.Time) bool {
	var ticket servertransfer.Ticket
	return json.Unmarshal([]byte(payload), &ticket) != nil || !now.Before(ticket.ExpiresAt)
}

// safeWorldForExperience与正式世界进入映射一致，只重新使用逻辑体验，永不携带原服务器地址。
func safeWorldForExperience(experience string) (string, bool) {
	switch experience {
	case gameservercontract.ExperienceVillageMain:
		return "divinebeasts.world.village_main@1", true
	case gameservercontract.ExperienceVillageTutorial:
		return "divinebeasts.world.village_tutorial@1", true
	case gameservercontract.ExperienceVillageTraining:
		return "divinebeasts.world.village_training@1", true
	case gameservercontract.ExperienceOpenWorldHub, gameservercontract.ExperienceLobbyMain:
		return "World.OpenWorld.Hub", true
	case gameservercontract.ExperienceOpenWorldMain:
		return "World.OpenWorld.Main", true
	}
	return "", false
}
