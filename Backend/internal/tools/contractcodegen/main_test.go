package main

import (
	"strings"
	"testing"
)

// TestValidateRoleExperienceMappingsRequiresCompleteUniqueCoverage（角色体验目录覆盖测试）防止未知ID、遗漏项或重复体验进入生成物。
func TestValidateRoleExperienceMappingsRequiresCompleteUniqueCoverage(t *testing.T) {
	roles := []string{"GameServer.Role.OpenWorld", "GameServer.Role.Village"}
	experiences := []string{"Experience.Lobby.Main", "Experience.OpenWorld.Main", "Experience.Village.Main"}
	validMappings := []roleExperienceMapping{
		{ServerRole: "GameServer.Role.OpenWorld", Experiences: []string{"Experience.Lobby.Main", "Experience.OpenWorld.Main"}},
		{ServerRole: "GameServer.Role.Village", Experiences: []string{"Experience.Village.Main"}},
	}
	validateRoleExperienceMappings(roles, experiences, validMappings)

	assertPanics := func(name string, roleValues, experienceValues []string, mappings []roleExperienceMapping) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Errorf("%s：非法目录必须在生成前拒绝", name)
			}
		}()
		validateRoleExperienceMappings(roleValues, experienceValues, mappings)
	}
	assertPanics("重复体验", roles, experiences, []roleExperienceMapping{
		{ServerRole: "GameServer.Role.OpenWorld", Experiences: []string{"Experience.Lobby.Main"}},
		{ServerRole: "GameServer.Role.Village", Experiences: []string{"Experience.Lobby.Main"}},
	})
	assertPanics("未声明角色", roles, experiences, []roleExperienceMapping{
		{ServerRole: "GameServer.Role.Unknown", Experiences: []string{"Experience.Lobby.Main"}},
	})
	assertPanics("未覆盖体验", roles, append(experiences, "Experience.Unmapped"), validMappings)
}

// TestGeneratedCatalogContainsSharedMappings（共享目录映射生成测试）保证Go和UE消费的目录包含相同角色体验对。
func TestGeneratedCatalogContainsSharedMappings(t *testing.T) {
	catalog := testDivineBeastsCatalog()
	goCatalog := string(generateDivineBeastsGo(catalog))
	cppCatalog := string(generateDivineBeastsCpp(catalog))
	if !strings.Contains(goCatalog, `"Experience.Lobby.Main": "GameServer.Role.OpenWorld"`) {
		t.Fatalf("Go生成目录未将历史Lobby体验映射到OpenWorld：%s", goCatalog)
	}
	if !strings.Contains(cppCatalog, `ExperienceServerRoleMapping{"Experience.Lobby.Main", "GameServer.Role.OpenWorld"}`) {
		t.Fatalf("C++生成目录未将历史Lobby体验映射到OpenWorld：%s", cppCatalog)
	}
}

// TestGeneratedCppCatalogMatchesRuntimeAdapter（C++目录与运行时适配器符号一致性测试）防止生成成功但UE消费端引用不存在符号。
func TestGeneratedCppCatalogMatchesRuntimeAdapter(t *testing.T) {
	generated := string(generateDivineBeastsCpp(testDivineBeastsCatalog()))
	for _, symbol := range []string{
		"namespace DivineBeasts::Contracts",
		"GameId", "ProjectId", "ContractVersion", "CatalogVersion", "GeneratedRevision",
		"ServerRoles", "ExperienceIds", "ArenaModeIds", "ExperienceMappings", "ArenaModeMappings",
		"ClientServerMinimumContractVersion", "ClientServerMaximumExclusiveContractVersion",
		"ServerBackendMinimumContractVersion", "ServerBackendMaximumExclusiveContractVersion",
	} {
		if !strings.Contains(generated, symbol) {
			t.Errorf("C++目录缺少UE运行时适配器所需符号 %q：%s", symbol, generated)
		}
	}
	if strings.Contains(generated, "DivineBeasts::Generated") {
		t.Fatal("生成目录不得使用UE适配器无法消费的第二套命名空间")
	}
	goGenerated := string(generateDivineBeastsGo(testDivineBeastsCatalog()))
	for _, symbol := range []string{"GameID =", "ProjectID =", "ContractVersion =", "GeneratedRevision =", "CatalogVersion ="} {
		if !strings.Contains(goGenerated, symbol) {
			t.Errorf("Go目录缺少共享项目元数据 %q：%s", symbol, goGenerated)
		}
	}
}

