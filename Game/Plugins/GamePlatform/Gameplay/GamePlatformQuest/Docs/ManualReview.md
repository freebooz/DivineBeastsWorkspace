# ManualReview（人工审查）

状态：待人工审查。AI 未代签。

人工审查至少覆盖：Definition/Prerequisite循环、Accept/Abandon、Interaction/Combat/Region Objective、同 EventId 去重、多Quest同事件、Revision冲突、普通批量 Flush、Completion幂等、Outbox同事务、NoReward/UnsupportedReward、服务/Server重启、跨服前Flush、Late Join、双Client隔离和安全写接口。

还必须审查 PostgreSQL Migration 前向执行/索引/约束/回滚、Outbox Dispatcher lease与重试、服务身份、Go unit/integration tests、UE Automation、Editor/Client/Server Build、Client/Server Cook 和 Development Definition 发行剥离。

当前 UE、Go、PostgreSQL 和真实 UE→Go Transport 环境不完整，上述运行项不得从源码存在推断为通过。
