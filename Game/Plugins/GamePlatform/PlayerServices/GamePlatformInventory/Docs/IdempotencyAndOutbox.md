# IdempotencyAndOutbox（幂等与事务外发）

`player_inventory_operations（背包操作表）`以 game_id/player_id/operation_id 为主键保存 operation_type 和第一次成功响应。相同 OperationId + 相同类型重放返回原结果；相同 OperationId 被换成另一操作类型时返回 DuplicateOperation（重复操作冲突），不能把不同副作用伪装为同一次操作。

Grant/Consume 与 `InventoryItemGranted（物品已授予）`/`InventoryItemConsumed（物品已消耗）` Outbox 事件同事务提交。EventId 由 OperationId + EventType 确定性派生，重试不会产生不同逻辑事件。

Quest Reward（任务奖励）使用 `quest:<CompletionId>:<RewardEntryId>`作为 Grant OperationId；Interaction Pickup（交互拾取）使用 `interaction:<InteractionRequestId>:<RewardEntryId>`。

真实 Outbox Publisher（事务外发发布器）/NATS JetStream（消息流）尚未配置，发布运行状态未执行。