// TestInclusiveCompatibilityMaximumConversion（兼容矩阵闭区间上界转换测试）确保Shared中的2.x能准确变为运行时半开区间3.0.0。
func TestInclusiveCompatibilityMaximumConversion(t *testing.T) {
	for input, expected := range map[string]string{
		"2.x":   "3.0.0",
		"1.x":   "2.0.0",
		"1.4.x": "1.5.0",
		"1.4.7": "1.4.8",
		"0.9.x": "0.10.0",
	} {
		actual, err := inclusiveMaxToExclusive(input)
		if err != nil {
			t.Errorf("inclusiveMaxToExclusive(%q)意外失败：%v", input, err)
			continue
		}
		if actual != expected {
			t.Errorf("inclusiveMaxToExclusive(%q)=%q，期望 %q", input, actual, expected)
		}
	}
	if _, err := inclusiveMaxToExclusive("x"); err == nil {
		t.Fatal("缺少主版本号的通配符必须被拒绝，不能扩大兼容范围")
	}
}

// TestGeneratedRevisionIsPathStableAndInputSensitive（生成修订号稳定性测试）确保输入枚举顺序不影响哈希且任一源内容变化都会被识别。
func TestGeneratedRevisionIsPathStableAndInputSensitive(t *testing.T) {
	first := map[string][]byte{"b.json": []byte("two"), "a.json": []byte("one")}
	second := map[string][]byte{"a.json": []byte("one"), "b.json": []byte("two")}
	if generateRevision(first) != generateRevision(second) {
		t.Fatal("相同路径与内容的Revision不能受map遍历顺序影响")
	}
	second["b.json"] = []byte("changed")
	if generateRevision(first) == generateRevision(second) {
		t.Fatal("源契约内容变化必须改变GeneratedRevision")
	}
}

// TestArenaModeMappingsRequireCompleteAndConsistentCoverage（竞技模式映射完整性测试）拒绝遗漏、重复及角色体验错配。
func TestArenaModeMappingsRequireCompleteAndConsistentCoverage(t *testing.T) {
	catalog := testDivineBeastsCatalog()
	validateArenaModeMappings(catalog.arenaModes, catalog.experiences, catalog.serverRoles, catalog.experienceMappings, catalog.arenaModeMappings)
	assertPanics := func(name string, mappings []arenaModeMapping) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Errorf("%s映射必须拒绝", name)
			}
		}()
		validateArenaModeMappings(catalog.arenaModes, catalog.experiences, catalog.serverRoles, catalog.experienceMappings, mappings)
	}
	assertPanics("缺失竞技模式", nil)
	assertPanics("角色体验错配", []arenaModeMapping{{ArenaMode: "Arena.Mode.Duel1v1", ServerRole: "GameServer.Role.MainArena", Experience: "Experience.Lobby.Main"}})
}

func testDivineBeastsCatalog() divineBeastsCatalog {
	return divineBeastsCatalog{
		gameID: "divinebeasts", projectID: "DivineBeastsArena", contractVersion: "2.0.0",
		catalogVersion: 1, generatedRevision: strings.Repeat("a", 64),
		serverRoles: []string{"GameServer.Role.OpenWorld", "GameServer.Role.MainArena"},
		experiences: []string{"Experience.OpenWorld.Hub", "Experience.Lobby.Main", "Experience.MainArena.Main"},
		arenaModes:  []string{"Arena.Mode.Duel1v1"},
		experienceMappings: []roleExperienceMapping{
			{ServerRole: "GameServer.Role.OpenWorld", Experiences: []string{"Experience.OpenWorld.Hub", "Experience.Lobby.Main"}},
			{ServerRole: "GameServer.Role.MainArena", Experiences: []string{"Experience.MainArena.Main"}},
		},
		arenaModeMappings:          []arenaModeMapping{{ArenaMode: "Arena.Mode.Duel1v1", ServerRole: "GameServer.Role.MainArena", Experience: "Experience.MainArena.Main"}},
		clientServerCompatibility:  contractCompatibility{MinimumInclusive: "2.0.0", MaximumExclusive: "3.0.0"},
		serverBackendCompatibility: contractCompatibility{MinimumInclusive: "2.0.0", MaximumExclusive: "3.0.0"},
	}
}
