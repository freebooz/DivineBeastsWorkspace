# QuestRewardIntegration（任务奖励集成）

PlayerQuestCompleted（玩家任务完成事件）已有 CompletionId 与 RewardSetId。Entitlement QuestRewardHandler 使用稳定 OperationId：`quest:<CompletionId>:<RewardEntryId>`，重复消息不会重复 Grant。

开发奖励映射位于 `Backend/configs/games/divinebeasts.quest-entitlement-rewards.development.json`，支持永久权益和 temporary_seconds（临时秒数）。

`QuestEntitlementRewardConsumer（任务权益奖励消费者）`已提供可靠处理边界，但正式 NATS/消息总线订阅器尚未落地，因此真实消息消费未执行。混合 Inventory + Entitlement 奖励的统一“全部完成后更新 Quest reward_status”协调器也尚未实现，本轮不伪造跨领域完成状态。