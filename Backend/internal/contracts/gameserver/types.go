// Package gameserver（游戏服务器共享契约适配）定义Go业务层稳定使用的GameServer DTO。
// 这些类型不依赖Proto消息；稳定目录常量和角色映射直接消费Shared契约生成绑定，避免重复维护字符串ID。
package gameserver

import (
	"errors"
	"fmt"

	divinebeastscatalog "divinebeasts/backend/generated/divinebeasts"
)

const (
	// RoleOpenWorld（常驻世界服务器角色）承载大厅、主城和开放世界区域。
	RoleOpenWorld = divinebeastscatalog.GameServerRoleOpenWorld
	// RoleVillage（新手村服务器角色）承载正常新手村、教学和训练体验。
	RoleVillage = divinebeastscatalog.GameServerRoleVillage
	// RoleMainArena（主竞技场服务器角色）承载1v1至5v5短生命周期竞技比赛。
	RoleMainArena = divinebeastscatalog.GameServerRoleMainArena

	// ExperienceLobbyMain（历史大厅体验标识）仅兼容映射到OpenWorld；新配置使用ExperienceOpenWorldHub。
	ExperienceLobbyMain = divinebeastscatalog.ExperienceLobbyMain
	// ExperienceOpenWorldHub（大厅体验）是OpenWorld默认承载体验。
	ExperienceOpenWorldHub = divinebeastscatalog.ExperienceOpenWorldHub
	// ExperienceOpenWorldMain（开放世界主体验）由OpenWorld服务器角色承载。
	ExperienceOpenWorldMain = divinebeastscatalog.ExperienceOpenWorldMain
	// ExperienceVillageMain（新手村主体验）由Village服务器角色承载。
	ExperienceVillageMain = divinebeastscatalog.ExperienceVillageMain
	// ExperienceVillageTutorial（教学体验）属于Village，不是独立ServerRole。
	ExperienceVillageTutorial = divinebeastscatalog.ExperienceVillageTutorial
	// ExperienceVillageTraining（训练体验）属于Village，不是独立ServerRole。
	ExperienceVillageTraining = divinebeastscatalog.ExperienceVillageTraining
	// ExperienceMainArenaMain（主竞技场体验）由MainArena服务器角色承载。
	ExperienceMainArenaMain = divinebeastscatalog.ExperienceMainArenaMain

	// ArenaMode1v1（1v1竞技模式）表示每队1名玩家。
	ArenaMode1v1 = divinebeastscatalog.ArenaModeDuel1v1
	// ArenaMode2v2（2v2竞技模式）表示每队2名玩家。
	ArenaMode2v2 = divinebeastscatalog.ArenaModeTeam2v2
	// ArenaMode3v3（3v3竞技模式）表示每队3名玩家。
	ArenaMode3v3 = divinebeastscatalog.ArenaModeTeam3v3
	// ArenaMode4v4（4v4竞技模式）表示每队4名玩家。
	ArenaMode4v4 = divinebeastscatalog.ArenaModeTeam4v4
	// ArenaMode5v5（5v5竞技模式）表示每队5名玩家。
	ArenaMode5v5 = divinebeastscatalog.ArenaModeTeam5v5
)

var arenaTeamSizes = map[string]int{
	ArenaMode1v1: 1,
	ArenaMode2v2: 2,
	ArenaMode3v3: 3,
	ArenaMode4v4: 4,
	ArenaMode5v5: 5,
}

// RoleForExperience（根据体验解析服务器角色）读取由Shared ServerCatalog生成的正式角色映射。
func RoleForExperience(experienceID string) (string, bool) {
	roleID, ok := divinebeastscatalog.ExperienceServerRoles[experienceID]
	return roleID, ok
}

// TeamSizeForArenaMode（获取竞技模式单队人数）返回1v1至5v5标准TeamSize。
func TeamSizeForArenaMode(arenaModeID string) (int, bool) {
	size, ok := arenaTeamSizes[arenaModeID]
	return size, ok
}

