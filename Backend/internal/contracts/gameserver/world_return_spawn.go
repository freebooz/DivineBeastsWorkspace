package gameserver

import "math"

// WorldReturnSpawn（可信返程落点）只在目标服务器消费有效返程票后交付。
// 坐标是角色胶囊中心的UE世界厘米，Yaw为角度；最终地图与碰撞安全由UE目标服务器复核。
type WorldReturnSpawn struct {
	CharacterID string  `json:"characterId"` // 源服务器当前已初始化的角色身份。
	MapPackage  string  `json:"mapPackage"`  // 源服务器的正式关卡包名。
	Xcm         float64 `json:"xCm"`         // 世界X坐标，厘米。
	Ycm         float64 `json:"yCm"`         // 世界Y坐标，厘米。
	Zcm         float64 `json:"zCm"`         // 世界Z坐标，厘米。
	YawDegrees  float64 `json:"yawDegrees"`  // 角色朝向，标准化至[-180, 180]度。
}

// IsValid对无效或越界投影失败关闭；合法数据仍须经过目标UE世界碰撞和地面检查。
func (s WorldReturnSpawn) IsValid() bool {
	return s.CharacterID != "" && len(s.CharacterID) <= 1024 && len(s.MapPackage) > 1 && len(s.MapPackage) <= 512 && s.MapPackage[0] == '/' &&
		math.Abs(s.Xcm) <= 100000000 && math.Abs(s.Ycm) <= 100000000 && math.Abs(s.Zcm) <= 100000000 && math.Abs(s.YawDegrees) <= 180
}
