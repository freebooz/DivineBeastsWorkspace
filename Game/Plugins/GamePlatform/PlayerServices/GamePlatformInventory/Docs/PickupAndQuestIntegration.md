# PickupAndQuestIntegration（拾取与任务集成）

Quest Reward（任务奖励）由 `PlayerQuestCompleted（玩家任务完成事件）`驱动后端 `QuestRewardHandler（任务奖励处理器）`。稳定 OperationId 为 `quest:<CompletionId>:<RewardEntryId>`；同一完成事件重复消费不会重复 Grant。全部 Inventory Reward（背包奖励）成功后才调用 `MarkRewardGranted（标记奖励已发放）`。

Quest Completion Outbox（任务完成事务外发）新增 reward_set_id，并仍由 Quest 事务写入；Quest UE 插件不依赖 InventoryClient，也不直接写 Inventory 表。当前真实消息订阅器/NATS（消息总线）尚未实现，因此运行消费状态仍为未执行。

Interaction Pickup（交互拾取）不再复用会提前 Consumed（已消耗）的旧 Consume Commit。`External（外部结果提交）`只确认交互事实；DBAServer 的 Pickup Adapter（拾取适配器）先 Reservation（保留），异步 Grant 成功才 FinalizeExternalConsume（最终消耗），明确失败恢复，OutcomeUnknown（结果未知）按同一 OperationId 查询。

当前 Operation 查询最多自动重试 5 次；仍未知时世界对象保持 Reservation，防止重复 Grant。若服务器/玩家适配器在未知状态下退出，真实跨重启恢复仍待运行验证和后续世界持久化能力。
