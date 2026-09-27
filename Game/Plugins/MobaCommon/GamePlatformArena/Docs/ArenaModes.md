# ArenaModes（竞技模式）

正式模式固定为：`Arena.Mode.Duel1v1（1v1）`、`Arena.Mode.Team2v2（2v2）`、`Arena.Mode.Team3v3（3v3）`、`Arena.Mode.Team4v4（4v4）`、`Arena.Mode.Team5v5（5v5）`。

五种模式TeamCount均为2，对应TotalPlayers为2/4/6/8/10。它们全部绑定 `GameServer.Role.MainArena（主竞技场服务器角色）`，不存在MainArena1v1～5v5独立Target或Binary。