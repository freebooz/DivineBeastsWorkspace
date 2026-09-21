// Package matchresult（比赛结果共享契约适配）定义MainArena向Backend提交的权威比赛结果DTO。
package matchresult

import "time"

// PlayerResult（玩家比赛结果）记录单个玩家的权威比赛统计。
type PlayerResult struct {
	PlayerID    string // PlayerID（玩家ID）。
	TeamID      string // TeamID（所属竞技队伍ID）。
	CharacterID string // CharacterID（使用角色ID）。
	Kills       uint32 // Kills（击杀数）。
	Deaths      uint32 // Deaths（死亡数）。
	Assists     uint32 // Assists（助攻数）。
	Score       uint32 // Score（服务端计算的比赛评分）。
}

// TeamResult（队伍比赛结果）记录一支队伍的权威结算数据。
type TeamResult struct {
	TeamID string // TeamID（竞技队伍ID）。
	Won    bool   // Won（是否获胜）。
	Score  uint32 // Score（队伍比分）。
}

// Result（权威比赛结果）只能由可信MainArena Dedicated Server提交。
type Result struct {
	MatchID       string         // MatchID（比赛唯一ID，用作幂等结算键）。
	ArenaModeID   string         // ArenaModeID（1v1至5v5竞技模式ID）。
	GameServerID  string         // GameServerID（承载比赛的MainArena实例ID）。
	StartedAt     time.Time      // StartedAt（比赛开始时间）。
	EndedAt       time.Time      // EndedAt（比赛结束时间）。
	WinningTeamID string         // WinningTeamID（获胜队伍ID，平局时可为空）。
	Teams         []TeamResult   // Teams（双方队伍结果）。
	Players       []PlayerResult // Players（所有玩家结果）。
}
