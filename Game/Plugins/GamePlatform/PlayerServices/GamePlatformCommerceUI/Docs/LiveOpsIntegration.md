# LiveOpsIntegration（运营集成）

Commerce Offer（商城报价）可引用 LiveOpsEventId 与 AudienceRuleSetId（运营活动编号/人群规则集）。

客户端 LiveOps 缓存只用于促销标签和倒计时显示。创建 PurchaseIntent 时 PlayerData Commerce Application 必须重新读取 Published LiveOps Catalog（已发布运营目录），使用 ServerTime（服务器时间）验证 Event Active（活动有效），并通过 LiveOps Application 执行 typed Eligibility（类型化资格）。

活动刚结束时，新 Intent 会被拒绝；已创建 Intent 是否可继续由 Intent TTL 和保存的 Price/Reward Snapshot 约束。