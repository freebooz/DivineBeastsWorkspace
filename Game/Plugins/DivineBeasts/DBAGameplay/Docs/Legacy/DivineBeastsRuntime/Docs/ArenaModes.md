# ArenaModes（竞技模式）

项目跨语言稳定ArenaMode ID固定为：

Arena.Mode.Duel1v1、Arena.Mode.Team2v2、Arena.Mode.Team3v3、Arena.Mode.Team4v4、Arena.Mode.Team5v5。

五个ID全部映射到GameServer.Role.MainArena和Experience.MainArena.Main。

DivineBeastsRuntime只维护项目允许值和项目上下文映射，不保存TeamSize、TotalPlayers、ScorePolicy、WinCondition等竞技规则。详细规则继续归GamePlatformArena/GamePlatformMobaData及未来DivineBeastsArena（项目竞技插件）。
