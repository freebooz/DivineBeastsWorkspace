package matchmaking

import (
	"fmt"
	"testing"

	catalog "divinebeasts/backend/generated/divinebeasts"
	"divinebeasts/backend/internal/contracts/gameserver"
)

// 单处模式容量变更必须同时影响匹配、分服查询及Assignment校验，避免静态数值恰好相等掩盖双表。
func TestArenaCapacityConsumersFollowSharedDefinition(t *testing.T) {
	id := catalog.ArenaModeTeam2v2
	previous := catalog.ArenaModeContexts[id]
	defer func() { catalog.ArenaModeContexts[id] = previous }()
	changed := previous
	changed.TeamSize = 3
	catalog.ArenaModeContexts[id] = changed
	mode, ok := ArenaModeByID(id)
	if !ok || mode.TeamSize != 3 || mode.TotalPlayers != 6 {
		t.Fatal("匹配未读取共享模式容量")
	}
	if size, ok := gameserver.TeamSizeForArenaMode(id); !ok || size != 3 {
		t.Fatal("分服未读取共享模式容量")
	}
	if _, err := NewTicket(CreateTicketRequest{ArenaModeID: id, PartyID: "party", PartyMemberIDs: []string{"p0", "p1", "p2"}}); err != nil {
		t.Fatal(err)
	}
	a := gameserver.Assignment{MatchID: "match", GameServerID: "server", MapID: "map", ArenaModeID: id, TeamSize: 3, TotalPlayers: 6}
	for team := 0; team < 2; team++ {
		for slot := 0; slot < 3; slot++ {
			a.Roster = append(a.Roster, gameserver.RosterPlayer{PlayerID: fmt.Sprintf("p%d%d", team, slot), TeamID: fmt.Sprintf("team%d", team)})
		}
	}
	if err := a.Validate(); err != nil {
		t.Fatalf("Assignment未读取共享容量: %v", err)
	}
	a.TeamSize = 2
	if err := a.Validate(); err == nil {
		t.Fatal("Assignment不能继续接受旧容量")
	}
}

func TestAllArenaModesHaveExpectedCapacity(t *testing.T) {
	cases := map[string]int{
		"Arena.Mode.Duel1v1": 1,
		"Arena.Mode.Team2v2": 2,
		"Arena.Mode.Team3v3": 3,
		"Arena.Mode.Team4v4": 4,
		"Arena.Mode.Team5v5": 5,
	}
	for id, teamSize := range cases {
		mode, ok := ArenaModeByID(id)
		if !ok {
			t.Fatalf("缺少竞技模式: %s", id)
		}
		if mode.TeamSize != teamSize || mode.TotalPlayers != teamSize*2 {
			t.Fatalf("模式容量错误: %+v", mode)
		}
	}
}

func TestPartyIsRejectedWhenLargerThanTeam(t *testing.T) {
	_, err := NewTicket(CreateTicketRequest{
		TicketID: "ticket-1", ArenaModeID: "Arena.Mode.Team2v2", PartyID: "party-1",
		PartyMemberIDs: []string{"p1", "p2", "p3"}, Region: "us-west",
	})
	if err == nil {
		t.Fatal("3人Party不能进入2v2")
	}
}

func TestTicketKeepsPartyAsAtomicUnit(t *testing.T) {
	ticket, err := NewTicket(CreateTicketRequest{
		TicketID: "ticket-1", ArenaModeID: "Arena.Mode.Team5v5", PartyID: "party-1",
		PartyMemberIDs: []string{"p1", "p2", "p3"}, Region: "us-west",
	})
	if err != nil {
		t.Fatalf("创建匹配票据失败: %v", err)
	}
	if len(ticket.PartyMemberIDs) != 3 || ticket.PartySize != 3 {
		t.Fatalf("Party成员必须完整保留: %+v", ticket)
	}
}
