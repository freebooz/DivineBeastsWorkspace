// 本文件实现竞技持久控制事实、本人位置和显式排空；结果已存不表示玩家已迁出，不提前复用独占实例。
package gameservercontrol

import (
	"context"
	"crypto/sha256"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sort"
	"time"
)

// ArenaStatus仅可信Match服务读取，包含真实Boot/位置以恢复步骤；不是公开玩家视图，也不含TransferTicket秘密。
type ArenaStatus struct {
	HeartbeatAtUTC time.Time             // 最近真实人数心跳；结束前旧的0人不能证明结束后已迁出。
	Record         ArenaAssignmentRecord // 已确认分配与可信结果。
	ServerAlive    bool                  // 最近心跳及Boot均有效；未知恢复期返回错误而非false。
	CurrentPlayers int                   // Dedicated Server实际心跳人数。
	Presences      []PlayerPresence      // 本轮名单的最新位置，已迁出者指向新实例。
}

// ConfigureArenaLifecycle启动前装配一次，启用持久恢复与结果后安全迁出门禁；不能在请求期间替换仓储。
func (s *Service) ConfigureArenaLifecycle(store ArenaLifecycleStore) error {
	if store == nil || s.arenaLifecycle != nil {
		return errors.New("ARENA_STORE_INVALID: 生命周期仓储缺失或重复装配")
	}
	if worlds, ok := store.(WorldAllocationStore); ok {
		s.worldAllocations = worlds
	}
	s.arenaLifecycle = store
	return nil
}

// RevokeTransfer只供可信匹配补偿，防重放仓储烧掉已签或结果未知的身份；重复撤销幂等。
func (s *Service) RevokeTransfer(ctx context.Context, ticketID string) error {
	if err := s.transfer.RevokeContext(ctx, ticketID); err != nil {
		return err
	}
	s.registry.FinishWorldTransfer(ticketID)
	return nil
}

// arenaAllocationFingerprint按玩家身份规范化名单，数组顺序不改变意图；字段/分队/角色变化必须冲突。
func arenaAllocationFingerprint(input AllocateMainArenaInput) (string, error) {
	input.Roster = append(input.Roster[:0:0], input.Roster...)
	sort.Slice(input.Roster, func(i, j int) bool { return input.Roster[i].PlayerID < input.Roster[j].PlayerID })
	payload, err := json.Marshal(input)
	if err != nil {
		return "", err
	}
	digest := sha256.Sum256(payload)
	return hex.EncodeToString(digest[:]), nil
}

// restoreArenaRegistration只恢复同一真实Boot；新Boot令旧比赛明确取消，不能满血继承旧名单/票据。
func (s *Service) restoreArenaRegistration(ctx context.Context, input RegisterInput) error {
	if s.arenaLifecycle == nil {
		return nil
	}
	record, found, err := s.arenaLifecycle.GetArenaByServer(ctx, input.GameServerID)
	if err != nil {
		return err
	}
	if !found {
		return nil
	}
	if record.ServerBootID != input.ServerBootID {
		if record.State == "active" {
			record.State = "cancelled"
		}
		record.UpdatedAtUTC = s.now().UTC()
		return s.arenaLifecycle.SaveArena(ctx, record)
	}
	if record.State == "released" {
		return nil
	}
	arena := record.Assignment.Arena
	if arena == nil {
		return errors.New("ARENA_RECORD_INVALID: 缺少竞技分配")
	}
	if _, err := s.registry.AllocateSpecific(input.GameServerID, gameserver.AllocationRequest{RoleID: record.Assignment.ServerRoleID, ExperienceID: record.Assignment.ExperienceID, RegionID: record.Assignment.RegionID, MatchID: arena.MatchID, RequiredCapacity: arena.TotalPlayers}); err != nil {
		return err
	}
	s.mu.Lock()
	s.assignments[input.GameServerID] = assignmentRecord{assignment: cloneServerAssignment(record.Assignment), endpoint: record.Endpoint}
	s.notifyAssignmentLocked(input.GameServerID)
	s.mu.Unlock()
	if record.State == "cancelled" {
		return s.registry.Drain(input.GameServerID)
	}
	return nil
}