// Endpoint（网络端点）描述Game Client或服务间连接所需的Host与Port。
type Endpoint struct {
	Host string // Host（主机名或IP地址）。
	Port uint32 // Port（网络端口）。
}

// RegisterRequest（游戏服务器注册请求）描述Dedicated Server启动后提交给控制面的注册信息。
type RegisterRequest struct {
	GameID           string   // GameID（游戏ID）。
	GameServerID     string   // GameServerID（游戏服务器实例ID）。
	ServerBootID     string   // ServerBootID（服务器进程启动代次，同一实例ID重启后必须变化）。
	ServerRoleID     string   // ServerRoleID（OpenWorld/Village/MainArena服务器角色）。
	ExperienceID     string   // ExperienceID（当前进程承载的体验，例如OpenWorld.Hub）。
	RegionID         string   // RegionID（部署区域ID）。
	ClusterID        string   // ClusterID（Kubernetes/Agones集群ID）。
	NodeID           string   // NodeID（承载节点ID）。
	WorldID          string   // WorldID（当前世界或地图逻辑ID）。
	PublicEndpoint   Endpoint // PublicEndpoint（客户端连接公网端点）。
	InternalEndpoint Endpoint // InternalEndpoint（服务间内部通信端点）。
	BuildVersion     string   // BuildVersion（GameServer构建版本）。
	ProtocolVersion  uint32   // ProtocolVersion（UE实时网络协议版本）。
	Capacity         int      // Capacity（最大可承载玩家数）。
}

// RosterPlayer（比赛名单玩家）描述被分配到MainArena的可信玩家与队伍关系。
type RosterPlayer struct {
	PlayerID    string // PlayerID（玩家ID）。
	TeamID      string // TeamID（竞技队伍ID）。
	CharacterID string // CharacterID（已锁定角色ID；尚未选定时可为空）。
}

// WorldAssignment（世界分配）描述OpenWorld/Village服务器运行和玩家跨服所需的权威目标。
type WorldAssignment struct {
	AssignmentID string // AssignmentID（世界分配唯一ID）。
	GameServerID string // GameServerID（目标服务器实例ID）。
	ServerRoleID string // ServerRoleID（OpenWorld或Village）。
	ExperienceID string // ExperienceID（OpenWorld.Hub/Main或Village.Main/Tutorial/Training体验）。
	WorldID      string // WorldID（目标世界、区域或实例逻辑ID）。
	RegionID     string // RegionID（目标部署区域）。
	Endpoint     string // Endpoint（客户端连接地址）。
}

// Validate（校验世界分配）确保Experience与ServerRole一致且目标信息完整。
func (a WorldAssignment) Validate() error {
	if a.AssignmentID == "" || a.GameServerID == "" || a.ExperienceID == "" || a.WorldID == "" || a.RegionID == "" || a.Endpoint == "" {
		return errors.New("世界分配的AssignmentID、GameServerID、ExperienceID、WorldID、RegionID和Endpoint不能为空")
	}
	roleID, ok := RoleForExperience(a.ExperienceID)
	if !ok || roleID == RoleMainArena {
		return errors.New("WORLD_EXPERIENCE_INVALID: ExperienceID不是可分配的常驻世界体验")
	}
	if roleID != a.ServerRoleID {
		return errors.New("WORLD_ROLE_MISMATCH: Experience与ServerRole不匹配")
	}
	return nil
}

// Assignment（主竞技场比赛分配）是MainArena Server启动比赛所需的权威运行上下文。
type Assignment struct {
	MatchID      string         // MatchID（比赛唯一ID）。
	ArenaModeID  string         // ArenaModeID（1v1至5v5竞技模式ID）。
	MapID        string         // MapID（主竞技场地图ID）。
	TeamSize     int            // TeamSize（每队玩家数）。
	TotalPlayers int            // TotalPlayers（比赛总玩家数）。
	GameServerID string         // GameServerID（承载该比赛的MainArena实例ID）。
	Roster       []RosterPlayer // Roster（完整权威玩家名单）。
}

