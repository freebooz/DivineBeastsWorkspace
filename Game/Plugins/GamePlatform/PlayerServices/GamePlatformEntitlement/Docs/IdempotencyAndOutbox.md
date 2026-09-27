# IdempotencyAndOutbox（幂等与事务外发）

player_entitlement_operations 以 game_id + player_id + operation_id 为主键，保存逻辑响应。重复相同类型 Operation 返回原结果并标 Duplicate；同一个 OperationId 被跨 Grant/Revoke 类型复用时拒绝。

Repository 使用 pg_advisory_xact_lock（PostgreSQL事务咨询锁）保护同 OperationId 并发。

EntitlementGranted 与 EntitlementRevoked 通过确定性 EventId 写入既有 gameplatform_outbox，与权益 Revision/Grant/Revoke 同事务。正式 Publisher（发布器）运行仍取决于现有 Outbox Dispatcher 的消息总线注入。