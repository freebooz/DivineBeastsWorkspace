# BackendIntegration（后端集成）

> 2026-09-30状态校正：本页保留旧方案/历史证据，旧DBAServer具体Quest HTTP/事件适配与后端交付宣称未在当前项目文件清单确认，不能作为现行验收。当前行为以[本轮整改说明](DesignRemediation-2026-09-30.md)、README与真实源码为准；本轮未执行数据库/Outbox/网络联调。

本轮没有新增 QuestService（独立任务微服务），保持既有五个 Go 服务入口。任务跨会话能力扩展到 `Backend/gameplatform/quest（平台任务后端领域）`、`Backend/gameplatform/internal/persistence/questrepository（PostgreSQL任务仓储）`和既有 `PlayerDataService（玩家数据服务）`。

Shared Contracts（共享契约）新增 `playerdata-quest.v1.yaml`，定义 GetQuestSnapshot、AcceptQuest、SaveQuestProgress、CompleteQuest、AbandonQuest 的内部 PlayerData 语义；另有 `player-quest-completed.v1.schema.json`定义 Outbox 完成事件。

PlayerDataService 内部 HTTP 写路由使用内部 Bearer Token、X-Game-Server-Id 和 X-Bound-Player-Id 做当前最小受控服务边界，并以路径 gameId/playerId 覆盖 Body 身份。正式 Online/Session 服务身份尚未形成，后续应替换/加强此最小 Header 模型。

项目层 DBAServer 已实现 `FDBAQuestPlayerDataPersistence（神兽联盟任务PlayerData持久化适配器）`：使用 UE 异步 `FHttpModule（HTTP模块）`访问 Shared Contract 中的 PlayerData 内部 Quest API，并将完成回调切回 GameThread（游戏线程）。QuestServer 仍只依赖 `IGamePlatformQuestPersistencePort（任务持久化端口）`，不直接依赖 HTTP 或凭据。

适配器要求服务端环境提供 `PLAYERDATA_BASE_URL（玩家数据服务地址）`、`PLAYERDATA_INTERNAL_TOKEN（内部令牌）`、`GAME_SERVER_INSTANCE_ID（游戏服务器实例编号）`、`GAME_ID（游戏编号）`；缺失时明确拒绝注册任务 Runtime，不降级成“内存持久化成功”。

PlayerDataService 的 Outbox Dispatcher 已形成 `RunWithPublisher（带发布器运行）`装配边界；只有注入真实 Publisher（发布器）才启动 Outbox 派发。当前仓库没有正式 NATS/JetStream Publisher，因此真实消息发布仍为未执行，数据库 Outbox 也不会被伪标已发布。
