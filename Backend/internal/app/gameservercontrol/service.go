// Package gameservercontrol（游戏服务器控制应用服务）编排GameServer注册、世界/竞技分配、迁移和比赛结果生命周期。
package gameservercontrol

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// RegisterInput（游戏服务器注册输入）是Dedicated Server注册到Backend控制面的应用层请求。
type RegisterInput struct {
	GameID          string // GameID（游戏ID）。
	GameServerID    string // GameServerID（游戏服务器实例ID）。
	ServerRoleID    string // ServerRoleID（OpenWorld/Village/MainArena服务器角色）。
	ExperienceID    string // ExperienceID（当前进程承载体验）。
	RegionID        string // RegionID（部署区域ID）。
	ClusterID       string // ClusterID（Kubernetes/Agones集群ID）。
	NodeID          string // NodeID（承载节点ID）。
	WorldID         string // WorldID（世界或地图逻辑ID）。
	PublicEndpoint  string // PublicEndpoint（Game Client连接地址，例如127.0.0.1:7777）。
	BuildVersion    string // BuildVersion（GameServer构建版本）。
	ProtocolVersion uint32 // ProtocolVersion（UE实时网络协议版本）。
	Capacity        int    // Capacity（最大玩家容量）。
}

// AllocateWorldInput（常驻世界分配输入）用于OpenWorld.Hub、OpenWorld.Main和Village三类体验。
type AllocateWorldInput struct {
	ExperienceID string // ExperienceID（目标体验）。
	WorldID      string // WorldID（目标世界/区域/实例逻辑ID）。
	RegionID     string // RegionID（目标部署区域）。
	PlayerSlots  int    // PlayerSlots（本次迁移需要预留的玩家容量，通常为1）。
}

// AllocateMainArenaInput（主竞技场分配输入）描述MatchService请求MainArena实例的完整上下文。
type AllocateMainArenaInput struct {
	MatchID     string                            // MatchID（比赛唯一ID）。
	ArenaModeID string                            // ArenaModeID（1v1至5v5竞技模式ID）。
	MapID       string                            // MapID（竞技场地图ID）。
	RegionID    string                            // RegionID（目标部署区域）。
	Roster      []gameservercontract.RosterPlayer // Roster（权威玩家名单与Team关系）。
}

// IssueTransferInput（迁移票据签发输入）描述单个玩家进入目标GameServer所需上下文。
type IssueTransferInput struct {
	TicketID                string        // TicketID（迁移票据唯一ID）。
	AssignmentID            string        // AssignmentID（目标世界或比赛分配ID）。
	GameID                  string        // GameID（游戏ID）。
	PlayerID                string        // PlayerID（玩家ID）。
	SessionID               string        // SessionID（在线会话ID）。
	SourceGameServerID      string        // SourceGameServerID（来源服务器ID，首次进服可为空）。
	DestinationGameServerID string        // DestinationGameServerID（目标服务器实例ID）。
	DestinationWorldID      string        // DestinationWorldID（目标世界ID）。
	DestinationExperienceID string        // DestinationExperienceID（目标体验）。
	MatchID                 string        // MatchID（目标比赛ID；世界迁移为空）。
	TTL                     time.Duration // TTL（迁移票据有效期）。
}

// AllocateWorldTransferInput（世界分配并迁移输入）把常驻世界分配与TransferTicket签发组合为一个应用用例。
type AllocateWorldTransferInput struct {
	World    AllocateWorldInput // World（目标世界分配参数）。
	Transfer IssueTransferInput // Transfer（玩家迁移上下文；目标Assignment字段由服务填充）。
}

// WorldTransferResult（世界跨服结果）一次返回目标Assignment和可交给客户端的迁移票据。
type WorldTransferResult struct {
	Assignment gameservercontract.WorldAssignment // Assignment（目标世界权威分配）。
	Ticket     servertransfer.Ticket              // Ticket（短期一次性跨服票据）。
}

// assignmentRecord（服务器任务内部记录）保存统一Assignment和目标连接端点。
type assignmentRecord struct {
	assignment gameservercontract.ServerAssignment
	endpoint   string
}

// Service（游戏服务器控制应用服务）组合领域服务并维护短期Assignment运行状态。
type Service struct {
	registry       *gameserver.Registry
	worldAllocator gameserver.Allocator
	arenaAllocator gameserver.Allocator
	transfer       *servertransfer.Service
	resultService  *match.ResultService
	now            func() time.Time
	mu             sync.RWMutex
	assignments    map[string]assignmentRecord
}

