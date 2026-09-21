package integrationadapter

import (
	"context"
	"testing"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// TestMatchToMainArenaEndToEnd（匹配到主竞技场端到端测试）验证跨应用适配后完整分配和迁移链路。
func TestMatchToMainArenaEndToEnd(t *testing.T) {
	now := time.Date(2026, 9, 17, 3, 0, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	gs := gameservercontrol.NewService(
		registry,
		servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now }),
		match.NewResultService(match.NewMemoryResultStore()),
		func() time.Time { return now },
	)
	if err := gs.Register(gameservercontrol.RegisterInput{GameID: "divine-beasts", GameServerID: "arena-1", ServerRoleID: gameservercontract.RoleMainArena, RegionID: "us-west", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:7777", Capacity: 10}); err != nil {
		t.Fatal(err)
	}
	if err := gs.SetReady("arena-1"); err != nil {
		t.Fatal(err)
	}

	adapter := NewArenaControl(gs)
	ids := []string{"match-e2e", "tx-1", "tx-2"}
	next := func(_ string) string { value := ids[0]; ids = ids[1:]; return value }
	orchestrator := matchservice.NewOrchestrator(adapter, next)

	result, err := orchestrator.CreateArenaMatch(matchservice.CreateArenaMatchInput{
		GameID: "divine-beasts", ArenaModeID: gameservercontract.ArenaMode1v1, MapID: "Map.MainArena.Default", RegionID: "us-west",
		Tickets: []matchmaking.Ticket{
			{TicketID: "mm-1", GameID: "divine-beasts", ArenaModeID: gameservercontract.ArenaMode1v1, PartyMemberIDs: []string{"p1"}, PartySize: 1, TeamSize: 1, AllowFill: true, Region: "us-west", State: "searching"},
			{TicketID: "mm-2", GameID: "divine-beasts", ArenaModeID: gameservercontract.ArenaMode1v1, PartyMemberIDs: []string{"p2"}, PartySize: 1, TeamSize: 1, AllowFill: true, Region: "us-west", State: "searching"},
		},
		Sessions: map[string]matchservice.PlayerSession{
			"p1": {PlayerID: "p1", SessionID: "s1", SourceGameServerID: "openworld-1"},
			"p2": {PlayerID: "p2", SessionID: "s2", SourceGameServerID: "openworld-2"},
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	if result.Assignment.GameServerID != "arena-1" || len(result.TransferTickets) != 2 {
		t.Fatalf("端到端结果错误: %+v", result)
	}

	ticket := result.TransferTickets["p1"]
	validated, err := gs.ValidateTransfer(ticket, "arena-1")
	if err != nil {
		t.Fatalf("目标MainArena验证Ticket失败: %v", err)
	}
	if validated.PlayerID != "p1" || validated.MatchID != "match-e2e" {
		t.Fatalf("验证上下文错误: %+v", validated)
	}

	_, err = gs.SubmitMatchResult(context.Background(), match.Result{MatchID: "match-e2e", ArenaModeID: gameservercontract.ArenaMode1v1, GameServerID: "arena-1", StartedAt: now, EndedAt: now.Add(time.Minute), WinningTeamID: "team-a"})
	if err != nil {
		t.Fatalf("提交比赛结果失败: %v", err)
	}
}
