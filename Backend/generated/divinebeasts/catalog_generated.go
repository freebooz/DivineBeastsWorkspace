// Code generated from Shared/Contracts/Games/DivineBeasts. DO NOT EDIT.
// Package divinebeasts（神兽联盟共享Go绑定）保存项目身份、版本与玩法目录。
package divinebeasts

const (
	GameID                                       = "divinebeasts"
	ProjectID                                    = "DivineBeastsArena"
	ContractVersion                              = "2.0.0"
	GeneratedRevision                            = "99d0928cff73710c1e71cbf4c74817c8e73d26ecf0a0dc2514a7e9b307edd3b7"
	CatalogVersion                               = 1
	ClientServerMinimumContractVersion           = "2.0.0"
	ClientServerMaximumExclusiveContractVersion  = "3.0.0"
	ServerBackendMinimumContractVersion          = "2.0.0"
	ServerBackendMaximumExclusiveContractVersion = "3.0.0"
	GameServerRoleOpenWorld                      = "GameServer.Role.OpenWorld"
	GameServerRoleVillage                        = "GameServer.Role.Village"
	GameServerRoleMainArena                      = "GameServer.Role.MainArena"
	ExperienceLobbyMain                          = "Experience.Lobby.Main"
	ExperienceOpenWorldMain                      = "Experience.OpenWorld.Main"
	ExperienceVillageMain                        = "Experience.Village.Main"
	ExperienceVillageTutorial                    = "Experience.Village.Tutorial"
	ExperienceVillageTraining                    = "Experience.Village.Training"
	ExperienceMainArenaMain                      = "Experience.MainArena.Main"
	ExperienceOpenWorldHub                       = "Experience.OpenWorld.Hub"
	ArenaModeDuel1v1                             = "Arena.Mode.Duel1v1"
	ArenaModeTeam2v2                             = "Arena.Mode.Team2v2"
	ArenaModeTeam3v3                             = "Arena.Mode.Team3v3"
	ArenaModeTeam4v4                             = "Arena.Mode.Team4v4"
	ArenaModeTeam5v5                             = "Arena.Mode.Team5v5"
)

// ExperienceServerRoles（体验对应的服务器角色）由Shared ServerCatalog生成，包含保留的兼容体验映射。
var ExperienceServerRoles = map[string]string{
	"Experience.OpenWorld.Hub":    "GameServer.Role.OpenWorld",
	"Experience.OpenWorld.Main":   "GameServer.Role.OpenWorld",
	"Experience.Lobby.Main":       "GameServer.Role.OpenWorld",
	"Experience.Village.Main":     "GameServer.Role.Village",
	"Experience.Village.Tutorial": "GameServer.Role.Village",
	"Experience.Village.Training": "GameServer.Role.Village",
	"Experience.MainArena.Main":   "GameServer.Role.MainArena",
}

// ArenaModeContexts（竞技模式对应的权威角色与体验）由Shared ServerCatalog生成。
type ArenaModeContext struct{ ServerRoleID, ExperienceID string }

var ArenaModeContexts = map[string]ArenaModeContext{
	"Arena.Mode.Duel1v1": {ServerRoleID: "GameServer.Role.MainArena", ExperienceID: "Experience.MainArena.Main"},
	"Arena.Mode.Team2v2": {ServerRoleID: "GameServer.Role.MainArena", ExperienceID: "Experience.MainArena.Main"},
	"Arena.Mode.Team3v3": {ServerRoleID: "GameServer.Role.MainArena", ExperienceID: "Experience.MainArena.Main"},
	"Arena.Mode.Team4v4": {ServerRoleID: "GameServer.Role.MainArena", ExperienceID: "Experience.MainArena.Main"},
	"Arena.Mode.Team5v5": {ServerRoleID: "GameServer.Role.MainArena", ExperienceID: "Experience.MainArena.Main"},
}