// NewService（创建游戏服务器控制应用服务）使用Registry同时承担世界和竞技分配，适合测试和本地开发。
func NewService(registry *gameserver.Registry, transfer *servertransfer.Service, resultService *match.ResultService, now func() time.Time) *Service {
	allocator := gameserver.NewRegistryAllocator(registry)
	return NewServiceWithAllocators(registry, allocator, allocator, transfer, resultService, now)
}

// NewServiceWithAllocators（创建可生产装配的游戏服务器控制服务）允许MainArena使用Agones、世界服务器使用常驻Registry池。
func NewServiceWithAllocators(registry *gameserver.Registry, worldAllocator, arenaAllocator gameserver.Allocator, transfer *servertransfer.Service, resultService *match.ResultService, now func() time.Time) *Service {
	if registry == nil || worldAllocator == nil || arenaAllocator == nil || transfer == nil || resultService == nil || now == nil {
		panic("GameServerControlService依赖不能为空")
	}
	return &Service{registry: registry, worldAllocator: worldAllocator, arenaAllocator: arenaAllocator, transfer: transfer, resultService: resultService, now: now, assignments: make(map[string]assignmentRecord)}
}

// Register（注册GameServer）把Dedicated Server实例写入快速注册表。
func (s *Service) Register(input RegisterInput) error {
	if input.GameServerID == "" || input.ServerRoleID == "" || input.RegionID == "" || input.PublicEndpoint == "" || input.Capacity <= 0 {
		return errors.New("GameServer注册关键字段不能为空且Capacity必须大于0")
	}
	if !gameserver.IsKnownRole(input.ServerRoleID) {
		return errors.New("GAME_SERVER_ROLE_INVALID: ServerRoleID不是正式服务器角色")
	}
	if input.ExperienceID == "" {
		switch input.ServerRoleID {
		case gameserver.RoleOpenWorld:
			input.ExperienceID = gameservercontract.ExperienceOpenWorldMain
		case gameserver.RoleVillage:
			input.ExperienceID = gameservercontract.ExperienceVillageMain
		case gameserver.RoleMainArena:
			input.ExperienceID = gameservercontract.ExperienceMainArenaMain
		}
	}
	roleForExperience, ok := gameservercontract.RoleForExperience(input.ExperienceID)
	if !ok || roleForExperience != input.ServerRoleID {
		return errors.New("GAME_SERVER_EXPERIENCE_INVALID: ExperienceID与ServerRoleID不匹配")
	}
	s.registry.Register(gameserver.Instance{ID: input.GameServerID, RoleID: input.ServerRoleID, ExperienceID: input.ExperienceID, RegionID: input.RegionID, ClusterID: input.ClusterID, NodeID: input.NodeID, WorldID: input.WorldID, PublicEndpoint: input.PublicEndpoint, BuildVersion: input.BuildVersion, ProtocolVersion: input.ProtocolVersion, Capacity: input.Capacity, Status: gameserver.StatusStarting, LastHeartbeat: s.now().UTC()})
	return nil
}

// Heartbeat（更新GameServer心跳）刷新玩家数、状态和最后活动时间。
func (s *Service) Heartbeat(gameServerID string, currentPlayers int, status gameserver.Status) error {
	return s.registry.Heartbeat(gameServerID, currentPlayers, status, s.now().UTC())
}

// SetReady（标记GameServer就绪）允许实例进入后续分配流程。
func (s *Service) SetReady(gameServerID string) error { return s.registry.SetReady(gameServerID) }

// Drain（排空GameServer）禁止实例接受新的玩家或比赛分配。
func (s *Service) Drain(gameServerID string) error { return s.registry.Drain(gameServerID) }

