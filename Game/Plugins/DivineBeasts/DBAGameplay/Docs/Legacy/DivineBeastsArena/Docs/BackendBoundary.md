# BackendBoundary（后端边界）

新增Go微服务：无。
新增Go业务接口：默认无。

复用现有：
- GatewayService。
- MatchService。
- GameServerControlService。
- Party/Matchmaking。
- ArenaConfig五模式。
- CreateArenaMatch。
- Assignment。
- TransferTicket。
- MatchResult。
- PostgreSQL持久化。
- Match.Completed Outbox。

本轮只增量补全现有Match域的CharacterId身份链和Assignment项目Revision字段；没有创建ArenaService。

长期MMR/XP/Inventory/Entitlement/Currency奖励不由Arena Server逐项调用PlayerData。
