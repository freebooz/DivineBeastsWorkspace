# ProjectArenaModel（项目竞技模型）

固定模式：
Arena.Mode.Duel1v1、Team2v2、Team3v3、Team4v4、Team5v5。

固定结构：
TeamCount=2。
TeamSize=1/2/3/4/5。
TotalPlayers=2/4/6/8/10。
ServerRole=GameServer.Role.MainArena。
Experience=Experience.MainArena.Main。

全部模式共用一个DivineBeastsArenaServer.Target.cs和同一Dedicated Server Binary。不存在MainArena1v1~5v5独立Role/Target/Binary。

模式差异由ArenaModeId + Mode Definition + Project Rule配置驱动。