// Validate（校验比赛分配）验证模式、人数、服务器绑定、玩家唯一性与每队人数。
func (a Assignment) Validate() error {
	expectedTeamSize, ok := TeamSizeForArenaMode(a.ArenaModeID)
	if !ok {
		return errors.New("MATCH_MODE_INVALID: ArenaModeID无效")
	}
	if a.MatchID == "" || a.GameServerID == "" || a.MapID == "" {
		return errors.New("比赛分配的MatchID、GameServerID和MapID不能为空")
	}
	if a.TeamSize != expectedTeamSize {
		return fmt.Errorf("TeamSize=%d与ArenaMode要求=%d不一致", a.TeamSize, expectedTeamSize)
	}
	expectedTotal := expectedTeamSize * 2
	if a.TotalPlayers != expectedTotal {
		return fmt.Errorf("TotalPlayers=%d与竞技模式要求=%d不一致", a.TotalPlayers, expectedTotal)
	}
	if len(a.Roster) != expectedTotal {
		return fmt.Errorf("Roster人数=%d与竞技模式要求=%d不一致", len(a.Roster), expectedTotal)
	}
	players := make(map[string]struct{}, len(a.Roster))
	teams := make(map[string]int, 2)
	for _, player := range a.Roster {
		if player.PlayerID == "" || player.TeamID == "" {
			return errors.New("Roster中的PlayerID和TeamID不能为空")
		}
		if _, exists := players[player.PlayerID]; exists {
			return errors.New("Roster中存在重复PlayerID")
		}
		players[player.PlayerID] = struct{}{}
		teams[player.TeamID]++
	}
	if len(teams) != 2 {
		return errors.New("竞技比赛必须且只能包含两个TeamID")
	}
	for teamID, count := range teams {
		if count != expectedTeamSize {
			return fmt.Errorf("Team %s人数=%d，期望=%d", teamID, count, expectedTeamSize)
		}
	}
	return nil
}

// ServerAssignment（统一服务器任务）供Dedicated Server查询自身运行上下文。
// WorldAssignment用于OpenWorld/Village；ArenaAssignment用于MainArena，二者只允许一个非空。
type ServerAssignment struct {
	AssignmentID string           // AssignmentID（服务器任务唯一ID）。
	ServerRoleID string           // ServerRoleID（服务器角色）。
	ExperienceID string           // ExperienceID（服务器体验）。
	WorldID      string           // WorldID（世界或地图逻辑ID）。
	RegionID     string           // RegionID（部署区域）。
	GameServerID string           // GameServerID（承载实例ID）。
	World        *WorldAssignment // World（常驻世界任务，可空）。
	Arena        *Assignment      // Arena（竞技比赛任务，可空）。
}

// Validate（校验统一服务器任务）保证角色、体验和具体任务类型一致。
func (a ServerAssignment) Validate() error {
	if a.AssignmentID == "" || a.GameServerID == "" || a.ServerRoleID == "" || a.ExperienceID == "" {
		return errors.New("ServerAssignment关键字段不能为空")
	}
	roleID, ok := RoleForExperience(a.ExperienceID)
	if !ok || roleID != a.ServerRoleID {
		return errors.New("SERVER_ASSIGNMENT_ROLE_MISMATCH: Experience与ServerRole不匹配")
	}
	if a.ServerRoleID == RoleMainArena {
		if a.Arena == nil || a.World != nil {
			return errors.New("MainArena任务必须且只能包含Arena Assignment")
		}
		return a.Arena.Validate()
	}
	if a.World == nil || a.Arena != nil {
		return errors.New("OpenWorld/Village任务必须且只能包含World Assignment")
	}
	return a.World.Validate()
}