// LookupPlayerPresence仅供认证Gateway查询同一会话本人，且实例Boot/最近心跳仍有效；不相信客户端SourceServerID。
func (s *Service) LookupPlayerPresence(ctx context.Context, player, session string) (PlayerPresence, error) {
	if s.arenaLifecycle == nil || player == "" || session == "" {
		return PlayerPresence{}, errors.New("PLAYER_PRESENCE_UNAVAILABLE: 未装配本人位置查询")
	}
	presence, found, err := s.arenaLifecycle.GetPresence(ctx, player)
	if err != nil {
		return PlayerPresence{}, err
	}
	if !found || presence.SessionID != session {
		return PlayerPresence{}, errors.New("PLAYER_NOT_IN_WORLD: 当前会话没有已接受准入")
	}
	server, exists := s.registry.Get(presence.GameServerID)
	if !exists || server.ServerBootID != presence.ServerBootID || server.LastHeartbeat.IsZero() || s.now().UTC().Sub(server.LastHeartbeat) > 45*time.Second {
		return PlayerPresence{}, errors.New("PLAYER_NOT_IN_WORLD: 原实例不可用")
	}
	return presence, nil
}

// GetArenaStatus在控制面重启后给实例60秒注册恢复宽限；故障不是正常胜利，不构造比赛结果。
func (s *Service) GetArenaStatus(ctx context.Context, matchID string) (ArenaStatus, error) {
	if s.arenaLifecycle == nil {
		return ArenaStatus{}, errors.New("ARENA_STORE_UNAVAILABLE: 未装配持久控制事实")
	}
	record, found, err := s.arenaLifecycle.GetArenaByMatch(ctx, matchID)
	if err != nil {
		return ArenaStatus{}, err
	}
	if !found {
		return ArenaStatus{}, errors.New("ARENA_RECORD_NOT_FOUND: 比赛尚无确认分配")
	}
	status := ArenaStatus{Record: record, Presences: []PlayerPresence{}}
	if record.UnallocatedCancellation {
		return status, nil
	}
	server, exists := s.registry.Get(record.Assignment.GameServerID)
	if !exists && s.now().UTC().Before(s.startedAtUTC.Add(60*time.Second)) {
		return ArenaStatus{}, errors.New("ARENA_STATUS_UNKNOWN: 等待实例重新注册")
	}
	status.ServerAlive = exists && server.ServerBootID == record.ServerBootID && !server.LastHeartbeat.IsZero() && s.now().UTC().Sub(server.LastHeartbeat) <= 45*time.Second
	if status.ServerAlive {
		status.CurrentPlayers = server.CurrentPlayers
		status.HeartbeatAtUTC = server.LastHeartbeat
	}
	for _, slot := range record.Assignment.Arena.Roster {
		presence, found, err := s.arenaLifecycle.GetPresence(ctx, slot.PlayerID)
		if err != nil {
			return ArenaStatus{}, err
		}
		if found {
			status.Presences = append(status.Presences, presence)
		}
	}
	return status, nil
}

// CancelArena停止新准入并排空，不把故障取消伪造成赢方；仍有玩家时保留记录供个人安全返回。
func (s *Service) CancelArena(ctx context.Context, matchID string) error {
	s.arenaAllocationMu.Lock()
	defer s.arenaAllocationMu.Unlock()
	if matchID == "" || len(matchID) > 128 {
		return errors.New("MATCH_ID_INVALID: 取消身份无效")
	}
	if s.arenaLifecycle == nil {
		return errors.New("ARENA_STORE_UNAVAILABLE: 未装配持久控制事实")
	}
	record, found, err := s.arenaLifecycle.GetArenaByMatch(ctx, matchID)
	if err != nil {
		return err
	}
	if !found {
		// 未收到分配响应也必须持久阻止迟到分配；晚到SaveArena在同一写域条件失败。
		return s.arenaLifecycle.SaveArena(ctx, ArenaAssignmentRecord{UnallocatedCancellation: true, Assignment: gameservercontract.ServerAssignment{Arena: &gameservercontract.Assignment{MatchID: matchID}}, State: "cancelled", UpdatedAtUTC: s.now().UTC()})
	}
	if record.State == "released" || record.State == "cancelled" {
		return nil
	}
	if record.State == "resultCommitted" {
		return errors.New("ARENA_OPERATION_TERMINAL: 已有可信结果不能改成故障取消")
	}
	record.State = "cancelled"
	record.UpdatedAtUTC = s.now().UTC()
	if err := s.arenaLifecycle.SaveArena(ctx, record); err != nil {
		return err
	}
	return s.registry.Drain(record.Assignment.GameServerID)
}

