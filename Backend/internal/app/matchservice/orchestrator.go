// Package matchservice（比赛组织应用服务）负责把匹配成功的原子Party/Ticket编排成正式竞技比赛。
package matchservice

import (
	"errors"
	"fmt"
	"time"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// PlayerSession（玩家在线会话）保存为玩家签发ServerTransfer票据所需的可信会话上下文。
type PlayerSession struct {
	PlayerID           string // PlayerID（玩家ID）。
	SessionID          string // SessionID（玩家在线会话ID）。
	SourceGameServerID string // SourceGameServerID（玩家当前所在GameServer实例ID）。
}

// ArenaAllocationRequest（竞技场分配请求）是MatchService调用GameServerControlService的端口模型。
type ArenaAllocationRequest struct {
	MatchID      string                            // MatchID（比赛唯一ID）。
	ArenaModeID  string                            // ArenaModeID（1v1至5v5竞技模式ID）。
	MapID        string                            // MapID（竞技场地图ID）。
	RegionID     string                            // RegionID（目标区域ID）。
	TeamSize     int                               // TeamSize（每队人数）。
	TotalPlayers int                               // TotalPlayers（总玩家人数）。
	Roster       []gameservercontract.RosterPlayer // Roster（分队完成后的权威玩家名单）。
}

// PlayerTransferRequest（玩家迁移请求）是MatchService为单个玩家申请TransferTicket的端口模型。
type PlayerTransferRequest struct {
	TicketID                string        // TicketID（迁移票据唯一ID）。
	AssignmentID            string        // AssignmentID（目标比赛Assignment ID）。
	GameID                  string        // GameID（游戏ID）。
	PlayerID                string        // PlayerID（玩家ID）。
	SessionID               string        // SessionID（在线会话ID）。
	SourceGameServerID      string        // SourceGameServerID（来源GameServer实例ID）。
	DestinationGameServerID string        // DestinationGameServerID（目标MainArena实例ID）。
	DestinationWorldID      string        // DestinationWorldID（目标世界ID）。
	DestinationExperienceID string        // DestinationExperienceID（目标Experience）。
	MatchID                 string        // MatchID（目标比赛ID）。
	TTL                     time.Duration // TTL（迁移票据有效期）。
}

// ArenaControl（竞技场控制端口）隔离MatchService与GameServerControlService的具体传输方式。
// 生产环境可以由gRPC Client（gRPC客户端）实现，本地集成测试也可以由进程内适配器实现。
type ArenaControl interface {
	AllocateMainArena(req ArenaAllocationRequest) (gameservercontract.Assignment, error)
	IssuePlayerTransfer(req PlayerTransferRequest) (servertransfer.Ticket, error)
}

// CreateArenaMatchInput（创建竞技比赛输入）描述已经由Matcher选中的完整Ticket集合。
type CreateArenaMatchInput struct {
	GameID      string                   // GameID（游戏ID）。
	ArenaModeID string                   // ArenaModeID（竞技模式ID）。
	MapID       string                   // MapID（竞技场地图ID）。
	RegionID    string                   // RegionID（目标区域ID）。
	Tickets     []matchmaking.Ticket     // Tickets（已经匹配成功的原子Party/Ticket集合）。
	Sessions    map[string]PlayerSession // Sessions（按PlayerID索引的在线会话）。
}

// CreateArenaMatchResult（创建竞技比赛结果）返回服务器Assignment和每名玩家的迁移票据。
type CreateArenaMatchResult struct {
	MatchID         string                           // MatchID（比赛唯一ID）。
	Assignment      gameservercontract.Assignment    // Assignment（MainArena权威比赛分配）。
	TransferTickets map[string]servertransfer.Ticket // TransferTickets（按PlayerID索引的迁移票据）。
}

// Orchestrator（比赛编排器）负责原子Party分队、MainArena分配和迁移票据签发。
type Orchestrator struct {
	arenaControl ArenaControl
	nextID       func(prefix string) string
}

// NewOrchestrator（创建比赛编排器）注入竞技场控制端口和ID生成器，便于测试稳定复现。
func NewOrchestrator(arenaControl ArenaControl, nextID func(prefix string) string) *Orchestrator {
	if arenaControl == nil || nextID == nil {
		panic("MatchService Orchestrator依赖不能为空")
	}
	return &Orchestrator{arenaControl: arenaControl, nextID: nextID}
}

// CreateArenaMatch（创建竞技比赛）把原子Party分配到两个队伍，随后请求MainArena并签发全部玩家迁移票据。
func (o *Orchestrator) CreateArenaMatch(input CreateArenaMatchInput) (CreateArenaMatchResult, error) {
	teamSize, ok := gameservercontract.TeamSizeForArenaMode(input.ArenaModeID)
	if !ok {
		return CreateArenaMatchResult{}, errors.New("MATCH_MODE_INVALID: ArenaModeID无效")
	}
	if input.GameID == "" || input.MapID == "" || input.RegionID == "" {
		return CreateArenaMatchResult{}, errors.New("GameID、MapID和RegionID不能为空")
	}
	totalPlayers := teamSize * 2
	if len(input.Tickets) == 0 {
		return CreateArenaMatchResult{}, errors.New("MATCH_TICKETS_EMPTY: 缺少匹配票据")
	}

	playerCount := 0
	for _, ticket := range input.Tickets {
		if ticket.ArenaModeID != input.ArenaModeID {
			return CreateArenaMatchResult{}, errors.New("MATCH_MODE_MISMATCH: Ticket竞技模式不一致")
		}
		if ticket.Region != input.RegionID {
			return CreateArenaMatchResult{}, errors.New("MATCH_REGION_MISMATCH: Ticket区域不一致")
		}
		if ticket.PartySize != len(ticket.PartyMemberIDs) || ticket.PartySize <= 0 || ticket.PartySize > teamSize {
			return CreateArenaMatchResult{}, errors.New("MATCH_PARTY_INVALID: Ticket Party人数无效")
		}
		playerCount += len(ticket.PartyMemberIDs)
	}
	if playerCount != totalPlayers {
		return CreateArenaMatchResult{}, fmt.Errorf("MATCH_PLAYER_COUNT_INVALID: 当前玩家数=%d，模式要求=%d", playerCount, totalPlayers)
	}

	teamATickets, teamBTickets, ok := partitionTickets(input.Tickets, teamSize)
	if !ok {
		return CreateArenaMatchResult{}, errors.New("MATCH_TEAM_PARTITION_FAILED: 无法在不拆Party的情况下组成两支队伍")
	}

	roster := make([]gameservercontract.RosterPlayer, 0, totalPlayers)
	seenPlayers := make(map[string]struct{}, totalPlayers)
	appendTeam := func(teamID string, tickets []matchmaking.Ticket) error {
		for _, ticket := range tickets {
			for _, playerID := range ticket.PartyMemberIDs {
				if _, exists := seenPlayers[playerID]; exists {
					return errors.New("MATCH_PLAYER_DUPLICATED: 同一玩家出现在多个Ticket中")
				}
				if _, exists := input.Sessions[playerID]; !exists {
					return fmt.Errorf("MATCH_SESSION_NOT_FOUND: 玩家%s缺少在线Session", playerID)
				}
				seenPlayers[playerID] = struct{}{}
				roster = append(roster, gameservercontract.RosterPlayer{PlayerID: playerID, TeamID: teamID})
			}
		}
		return nil
	}
	if err := appendTeam("team-a", teamATickets); err != nil {
		return CreateArenaMatchResult{}, err
	}
	if err := appendTeam("team-b", teamBTickets); err != nil {
		return CreateArenaMatchResult{}, err
	}

	matchID := o.nextID("match")
	assignment, err := o.arenaControl.AllocateMainArena(ArenaAllocationRequest{
		MatchID: matchID, ArenaModeID: input.ArenaModeID, MapID: input.MapID, RegionID: input.RegionID,
		TeamSize: teamSize, TotalPlayers: totalPlayers, Roster: roster,
	})
	if err != nil {
		return CreateArenaMatchResult{}, err
	}
	if err := assignment.Validate(); err != nil {
		return CreateArenaMatchResult{}, fmt.Errorf("GameServerControl返回无效Assignment: %w", err)
	}

	transferTickets := make(map[string]servertransfer.Ticket, totalPlayers)
	for _, player := range assignment.Roster {
		session := input.Sessions[player.PlayerID]
		ticket, err := o.arenaControl.IssuePlayerTransfer(PlayerTransferRequest{
			TicketID:                o.nextID("transfer"),
			AssignmentID:            "match:" + matchID,
			GameID:                  input.GameID,
			PlayerID:                player.PlayerID,
			SessionID:               session.SessionID,
			SourceGameServerID:      session.SourceGameServerID,
			DestinationGameServerID: assignment.GameServerID,
			DestinationWorldID:      "World.MainArena",
			DestinationExperienceID: gameservercontract.ExperienceMainArenaMain,
			MatchID:                 matchID,
			TTL:                     30 * time.Second,
		})
		if err != nil {
			return CreateArenaMatchResult{}, fmt.Errorf("为玩家%s签发迁移票据失败: %w", player.PlayerID, err)
		}
		transferTickets[player.PlayerID] = ticket
	}

	return CreateArenaMatchResult{MatchID: matchID, Assignment: assignment, TransferTickets: transferTickets}, nil
}

// partitionTickets（原子Ticket分队）使用回溯在最多10名玩家的极小搜索空间内寻找精确TeamSize组合。
// 每个Ticket代表一个不可拆分Party，因此只会整体进入team-a或team-b。
func partitionTickets(tickets []matchmaking.Ticket, teamSize int) ([]matchmaking.Ticket, []matchmaking.Ticket, bool) {
	selected := make([]bool, len(tickets))
	var search func(index, current int) bool
	search = func(index, current int) bool {
		if current == teamSize {
			return true
		}
		if current > teamSize || index >= len(tickets) {
			return false
		}

		selected[index] = true
		if search(index+1, current+len(tickets[index].PartyMemberIDs)) {
			return true
		}
		selected[index] = false
		return search(index+1, current)
	}
	if !search(0, 0) {
		return nil, nil, false
	}

	teamA := make([]matchmaking.Ticket, 0)
	teamB := make([]matchmaking.Ticket, 0)
	for i, ticket := range tickets {
		if selected[i] {
			teamA = append(teamA, ticket)
		} else {
			teamB = append(teamB, ticket)
		}
	}
	return teamA, teamB, true
}
