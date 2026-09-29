# ConfigurationAndRun（配置与运行）

Inventory 不再维护独立 Gateway URL 或 AccessToken 配置。UE `GamePlatformInventoryClient`自动复用 `GamePlatformOnlineClient`的 ServiceOrigin、认证会话、刷新、超时与取消策略；背包 Transport 不读取 Token，也没有 `INVENTORY_GATEWAY_URL` 等旁路配置。

本地非 `productiondeps` 模式由 PlayerDataService 自动装配 `inventory.NewMemoryRepository()`，用于多进程/自动化联调，不作为生产真源。

生产 HTTP 模式的 PlayerDataService 复用现有 `POSTGRES_DSN` 和 PostgreSQL 连接池；必须先应用 `Backend/migrations/000008_player_inventory.sql`。Gateway 继续通过现有 `PLAYER_DATA_SERVICE_URL` 调用 PlayerData internal HTTP。Inventory 没有独立数据库连接、独立服务端口或独立内部 Token 配置。

生产 gRPC 模式已在 PlayerData Composition 中装配同一 PostgreSQL Inventory Repository，并更新内部 Proto 协议源；但新增 RPC 的生成绑定/Adapter 尚未 Codegen，因此现阶段不能把 gRPC Inventory 链作为可运行能力验收。

旧文档中的 `DATABASE_URL`、`PLAYERDATA_INTERNAL_TOKEN`、`INVENTORY_ITEM_POLICY_FILE`、`PLAYERDATA_BASE_URL`、`GAME_ID` 等 Inventory 专用要求与当前真实装配不一致，已移除。可信 Dedicated Server Grant/Consume 尚未实现，因此也不存在可投入运行的 Grant 专用环境变量。

当前 Runner 缺少 Go/Protobuf/OpenAPI 代码生成工具；恢复工具链后必须执行 Shared Codegen、内部 Proto 生成、Go Test 和生产构建，再更新本文件的验证状态。
