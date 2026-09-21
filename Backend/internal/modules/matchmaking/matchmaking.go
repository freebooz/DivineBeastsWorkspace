// Package matchmaking（匹配领域）实现1v1到5v5竞技模式定义与匹配票据校验。
package matchmaking

import (
	"errors"
	"time"
)

// ArenaMode（竞技模式）定义每队人数、总人数及组队策略。
type ArenaMode struct {
	ID                string // ID（竞技模式ID）。
	TeamSize          int    // TeamSize（每队人数）。
	TotalPlayers      int    // TotalPlayers（总玩家数）。
	AllowSoloQueue    bool   // AllowSoloQueue（是否允许单排）。
	AllowPartialParty bool   // AllowPartialParty（是否允许非满编Party）。
	AllowFill         bool   // AllowFill（是否允许系统补人）。
}

var arenaModes = map[string]ArenaMode{
	"Arena.Mode.Duel1v1": {ID: "Arena.Mode.Duel1v1", TeamSize: 1, TotalPlayers: 2, AllowSoloQueue: true, AllowPartialParty: false, AllowFill: false},
	"Arena.Mode.Team2v2": {ID: "Arena.Mode.Team2v2", TeamSize: 2, TotalPlayers: 4, AllowSoloQueue: true, AllowPartialParty: true, AllowFill: true},
	"Arena.Mode.Team3v3": {ID: "Arena.Mode.Team3v3", TeamSize: 3, TotalPlayers: 6, AllowSoloQueue: true, AllowPartialParty: true, AllowFill: true},
	"Arena.Mode.Team4v4": {ID: "Arena.Mode.Team4v4", TeamSize: 4, TotalPlayers: 8, AllowSoloQueue: true, AllowPartialParty: true, AllowFill: true},
	"Arena.Mode.Team5v5": {ID: "Arena.Mode.Team5v5", TeamSize: 5, TotalPlayers: 10, AllowSoloQueue: true, AllowPartialParty: true, AllowFill: true},
}

// ArenaModeByID（按ID获取竞技模式）返回统一1v1~5v5配置。
func ArenaModeByID(id string) (ArenaMode, bool) { mode, ok := arenaModes[id]; return mode, ok }

// CreateTicketRequest（创建匹配票据请求）只接受客户端可声明的数据，不包含MMR、处罚等Backend权威字段。
type CreateTicketRequest struct {
	TicketID       string   // TicketID（匹配票据ID）。
	GameID         string   // GameID（游戏ID）。
	ArenaModeID    string   // ArenaModeID（竞技模式ID）。
	PartyID        string   // PartyID（Party ID，单排可为空）。
	PartyMemberIDs []string // PartyMemberIDs（不可拆分Party成员列表）。
	Region         string   // Region（目标区域）。
}

// Ticket（匹配票据）是进入Matchmaking Queue（匹配队列）的原子单元。
type Ticket struct {
	TicketID       string    // TicketID（匹配票据ID）。
	GameID         string    // GameID（游戏ID）。
	ArenaModeID    string    // ArenaModeID（竞技模式ID）。
	PartyID        string    // PartyID（组队ID，单排可为空）。
	PartyMemberIDs []string  // PartyMemberIDs（不可拆分的组队成员ID列表）。
	PartySize      int       // PartySize（当前组队人数）。
	TeamSize       int       // TeamSize（竞技模式每队人数）。
	AllowFill      bool      // AllowFill（是否允许系统补充队友）。
	Region         string    // Region（匹配区域）。
	State          string    // State（匹配票据状态）。
	CreatedAt      time.Time // CreatedAt（匹配票据创建时间）。
}

// NewTicket（创建匹配票据）校验Party大小、成员唯一性和模式容量。
func NewTicket(req CreateTicketRequest) (Ticket, error) {
	mode, ok := ArenaModeByID(req.ArenaModeID)
	if !ok {
		return Ticket{}, errors.New("MATCH_MODE_INVALID: ArenaModeID无效")
	}
	if len(req.PartyMemberIDs) == 0 {
		return Ticket{}, errors.New("匹配票据至少包含一个玩家")
	}
	if len(req.PartyMemberIDs) > mode.TeamSize {
		return Ticket{}, errors.New("MATCH_PARTY_TOO_LARGE: Party人数超过单队容量")
	}
	if req.PartyID == "" && len(req.PartyMemberIDs) > 1 {
		return Ticket{}, errors.New("多人匹配必须提供PartyID")
	}
	seen := make(map[string]struct{}, len(req.PartyMemberIDs))
	members := make([]string, 0, len(req.PartyMemberIDs))
	for _, playerID := range req.PartyMemberIDs {
		if playerID == "" {
			return Ticket{}, errors.New("PlayerID不能为空")
		}
		if _, exists := seen[playerID]; exists {
			return Ticket{}, errors.New("PartyMemberIDs不能重复")
		}
		seen[playerID] = struct{}{}
		members = append(members, playerID)
	}
	return Ticket{
		TicketID: req.TicketID, GameID: req.GameID, ArenaModeID: req.ArenaModeID, PartyID: req.PartyID,
		PartyMemberIDs: members, PartySize: len(members), TeamSize: mode.TeamSize, AllowFill: mode.AllowFill,
		Region: req.Region, State: "searching", CreatedAt: time.Now().UTC(),
	}, nil
}
