# QuestRewardIntegration（任务奖励集成）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

PlayerQuestCompleted（玩家任务完成事件）已有 CompletionId 与 RewardSetId。Entitlement QuestRewardHandler 使用稳定 OperationId：`quest:<CompletionId>:<RewardEntryId>`，重复消息不会重复 Grant。

开发奖励映射位于 `Backend/configs/games/divinebeasts.quest-entitlement-rewards.development.json`，支持永久权益和 temporary_seconds（临时秒数）。

`QuestEntitlementRewardConsumer（任务权益奖励消费者）`已提供可靠处理边界，但正式 NATS/消息总线订阅器尚未落地，因此真实消息消费未执行。混合 Inventory + Entitlement 奖励的统一“全部完成后更新 Quest reward_status”协调器也尚未实现，本轮不伪造跨领域完成状态。