// FinalizeArena只有在可信结果/取消且玩家已离开或原进程已失效时才回收；先实际释放，再提交released终态。
func (s *Service) FinalizeArena(ctx context.Context, matchID string) error {
	s.arenaAllocationMu.Lock()
	defer s.arenaAllocationMu.Unlock()
	status, err := s.GetArenaStatus(ctx, matchID)
	if err != nil {
		return err
	}
	if status.Record.State == "released" {
		return nil
	}
	if status.Record.State != "resultCommitted" && status.Record.State != "cancelled" {
		return errors.New("ARENA_NOT_TERMINAL: 尚无可信结束事实")
	}
	if status.ServerAlive && (status.CurrentPlayers > 0 || status.HeartbeatAtUTC.Before(status.Record.UpdatedAtUTC)) {
		return errors.New("ARENA_PLAYERS_REMAIN: 等待玩家安全迁出")
	}
	// 原Boot已消失时只终结旧记录，绝不Drain/Release同名后继进程。
	if server, found := s.registry.Get(status.Record.Assignment.GameServerID); found && server.ServerBootID == status.Record.ServerBootID {
		if err := s.registry.Drain(server.ID); err != nil {
			return err
		}
		if err := s.arenaAllocator.Release(ctx, server.ID); err != nil {
			return err
		}
	}
	record := status.Record
	record.State = "released"
	record.UpdatedAtUTC = s.now().UTC()
	if err := s.arenaLifecycle.SaveArena(ctx, record); err != nil {
		return err
	}
	s.mu.Lock()
	if current, found := s.assignments[record.Assignment.GameServerID]; found && current.assignment.Arena != nil && current.assignment.Arena.MatchID == matchID {
		delete(s.assignments, record.Assignment.GameServerID)
	}
	s.mu.Unlock()
	return nil
}

// validateResultRoster只在已装配完整生命周期时启用，逐项核对锁定玩家/队伍/角色，拒绝伪造或遗漏名单。
func validateResultRoster(result match.Result, record ArenaAssignmentRecord) error {
	if record.Assignment.Arena == nil || record.State == "cancelled" || record.State == "released" {
		return errors.New("MATCH_RESULT_ASSIGNMENT_INVALID: 比赛已取消或释放")
	}
	if record.Assignment.Arena.ProjectRuleRevision > 0 {
		allowed := map[string]bool{"Elimination": true, "TimeLimit": true, "PlayerForfeit": true, "TeamForfeit": true, "ReconnectTimeout": true}
		if result.EndRevision != 1 || !allowed[result.EndReason] {
			return errors.New("MATCH_RESULT_REVISION_INVALID: 结束原因或修订不符合单局规则")
		}
	}
	roster := record.Assignment.Arena.Roster
	if len(result.Players) != len(roster) || len(result.Teams) != 2 || result.StartedAt.IsZero() || result.EndedAt.Before(result.StartedAt) {
		return errors.New("MATCH_RESULT_ROSTER_MISMATCH: 名单或权威时间无效")
	}
	seen := map[string]bool{}
	teams := map[string]bool{}
	for _, player := range result.Players {
		if seen[player.PlayerID] {
			return errors.New("MATCH_RESULT_ROSTER_MISMATCH: 重复玩家")
		}
		seen[player.PlayerID] = true
		found := false
		for _, slot := range roster {
			if slot.PlayerID == player.PlayerID && slot.TeamID == player.TeamID && slot.CharacterID == player.CharacterID {
				found = true
				teams[slot.TeamID] = true
				break
			}
		}
		if !found {
			return errors.New("MATCH_RESULT_ROSTER_MISMATCH: 非锁定参赛者")
		}
	}
	resultTeams := map[string]bool{}
	for _, team := range result.Teams {
		if !teams[team.TeamID] || resultTeams[team.TeamID] {
			return errors.New("MATCH_RESULT_TEAM_MISMATCH: 队伍不存在或重复")
		}
		if team.Won != (result.WinningTeamID != "" && team.TeamID == result.WinningTeamID) {
			return errors.New("MATCH_RESULT_TEAM_MISMATCH: 赢方与逐队胜负不一致")
		}
		resultTeams[team.TeamID] = true
	}
	if result.WinningTeamID != "" && !teams[result.WinningTeamID] {
		return errors.New("MATCH_RESULT_TEAM_MISMATCH: 获胜队不在比赛中")
	}
	return nil
}
