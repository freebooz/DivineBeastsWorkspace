# ConfigurationAndRun（配置与运行）

PlayerDataService 至少需要 `DATABASE_URL（数据库地址）`、`PLAYERDATA_INTERNAL_TOKEN（内部令牌）`和 `INVENTORY_ITEM_POLICY_FILE（背包物品策略文件）`。Inventory 已启用时 ItemPolicy 是长期背包权威规则真源之一，未配置将直接拒绝启动，而不是以空策略目录伪装成可用服务。

GatewayService 带正式认证器运行时需要 PLAYERDATA_BASE_URL、PLAYERDATA_INTERNAL_TOKEN、GAME_ID、GATEWAY_CALLER_ID，可选 GATEWAY_LISTEN_ADDR。默认入口不会生成伪认证器。

DBAServer 的长期 Grant 使用 PLAYERDATA_BASE_URL、PLAYERDATA_INTERNAL_TOKEN、GAME_SERVER_INSTANCE_ID、GAME_ID。UE Client Gateway Transport 的 URL/AccessToken 必须由认证后的 Online/Identity 层注入。

当前 Development ItemPolicy/RewardMap 只用于开发验证，不是正式生产配置。
