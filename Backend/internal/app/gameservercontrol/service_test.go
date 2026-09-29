package gameservercontrol

import (
	"context"
	"testing"
	"time"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// TestMainArenaLifecycle（主竞技场生命周期测试）覆盖注册、就绪、分配、查询Assignment和玩家迁移票据验证。
func TestMainArenaLifecycle(t *testing.T) {
	now := time.Date(2026, 9, 17, 1, 0, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	transferService := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	resultService := match.NewResultService(match.NewMemoryResultStore())
	service := NewService(registry, transferService, resultService, func() time.Time { return now })

	if err := service.Register(RegisterInput{
		GameID: "divine-beasts", GameServerID: "arena-usw-001", ServerBootID: "boot-arena-usw-001", ServerRoleID: gameservercontract.RoleMainArena,
		RegionID: "us-west", ClusterID: "cluster-a", NodeID: "node-a", WorldID: "World.MainArena",
		PublicEndpoint: "127.0.0.1:7777", BuildVersion: "0.2.0", ProtocolVersion: 2, Capacity: 10,
	}); err != nil {
		t.Fatalf("注册GameServer失败: %v", err)
	}
	if err := service.SetReady("arena-usw-001"); err != nil {
		t.Fatalf("设置Ready失败: %v", err)
	}

	roster := make([]gameservercontract.RosterPlayer, 0, 10)
	for i := 1; i <= 5; i++ {
		roster = append(roster, gameservercontract.RosterPlayer{PlayerID: "a" + string(rune('0'+i)), TeamID: "team-a"})
		roster = append(roster, gameservercontract.RosterPlayer{PlayerID: "b" + string(rune('0'+i)), TeamID: "team-b"})
	}
	assignment, err := service.AllocateMainArena(AllocateMainArenaInput{
		MatchID: "match-001", ArenaModeID: gameservercontract.ArenaMode5v5, MapID: "Map.MainArena.Default",
		RegionID: "us-west", Roster: roster,
	})
	if err != nil {
		t.Fatalf("分配MainArena失败: %v", err)
	}
	if assignment.GameServerID != "arena-usw-001" || assignment.TotalPlayers != 10 || assignment.TeamSize != 5 {
		t.Fatalf("Assignment不正确: %+v", assignment)
	}

	queried, found := service.GetAssignment("arena-usw-001")
	if !found || queried.MatchID != "match-001" {
		t.Fatalf("应可按GameServerID查询Assignment: found=%v assignment=%+v", found, queried)
	}

	ticket, err := service.IssueTransfer(IssueTransferInput{
		TicketID: "transfer-001", GameID: "divine-beasts", PlayerID: "a1", SessionID: "session-a1",
		DestinationGameServerID: "arena-usw-001", DestinationWorldID: "World.MainArena", MatchID: "match-001", TTL: 30 * time.Second,
	})
	if err != nil {
		t.Fatalf("签发迁移票据失败: %v", err)
	}
	if ticket.DestinationEndpoint != "127.0.0.1:7777" {
		t.Fatalf("迁移票据Endpoint错误: %+v", ticket)
	}

	validated, err := service.ValidateTransfer(ticket, "arena-usw-001")
	if err != nil {
		t.Fatalf("目标服务器验证迁移票据失败: %v", err)
	}
	if validated.PlayerID != "a1" || validated.MatchID != "match-001" {
		t.Fatalf("验证结果错误: %+v", validated)
	}
	if validated.GameSessionID == "" || validated.SessionEpoch == 0 ||
		validated.DestinationServerBootID != "boot-arena-usw-001" ||
		validated.DestinationProtocolVersion != 2 {
		t.Fatalf("验证结果缺少完整权威Binding: %+v", validated)
	}
}

// TestSubmitMatchResultReleasesMainArena（比赛结果提交释放服务器测试）验证结算成功后MainArena解除Match绑定。
func TestSubmitMatchResultReleasesMainArena(t *testing.T) {
	now := time.Date(2026, 9, 17, 1, 0, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	transferService := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
	resultService := match.NewResultService(match.NewMemoryResultStore())
	service := NewService(registry, transferService, resultService, func() time.Time { return now })

	service.Register(RegisterInput{GameID: "divine-beasts", GameServerID: "arena-1", ServerBootID: "boot-arena-1", ServerRoleID: gameservercontract.RoleMainArena, RegionID: "us-west", WorldID: "World.MainArena", PublicEndpoint: "127.0.0.1:7777", ProtocolVersion: 1, Capacity: 2})
	service.SetReady("arena-1")
	_, err := service.AllocateMainArena(AllocateMainArenaInput{
		MatchID: "match-1", ArenaModeID: gameservercontract.ArenaMode1v1, MapID: "Map.MainArena.Default", RegionID: "us-west",
		Roster: []gameservercontract.RosterPlayer{{PlayerID: "p1", TeamID: "team-a"}, {PlayerID: "p2", TeamID: "team-b"}},
	})
	if err != nil {
		t.Fatal(err)
	}

	stored, err := service.SubmitMatchResult(context.Background(), match.Result{MatchID: "match-1", ArenaModeID: gameservercontract.ArenaMode1v1, GameServerID: "arena-1", StartedAt: now, EndedAt: now.Add(time.Minute), WinningTeamID: "team-a"})
	if err != nil {
		t.Fatalf("提交比赛结果失败: %v", err)
	}
	if stored.ResultID == "" {
		t.Fatal("应返回ResultID")
	}
	if _, found := service.GetAssignment("arena-1"); found {
		t.Fatal("比赛结果提交成功后应清理MainArena Assignment")
	}

	// 释放后同一实例应再次可分配。
	_, err = registry.Allocate(gameserver.AllocationRequest{RoleID: gameserver.RoleMainArena, RegionID: "us-west", RequiredCapacity: 2, MatchID: "match-2"})
	if err != nil {
		t.Fatalf("释放后的MainArena应可再次分配: %v", err)
	}
}

// TestWorldAllocationAndTransfer（常驻世界分配与跨服测试）验证OpenWorld/Village体验按正式角色分池，
// 并覆盖世界Assignment、TransferTicket一次性消费及容量预留提交。
func TestWorldAllocationAndTransfer(t *testing.T) {
	now := time.Date(2026, 9, 21, 4, 30, 0, 0, time.UTC)
	cases := []struct {
		name         string
		serverID     string
		roleID       string
		experienceID string
		worldID      string
	}{
		{"OpenWorldHub", "ow-hub-1", gameservercontract.RoleOpenWorld, gameservercontract.ExperienceOpenWorldHub, "World.OpenWorld.Hub"},
		{"LegacyLobbyExperienceAlias", "ow-legacy-hub-1", gameservercontract.RoleOpenWorld, gameservercontract.ExperienceLobbyMain, "World.OpenWorld.Hub"},
		{"OpenWorldMain", "ow-main-1", gameservercontract.RoleOpenWorld, gameservercontract.ExperienceOpenWorldMain, "World.OpenWorld.Main"},
		{"VillageMain", "village-main-1", gameservercontract.RoleVillage, gameservercontract.ExperienceVillageMain, "World.Village.Main"},
		{"VillageTutorial", "village-tutorial-1", gameservercontract.RoleVillage, gameservercontract.ExperienceVillageTutorial, "World.Village.Tutorial"},
		{"VillageTraining", "village-training-1", gameservercontract.RoleVillage, gameservercontract.ExperienceVillageTraining, "World.Village.Training"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			registry := gameserver.NewRegistry()
			transferService := servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now })
			service := NewService(registry, transferService, match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
			if err := service.Register(RegisterInput{GameID: "divine-beasts", GameServerID: tc.serverID, ServerBootID: "boot-" + tc.serverID, ServerRoleID: tc.roleID, ExperienceID: tc.experienceID, RegionID: "us-west", WorldID: tc.worldID, PublicEndpoint: "127.0.0.1:7777", ProtocolVersion: 1, Capacity: 100}); err != nil {
				t.Fatal(err)
			}
			if err := service.SetReady(tc.serverID); err != nil {
				t.Fatal(err)
			}
			assignment, err := service.AllocateWorld(context.Background(), AllocateWorldInput{ExperienceID: tc.experienceID, WorldID: tc.worldID, RegionID: "us-west", PlayerSlots: 1})
			if err != nil {
				t.Fatalf("分配世界失败: %v", err)
			}
			if assignment.ServerRoleID != tc.roleID || assignment.ExperienceID != tc.experienceID || assignment.GameServerID != tc.serverID {
				t.Fatalf("世界Assignment错误: %+v", assignment)
			}
			instance, _ := registry.Get(tc.serverID)
			if instance.ReservedPlayers != 1 {
				t.Fatalf("签票前应预留1个容量，实际=%d", instance.ReservedPlayers)
			}
			ticket, err := service.IssueTransfer(IssueTransferInput{TicketID: "ticket-" + tc.serverID, GameID: "divine-beasts", PlayerID: "player-1", SessionID: "session-1", DestinationGameServerID: tc.serverID, TTL: 30 * time.Second})
			if err != nil {
				t.Fatalf("签发世界迁移票据失败: %v", err)
			}
			if ticket.AssignmentID != assignment.AssignmentID || ticket.DestinationExperienceID != tc.experienceID || ticket.DestinationWorldID != tc.worldID {
				t.Fatalf("票据未绑定世界Assignment: %+v", ticket)
			}
			if _, err := service.ValidateTransferContext(context.Background(), ticket, tc.serverID); err != nil {
				t.Fatalf("首次消费票据失败: %v", err)
			}
			if _, err := service.ValidateTransferContext(context.Background(), ticket, tc.serverID); err == nil {
				t.Fatal("同一TransferTicket第二次消费必须被拒绝")
			}
			instance, _ = registry.Get(tc.serverID)
			if instance.ReservedPlayers != 0 {
				t.Fatalf("票据成功消费后容量预留应提交，实际=%d", instance.ReservedPlayers)
			}
		})
	}
}

// TestLobbyExperienceUsesOpenWorldRole（大厅体验角色归属测试）确保新旧大厅体验标识都映射到OpenWorld，旧角色被拒绝。
func TestLobbyExperienceUsesOpenWorldRole(t *testing.T) {
	for _, experienceID := range []string{gameservercontract.ExperienceOpenWorldHub, gameservercontract.ExperienceLobbyMain} {
		role, ok := gameservercontract.RoleForExperience(experienceID)
		if !ok || role != gameservercontract.RoleOpenWorld {
			t.Fatalf("大厅体验%s必须映射到OpenWorld，role=%s ok=%v", experienceID, role, ok)
		}
	}
	if gameserver.IsKnownRole("GameServer.Role.Lobby") {
		t.Fatal("大厅属于OpenWorld体验，不得注册独立Lobby角色")
	}
}

// TestOpenWorldHubCanRegisterReadyButCannotSubmitMatchResult（大厅体验生命周期和权限测试）确保大厅由OpenWorld承载且不能提交竞技结果。
func TestOpenWorldHubCanRegisterReadyButCannotSubmitMatchResult(t *testing.T) {
	now := time.Date(2026, 9, 21, 4, 30, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	service := NewService(registry, servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now }), match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	if err := service.Register(RegisterInput{GameID: "divine-beasts", GameServerID: "openworld-hub-1", ServerBootID: "boot-openworld-hub-1", ServerRoleID: gameservercontract.RoleOpenWorld, ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", WorldID: "World.OpenWorld.Hub", PublicEndpoint: "127.0.0.1:7777", ProtocolVersion: 1, Capacity: 100}); err != nil {
		t.Fatalf("OpenWorld大厅体验注册失败：%v", err)
	}
	if err := service.SetReady("openworld-hub-1"); err != nil {
		t.Fatalf("OpenWorld大厅体验设置Ready失败：%v", err)
	}
	instance, found := registry.Get("openworld-hub-1")
	if !found || instance.Status != gameserver.StatusReady || instance.RoleID != gameservercontract.RoleOpenWorld {
		t.Fatalf("大厅体验应以OpenWorld角色Ready，found=%v instance=%+v", found, instance)
	}
	_, err := service.SubmitMatchResult(context.Background(), match.Result{MatchID: "forbidden-match", GameServerID: "openworld-hub-1"})
	if err == nil {
		t.Fatal("OpenWorld不得提交MainArena权威比赛结果")
	}
}

// TestRegisterRejectsUnknownRoleAndMismatchedExperience（注册角色校验测试）拒绝未知角色、废弃角色和角色体验错配。
func TestRegisterRejectsUnknownRoleAndMismatchedExperience(t *testing.T) {
	now := time.Date(2026, 9, 21, 4, 30, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	service := NewService(registry, servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now }), match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	base := RegisterInput{GameID: "divine-beasts", ServerBootID: "boot-test", RegionID: "us-west", PublicEndpoint: "127.0.0.1:7777", ProtocolVersion: 1, Capacity: 10}
	unknown := base
	unknown.GameServerID = "unknown-role"
	unknown.ServerRoleID = "GameServer.Role.Unknown"
	unknown.ExperienceID = gameservercontract.ExperienceLobbyMain
	if err := service.Register(unknown); err == nil {
		t.Fatal("未知ServerRole必须拒绝注册")
	}
	removedRole := base
	removedRole.GameServerID = "removed-lobby-role"
	removedRole.ServerRoleID = "GameServer.Role.Lobby"
	removedRole.ExperienceID = gameservercontract.ExperienceOpenWorldHub
	if err := service.Register(removedRole); err == nil {
		t.Fatal("独立Lobby ServerRole必须拒绝注册")
	}
	mismatch := base
	mismatch.GameServerID = "role-experience-mismatch"
	mismatch.ServerRoleID = gameservercontract.RoleVillage
	mismatch.ExperienceID = gameservercontract.ExperienceOpenWorldMain
	if err := service.Register(mismatch); err == nil {
		t.Fatal("Village角色不得承载OpenWorld体验")
	}
}

// TestWorldAllocationRejectsRoleExperienceMismatch（分配角色体验一致性测试）确保注册表中的错配实例不会进入玩家分配结果。
func TestWorldAllocationRejectsRoleExperienceMismatch(t *testing.T) {

	now := time.Date(2026, 9, 21, 4, 30, 0, 0, time.UTC)
	registry := gameserver.NewRegistry()
	registry.Register(gameserver.Instance{
		ID: "mismatched-openworld-experience", RoleID: gameservercontract.RoleVillage,
		ExperienceID: gameservercontract.ExperienceOpenWorldMain,
		RegionID:     "us-west", WorldID: "World.OpenWorld.Hub", PublicEndpoint: "127.0.0.1:7777",
		Capacity: 10, Status: gameserver.StatusReady,
	})
	service := NewService(registry, servertransfer.NewService([]byte("01234567890123456789012345678901"), func() time.Time { return now }), match.NewResultService(match.NewMemoryResultStore()), func() time.Time { return now })
	_, err := service.AllocateWorld(context.Background(), AllocateWorldInput{
		ExperienceID: gameservercontract.ExperienceOpenWorldHub,
		WorldID:      "World.OpenWorld.Hub",
		RegionID:     "us-west",
		PlayerSlots:  1,
	})
	if err == nil {
		t.Fatal("角色或体验错配的GameServer不得被分配")
	}
}
