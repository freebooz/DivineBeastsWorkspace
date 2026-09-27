# ConfigurationAndRun（配置与运行）

Go后端启用Arena内部路由需要 `ARENA_BACKEND_ENABLED=1`、`DATABASE_URL（数据库连接地址）`、`GAMESERVERCONTROL_INTERNAL_TOKEN（服务器控制内部令牌）`、`ARENA_TRANSFER_TICKET_SECRET（转服票据签名密钥）`。MatchService（匹配服务）还需要 `MATCHSERVICE_INTERNAL_TOKEN（匹配服务内部令牌）` 与 `GAMESERVERCONTROL_BASE_URL（服务器控制服务地址）`。MainArena Dedicated Server（主竞技场专用服务器）自动读取Assignment时还必须提供 `GAME_SERVER_ID（游戏服务器实例编号）`。

Outbox→NATS JetStream（事务外发到流式消息）通过 `ARENA_MATCH_OUTBOX_ENABLED=1` 启动，并要求 `NATS_URL`；可选 `NATS_CREDS_FILE`。未启用发布器时结果仍可事务持久化，但不能声称 `Match.Completed` 已发送。