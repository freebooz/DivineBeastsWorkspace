# ConfigurationAndRun（配置与运行）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

PlayerDataService 至少需要 DATABASE_URL（数据库地址）、PLAYERDATA_INTERNAL_TOKEN（内部令牌）、GATEWAY_CALLER_ID（网关调用方编号）、INVENTORY_ITEM_POLICY_FILE（背包规则文件）和 ENTITLEMENT_DEFINITION_FILE（权益定义文件）。

Entitlement 开发目录：`Backend/configs/games/divinebeasts.entitlement.development.json`。Quest 权益奖励映射环境变量样例为 `QUEST_ENTITLEMENT_REWARD_FILE`；当前消息消费者尚未正式装配订阅器，所以该文件不是 PlayerData HTTP 启动的硬依赖。

Gateway 仍要求正式 PlayerAuthenticator 注入；默认 Run 会明确失败而不是使用不安全的自报 PlayerId。
