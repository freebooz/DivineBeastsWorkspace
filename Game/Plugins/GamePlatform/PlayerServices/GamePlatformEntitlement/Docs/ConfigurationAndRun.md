# ConfigurationAndRun（配置与运行）

PlayerDataService 至少需要 DATABASE_URL（数据库地址）、PLAYERDATA_INTERNAL_TOKEN（内部令牌）、GATEWAY_CALLER_ID（网关调用方编号）、INVENTORY_ITEM_POLICY_FILE（背包规则文件）和 ENTITLEMENT_DEFINITION_FILE（权益定义文件）。

Entitlement 开发目录：`Backend/configs/games/divinebeasts.entitlement.development.json`。Quest 权益奖励映射环境变量样例为 `QUEST_ENTITLEMENT_REWARD_FILE`；当前消息消费者尚未正式装配订阅器，所以该文件不是 PlayerData HTTP 启动的硬依赖。

Gateway 仍要求正式 PlayerAuthenticator 注入；默认 Run 会明确失败而不是使用不安全的自报 PlayerId。