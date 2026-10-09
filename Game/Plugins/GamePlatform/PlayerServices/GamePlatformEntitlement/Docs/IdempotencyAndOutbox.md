# IdempotencyAndOutbox（幂等与事务外发）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

player_entitlement_operations 以 game_id + player_id + operation_id 为主键，保存逻辑响应。重复相同类型 Operation 返回原结果并标 Duplicate；同一个 OperationId 被跨 Grant/Revoke 类型复用时拒绝。

Repository 使用 pg_advisory_xact_lock（PostgreSQL事务咨询锁）保护同 OperationId 并发。

EntitlementGranted 与 EntitlementRevoked 通过确定性 EventId 写入既有 gameplatform_outbox，与权益 Revision/Grant/Revoke 同事务。正式 Publisher（发布器）运行仍取决于现有 Outbox Dispatcher 的消息总线注入。
