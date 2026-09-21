package matchmaking

import "testing"

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