// AllocateWorld（分配常驻世界）支持OpenWorld.Hub、OpenWorld.Main、Village.Main/Tutorial/Training。
// Hub与Main都使用OpenWorld ServerRole，不创建独立Lobby Server。
func (s *Service) AllocateWorld(ctx context.Context, input AllocateWorldInput) (gameservercontract.WorldAssignment, error) {
	roleID, ok := gameservercontract.RoleForExperience(input.ExperienceID)
	if !ok || roleID == gameservercontract.RoleMainArena {
		return gameservercontract.WorldAssignment{}, errors.New("WORLD_EXPERIENCE_INVALID: 目标体验不是OpenWorld/Village世界体验")
	}
	if input.WorldID == "" || input.RegionID == "" {
		return gameservercontract.WorldAssignment{}, errors.New("WorldID和RegionID不能为空")
	}
	if input.PlayerSlots <= 0 {
		input.PlayerSlots = 1
	}
	allocated, err := s.worldAllocator.Allocate(ctx, gameserver.AllocationRequest{RoleID: roleID, ExperienceID: input.ExperienceID, RegionID: input.RegionID, WorldID: input.WorldID, RequiredCapacity: input.PlayerSlots})
	if err != nil {
		return gameservercontract.WorldAssignment{}, err
	}

	s.mu.Lock()
	defer s.mu.Unlock()
	if current, exists := s.assignments[allocated.ID]; exists {
		if current.assignment.ServerRoleID != roleID || current.assignment.ExperienceID != input.ExperienceID || current.assignment.WorldID != input.WorldID {
			_ = s.registry.ReleaseReservation(allocated.ID, input.PlayerSlots)
			return gameservercontract.WorldAssignment{}, errors.New("WORLD_ASSIGNMENT_CONFLICT: GameServer已绑定其他世界体验")
		}
		world := *current.assignment.World
		world.Endpoint = current.endpoint
		return world, nil
	}
	assignmentID := "world:" + allocated.ID + ":" + input.ExperienceID
	world := gameservercontract.WorldAssignment{AssignmentID: assignmentID, GameServerID: allocated.ID, ServerRoleID: roleID, ExperienceID: input.ExperienceID, WorldID: input.WorldID, RegionID: input.RegionID, Endpoint: allocated.PublicEndpoint}
	if err := world.Validate(); err != nil {
		_ = s.registry.ReleaseReservation(allocated.ID, input.PlayerSlots)
		return gameservercontract.WorldAssignment{}, err
	}
	serverAssignment := gameservercontract.ServerAssignment{AssignmentID: assignmentID, ServerRoleID: roleID, ExperienceID: input.ExperienceID, WorldID: input.WorldID, RegionID: input.RegionID, GameServerID: allocated.ID, World: &world}
	if err := serverAssignment.Validate(); err != nil {
		_ = s.registry.ReleaseReservation(allocated.ID, input.PlayerSlots)
		return gameservercontract.WorldAssignment{}, err
	}
	s.assignments[allocated.ID] = assignmentRecord{assignment: serverAssignment, endpoint: allocated.PublicEndpoint}
	return world, nil
}

// AllocateWorldTransfer（分配世界并签发跨服票据）完成OpenWorld/Village迁移的应用层闭环。
// 如果签票失败，会释放本次容量预留，避免常驻世界容量泄漏。
func (s *Service) AllocateWorldTransfer(ctx context.Context, input AllocateWorldTransferInput) (WorldTransferResult, error) {
	assignment, err := s.AllocateWorld(ctx, input.World)
	if err != nil {
		return WorldTransferResult{}, err
	}
	input.Transfer.AssignmentID = assignment.AssignmentID
	input.Transfer.DestinationGameServerID = assignment.GameServerID
	input.Transfer.DestinationWorldID = assignment.WorldID
	input.Transfer.DestinationExperienceID = assignment.ExperienceID
	input.Transfer.MatchID = ""
	if input.Transfer.TTL <= 0 {
		input.Transfer.TTL = 30 * time.Second
	}
	ticket, err := s.IssueTransfer(input.Transfer)
	if err != nil {
		_ = s.registry.ReleaseReservation(assignment.GameServerID, input.World.PlayerSlots)
		return WorldTransferResult{}, err
	}
	return WorldTransferResult{Assignment: assignment, Ticket: ticket}, nil
}

// AllocateMainArena（分配主竞技场）按竞技模式自动计算容量，并保证一场比赛独占一个MainArena实例。
func (s *Service) AllocateMainArena(input AllocateMainArenaInput) (gameservercontract.Assignment, error) {
	return s.AllocateMainArenaContext(context.Background(), input)
}

