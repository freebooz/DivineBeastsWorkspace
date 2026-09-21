package matchservice

import (
	"testing"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// fakeArenaControl（测试用竞技场控制端口）记录MatchService请求的容量和Roster，避免测试依赖真实GameServerControlService。
type fakeArenaControl struct {
	allocation ArenaAllocationRequest
}

func (f *fakeArenaControl) AllocateMainArena(req ArenaAllocationRequest) (gameservercontract.Assignment, error) {
	f.allocation = req
	return gameservercontract.Assignment{
		MatchID: req.MatchID, ArenaModeID: req.ArenaModeID, MapID: req.MapID, TeamSize: req.TeamSize,
		TotalPlayers: req.TotalPlayers, GameServerID: "arena-001", Roster: req.Roster,
	}, nil
}

func (f *fakeArenaControl) IssuePlayerTransfer(req PlayerTransferRequest) (servertransfer.Ticket, error) {
	return servertransfer.Ticket{TicketID: req.TicketID, PlayerID: req.PlayerID, SessionID: req.SessionID, DestinationGameServerID: req.DestinationGameServerID, DestinationEndpoint: "127.0.0.1:7777", DestinationWorldID: "World.MainArena", MatchID: req.MatchID}, nil
}

// TestCreateArenaMatch5v5（5v5比赛创建测试）验证10名玩家分为两个5人队并请求10人容量。
func TestCreateArenaMatch5v5(t *testing.T) {
	control := &fakeArenaControl{}
	ids := sequenceIDs("match-001", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "t10")
	orchestrator := NewOrchestrator(control, ids)

	tickets := []matchmaking.Ticket{
		ticket("party-a", gameservercontract.ArenaMode5v5, "us-west", "a1", "a2", "a3"),
		ticket("party-b", gameservercontract.ArenaMode5v5, "us-west", "b1", "b2"),
		ticket("party-c", gameservercontract.ArenaMode5v5, "us-west", "c1", "c2", "c3"),
		ticket("party-d", gameservercontract.ArenaMode5v5, "us-west", "d1", "d2"),
	}
	sessions := sessionsFor(tickets)

	result, err := orchestrator.CreateArenaMatch(CreateArenaMatchInput{GameID: "divine-beasts", ArenaModeID: gameservercontract.ArenaMode5v5, MapID: "Map.MainArena.Default", RegionID: "us-west", Tickets: tickets, Sessions: sessions})
	if err != nil {
		t.Fatalf("创建5v5比赛失败: %v", err)
	}
	if control.allocation.TotalPlayers != 10 || control.allocation.TeamSize != 5 {
		t.Fatalf("分配容量错误: %+v", control.allocation)
	}
	if len(result.TransferTickets) != 10 {
		t.Fatalf("应为10名玩家签发迁移票据，实际=%d", len(result.TransferTickets))
	}
	assertPartySameTeam(t, result.Assignment.Roster, []string{"a1", "a2", "a3"})
	assertPartySameTeam(t, result.Assignment.Roster, []string{"b1", "b2"})
	assertPartySameTeam(t, result.Assignment.Roster, []string{"c1", "c2", "c3"})
	assertPartySameTeam(t, result.Assignment.Roster, []string{"d1", "d2"})
}

// TestCreateArenaMatch3v3WithPartialParties（3v3非满编组队测试）验证2人Party可与单排补人成队且Party不拆分。
func TestCreateArenaMatch3v3WithPartialParties(t *testing.T) {
	control := &fakeArenaControl{}
	ids := sequenceIDs("match-003", "x1", "x2", "x3", "x4", "x5", "x6")
	orchestrator := NewOrchestrator(control, ids)
	tickets := []matchmaking.Ticket{
		ticket("party-a", gameservercontract.ArenaMode3v3, "us-west", "a1", "a2"),
		ticket("", gameservercontract.ArenaMode3v3, "us-west", "s1"),
		ticket("party-b", gameservercontract.ArenaMode3v3, "us-west", "b1", "b2"),
		ticket("", gameservercontract.ArenaMode3v3, "us-west", "s2"),
	}

	result, err := orchestrator.CreateArenaMatch(CreateArenaMatchInput{GameID: "divine-beasts", ArenaModeID: gameservercontract.ArenaMode3v3, MapID: "Map.MainArena.Default", RegionID: "us-west", Tickets: tickets, Sessions: sessionsFor(tickets)})
	if err != nil {
		t.Fatalf("创建3v3比赛失败: %v", err)
	}
	if result.Assignment.TeamSize != 3 || result.Assignment.TotalPlayers != 6 {
		t.Fatalf("3v3人数错误: %+v", result.Assignment)
	}
	assertPartySameTeam(t, result.Assignment.Roster, []string{"a1", "a2"})
	assertPartySameTeam(t, result.Assignment.Roster, []string{"b1", "b2"})
}

func ticket(partyID, mode, region string, players ...string) matchmaking.Ticket {
	return matchmaking.Ticket{TicketID: "ticket-" + players[0], GameID: "divine-beasts", ArenaModeID: mode, PartyID: partyID, PartyMemberIDs: players, PartySize: len(players), TeamSize: mustTeamSize(mode), AllowFill: true, Region: region, State: "searching"}
}

func mustTeamSize(mode string) int {
	size, _ := gameservercontract.TeamSizeForArenaMode(mode)
	return size
}

func sessionsFor(tickets []matchmaking.Ticket) map[string]PlayerSession {
	result := map[string]PlayerSession{}
	for _, ticket := range tickets {
		for _, playerID := range ticket.PartyMemberIDs {
			result[playerID] = PlayerSession{PlayerID: playerID, SessionID: "session-" + playerID, SourceGameServerID: "openworld-1"}
		}
	}
	return result
}

func assertPartySameTeam(t *testing.T, roster []gameservercontract.RosterPlayer, players []string) {
	t.Helper()
	team := ""
	for _, playerID := range players {
		found := false
		for _, entry := range roster {
			if entry.PlayerID != playerID {
				continue
			}
			found = true
			if team == "" {
				team = entry.TeamID
			} else if entry.TeamID != team {
				t.Fatalf("Party被拆分: player=%s team=%s expectedTeam=%s", playerID, entry.TeamID, team)
			}
		}
		if !found {
			t.Fatalf("Roster中缺少玩家%s", playerID)
		}
	}
}

func sequenceIDs(values ...string) func(string) string {
	index := 0
	return func(_ string) string {
		value := values[index]
		index++
		return value
	}
}
