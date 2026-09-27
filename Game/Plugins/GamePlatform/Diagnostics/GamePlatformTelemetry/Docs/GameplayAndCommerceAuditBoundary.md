# GameplayAndCommerceAuditBoundary（玩法与商业审计边界）

Telemetry 是可采样、可降级、可丢弃的观测数据，不是 Gameplay Authority（玩法权威）、Persistent Business Authority（持久业务权威）或 Business Audit（业务审计）。

Inventory Grant、Entitlement Grant/Revoke、Progression XP、Quest Completion、Commerce Payment/Refund、Currency Debit/Credit、Admin Command 必须继续依赖业务数据库 + OperationId + Outbox/Audit。

Telemetry 可以镜像这些动作的 latency/result/error class（时延/结果/错误类别），但丢失遥测不得影响奖励、支付、背包、任务或登录状态。