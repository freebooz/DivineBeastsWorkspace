# MigrationAndHandover（迁移与交接）

实施前 GamePlatformQuest 三模块均为空骨架，Backend quest/playerdata/migrations/contracts 也只有占位目录；因此本轮没有旧 Quest Progress 真源需要迁移，也没有第二套任务框架。

后端仍保持 GatewayService、IdentityService、PlayerDataService、MatchService、GameServerControlService 五个入口；没有新增 QuestService。PlayerDataService 从空入口扩展为 PostgreSQL quest repository + internal HTTP API。

DB Migration `0001_player_quest_progress`采用增量 create，不清空既有玩家数据；down migration 仅用于显式回滚这些新表。正式生产部署前仍需迁移框架、备份和兼容窗口审查。

后续 Inventory/Entitlement/Progression 接入时应消费 PlayerQuestCompleted/RewardSetId，而不是回到 Quest 中直接写长期资产。Online/Session 生产传输接入后，应实现异步 Persistence Port Adapter 并保持游戏线程无阻塞。
