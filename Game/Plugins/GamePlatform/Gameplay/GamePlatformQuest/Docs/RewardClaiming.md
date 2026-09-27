# RewardClaiming（奖励领取）

Quest Completed（任务完成）与 Reward Granted（奖励已发放）严格分离。Snapshot 同时记录 RewardSetId、RewardClaimId 和 RewardStatus。没有 RewardSet 的 Development Quest 使用 NoReward；有 RewardSet 但当前没有正式 Inventory/Entitlement/Progression 消费者时使用 UnsupportedRewardType 或 PendingExternalReward，不能宣称奖励已到账。

Completion 时只有 RewardSetId 非空才生成 RewardClaimId；NoReward 任务不制造无意义的 RewardClaimId。Quest 插件和 PlayerData 代码均不直接修改 Gold（金币）或 PlayerProfile 余额字段。

`PlayerQuestCompleted（玩家任务完成事件）`Outbox payload 包含稳定 CompletionId，并在 RewardClaimId 存在时才写该字段。未来奖励领域消费者应按 EventId/CompletionId/RewardClaimId 幂等处理。

当前没有正式奖励消费者，也没有 NATS JetStream Publisher（NATS消息发布器）运行证据，因此 RewardStatus=Granted 的真实闭环不在本轮验收范围。