// AllocateMainArenaContext（分配主竞技场）允许生产Agones分配器使用请求Context。
func (s *Service) AllocateMainArenaContext(ctx context.Context, input AllocateMainArenaInput) (gameservercontract.Assignment, error) {
	teamSize, ok := gameservercontract.TeamSizeForArenaMode(input.ArenaModeID)
	if !ok {
		return gameservercontract.Assignment{}, errors.New("MATCH_MODE_INVALID: ArenaModeID无效")
	}
	if input.MatchID == "" || input.MapID == "" || input.RegionID == "" {
		return gameservercontract.Assignment{}, errors.New("MatchID、MapID和RegionID不能为空")
	}
	totalPlayers := teamSize * 2
	if len(input.Roster) != totalPlayers {
		return gameservercontract.Assignment{}, fmt.Errorf("Roster人数=%d，竞技模式要求=%d", len(input.Roster), totalPlayers)
	}
	allocated, err := s.arenaAllocator.Allocate(ctx, gameserver.AllocationRequest{RoleID: gameserver.RoleMainArena, ExperienceID: gameservercontract.ExperienceMainArenaMain, RegionID: input.RegionID, RequiredCapacity: totalPlayers, MatchID: input.MatchID})
	if err != nil {
		return gameservercontract.Assignment{}, err
	}
	assignment := gameservercontract.Assignment{MatchID: input.MatchID, ArenaModeID: input.ArenaModeID, MapID: input.MapID, TeamSize: teamSize, TotalPlayers: totalPlayers, GameServerID: allocated.ID, Roster: append([]gameservercontract.RosterPlayer(nil), input.Roster...)}
	if err := assignment.Validate(); err != nil {
		_ = s.arenaAllocator.Release(ctx, allocated.ID)
		return gameservercontract.Assignment{}, err
	}
	assignmentID := "match:" + input.MatchID
	serverAssignment := gameservercontract.ServerAssignment{AssignmentID: assignmentID, ServerRoleID: gameservercontract.RoleMainArena, ExperienceID: gameservercontract.ExperienceMainArenaMain, WorldID: "World.MainArena", RegionID: input.RegionID, GameServerID: allocated.ID, Arena: &assignment}
	if err := serverAssignment.Validate(); err != nil {
		_ = s.arenaAllocator.Release(ctx, allocated.ID)
		return gameservercontract.Assignment{}, err
	}
	s.mu.Lock()
	s.assignments[allocated.ID] = assignmentRecord{assignment: serverAssignment, endpoint: allocated.PublicEndpoint}
	s.mu.Unlock()
	return assignment, nil
}

// GetServerAssignment（获取统一服务器任务）供三类Dedicated Server查询当前权威运行上下文。
func (s *Service) GetServerAssignment(gameServerID string) (gameservercontract.ServerAssignment, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	record, found := s.assignments[gameServerID]
	if !found {
		return gameservercontract.ServerAssignment{}, false
	}
	result := cloneServerAssignment(record.assignment)
	return result, true
}

// GetAssignment（获取竞技比赛分配）保留MainArena旧调用兼容入口。
func (s *Service) GetAssignment(gameServerID string) (gameservercontract.Assignment, bool) {
	assignment, found := s.GetServerAssignment(gameServerID)
	if !found || assignment.Arena == nil {
		return gameservercontract.Assignment{}, false
	}
	result := *assignment.Arena
	result.Roster = append([]gameservercontract.RosterPlayer(nil), result.Roster...)
	return result, true
}

// IssueTransfer（签发迁移票据）同时支持世界迁移和MainArena迁移，并绑定Assignment/Experience/World。
func (s *Service) IssueTransfer(input IssueTransferInput) (servertransfer.Ticket, error) {
	s.mu.RLock()
	record, found := s.assignments[input.DestinationGameServerID]
	s.mu.RUnlock()
	if !found {
		return servertransfer.Ticket{}, errors.New("GAME_SERVER_ASSIGNMENT_NOT_FOUND: 目标GameServer没有任务分配")
	}
	assignment := record.assignment
	if input.AssignmentID == "" {
		input.AssignmentID = assignment.AssignmentID
	}
	if input.AssignmentID != assignment.AssignmentID {
		return servertransfer.Ticket{}, errors.New("TRANSFER_ASSIGNMENT_MISMATCH: AssignmentID与目标服务器任务不一致")
	}
	if input.DestinationExperienceID == "" {
		input.DestinationExperienceID = assignment.ExperienceID
	}
	if input.DestinationWorldID == "" {
		input.DestinationWorldID = assignment.WorldID
	}
	if input.DestinationExperienceID != assignment.ExperienceID || input.DestinationWorldID != assignment.WorldID {
		return servertransfer.Ticket{}, errors.New("TRANSFER_WORLD_MISMATCH: 目标Experience或World与Assignment不一致")
	}
	if assignment.Arena != nil {
		if input.MatchID != assignment.Arena.MatchID {
			return servertransfer.Ticket{}, errors.New("TRANSFER_MATCH_MISMATCH: MatchID与目标服务器Assignment不一致")
		}
		admitted := false
		for _, player := range assignment.Arena.Roster {
			if player.PlayerID == input.PlayerID {
				admitted = true
				break
			}
		}
		if !admitted {
			return servertransfer.Ticket{}, errors.New("TRANSFER_PLAYER_NOT_IN_ROSTER: 玩家不在目标比赛名单中")
		}
	} else if input.MatchID != "" {
		return servertransfer.Ticket{}, errors.New("TRANSFER_MATCH_MISMATCH: 世界迁移不应携带MatchID")
	}

	return s.transfer.Issue(servertransfer.IssueRequest{TicketID: input.TicketID, AssignmentID: input.AssignmentID, GameID: input.GameID, PlayerID: input.PlayerID, SessionID: input.SessionID, SourceGameServerID: input.SourceGameServerID, DestinationGameServerID: input.DestinationGameServerID, DestinationEndpoint: record.endpoint, DestinationWorldID: input.DestinationWorldID, DestinationExperienceID: input.DestinationExperienceID, MatchID: input.MatchID, TTL: input.TTL})
}

