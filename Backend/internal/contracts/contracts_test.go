package contracts_test

import (
	"testing"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
)

// TestArenaModeTeamSizes（竞技模式人数映射测试）确保Shared/Backend对1v1~5v5的单队人数理解完全一致。
func TestArenaModeTeamSizes(t *testing.T) {
	cases := map[string]int{
		gameservercontract.ArenaMode1v1: 1,
		gameservercontract.ArenaMode2v2: 2,
		gameservercontract.ArenaMode3v3: 3,
		gameservercontract.ArenaMode4v4: 4,
		gameservercontract.ArenaMode5v5: 5,
	}
	for id, expected := range cases {
		got, ok := gameservercontract.TeamSizeForArenaMode(id)
		if !ok {
			t.Fatalf("竞技模式 %s 应被识别", id)
		}
		if got != expected {
			t.Fatalf("竞技模式 %s 的TeamSize=%d，期望=%d", id, got, expected)
		}
	}
}

// TestAssignmentValidatesRoster（服务器比赛分配名单校验测试）确保MainArena分配中的玩家数量与模式定义一致。
func TestAssignmentValidatesRoster(t *testing.T) {
	assignment := gameservercontract.Assignment{
		MatchID:      "match-001",
		ArenaModeID:  gameservercontract.ArenaMode3v3,
		MapID:        "Map.MainArena.Default",
		TeamSize:     3,
		TotalPlayers: 6,
		GameServerID: "arena-usw-001",
		Roster: []gameservercontract.RosterPlayer{
			{PlayerID: "p1", TeamID: "team-a"},
			{PlayerID: "p2", TeamID: "team-a"},
			{PlayerID: "p3", TeamID: "team-a"},
			{PlayerID: "p4", TeamID: "team-b"},
			{PlayerID: "p5", TeamID: "team-b"},
			{PlayerID: "p6", TeamID: "team-b"},
		},
	}
	if err := assignment.Validate(); err != nil {
		t.Fatalf("合法3v3分配不应失败: %v", err)
	}

	assignment.Roster = assignment.Roster[:5]
	if err := assignment.Validate(); err == nil {
		t.Fatal("Roster人数不足时应校验失败")
	}
}

// TestGameServerRoles（正式服务器角色测试）防止重新引入独立Lobby Server角色。
func TestGameServerRoles(t *testing.T) {
	roles := []string{
		gameservercontract.RoleOpenWorld,
		gameservercontract.RoleVillage,
		gameservercontract.RoleMainArena,
	}
	want := []string{
		"GameServer.Role.OpenWorld",
		"GameServer.Role.Village",
		"GameServer.Role.MainArena",
	}
	for i := range want {
		if roles[i] != want[i] {
			t.Fatalf("服务器角色[%d]=%s，期望=%s", i, roles[i], want[i])
		}
	}
}

// TestExperienceRoleMapping（体验到服务器角色映射测试）确保大厅并入OpenWorld而教学/训练并入Village。
func TestExperienceRoleMapping(t *testing.T) {
	cases := map[string]string{
		gameservercontract.ExperienceOpenWorldHub:    gameservercontract.RoleOpenWorld,
		gameservercontract.ExperienceOpenWorldMain:   gameservercontract.RoleOpenWorld,
		gameservercontract.ExperienceVillageMain:     gameservercontract.RoleVillage,
		gameservercontract.ExperienceVillageTutorial: gameservercontract.RoleVillage,
		gameservercontract.ExperienceVillageTraining: gameservercontract.RoleVillage,
		gameservercontract.ExperienceMainArenaMain:   gameservercontract.RoleMainArena,
	}
	for experienceID, wantRole := range cases {
		got, ok := gameservercontract.RoleForExperience(experienceID)
		if !ok || got != wantRole {
			t.Fatalf("体验%s映射角色=%s, ok=%v，期望=%s", experienceID, got, ok, wantRole)
		}
	}
	if _, ok := gameservercontract.RoleForExperience("Experience.Lobby.Main"); ok {
		t.Fatal("不得重新引入独立Lobby体验/服务器角色模型")
	}
}