// ValidateTransfer（验证迁移票据）保留旧无Context调用入口。
func (s *Service) ValidateTransfer(ticket servertransfer.Ticket, destinationGameServerID string) (servertransfer.ValidationResult, error) {
	return s.ValidateTransferContext(context.Background(), ticket, destinationGameServerID)
}

// ValidateTransferContext（验证迁移票据）成功消费后提交世界服务器容量预留。
func (s *Service) ValidateTransferContext(ctx context.Context, ticket servertransfer.Ticket, destinationGameServerID string) (servertransfer.ValidationResult, error) {
	result, err := s.transfer.ValidateContext(ctx, servertransfer.ValidateRequest{Ticket: ticket, DestinationGameServerID: destinationGameServerID})
	if err != nil {
		return servertransfer.ValidationResult{}, err
	}
	s.mu.RLock()
	record, found := s.assignments[destinationGameServerID]
	s.mu.RUnlock()
	if !found || record.assignment.AssignmentID != result.AssignmentID {
		return servertransfer.ValidationResult{}, errors.New("TRANSFER_ASSIGNMENT_NOT_FOUND: 票据对应Assignment不存在")
	}
	if record.assignment.World != nil {
		_ = s.registry.CommitReservation(destinationGameServerID, 1)
	}
	return result, nil
}

// SubmitMatchResult（提交权威比赛结果）验证GameServer绑定后执行幂等结算，并在成功后释放MainArena实例。
func (s *Service) SubmitMatchResult(ctx context.Context, result match.Result) (match.StoredResult, error) {
	s.mu.RLock()
	record, found := s.assignments[result.GameServerID]
	s.mu.RUnlock()
	if !found || record.assignment.Arena == nil {
		return match.StoredResult{}, errors.New("GAME_SERVER_ASSIGNMENT_NOT_FOUND: GameServer没有活动比赛")
	}
	arena := record.assignment.Arena
	if arena.MatchID != result.MatchID {
		return match.StoredResult{}, errors.New("MATCH_RESULT_SERVER_MISMATCH: 比赛结果与GameServer绑定不一致")
	}
	if arena.ArenaModeID != result.ArenaModeID {
		return match.StoredResult{}, errors.New("MATCH_RESULT_MODE_MISMATCH: 比赛结果竞技模式与Assignment不一致")
	}
	stored, err := s.resultService.Submit(ctx, result)
	if err != nil {
		return match.StoredResult{}, err
	}
	if err := s.arenaAllocator.Release(ctx, result.GameServerID); err != nil {
		return match.StoredResult{}, err
	}
	s.mu.Lock()
	delete(s.assignments, result.GameServerID)
	s.mu.Unlock()
	return stored, nil
}

func cloneServerAssignment(value gameservercontract.ServerAssignment) gameservercontract.ServerAssignment {
	if value.World != nil {
		world := *value.World
		value.World = &world
	}
	if value.Arena != nil {
		arena := *value.Arena
		arena.Roster = append([]gameservercontract.RosterPlayer(nil), arena.Roster...)
		value.Arena = &arena
	}
	return value
}
