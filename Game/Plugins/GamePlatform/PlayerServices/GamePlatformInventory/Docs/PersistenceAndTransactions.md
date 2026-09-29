# PersistenceAndTransactions（持久化与事务）

数据库真源为 `Backend/migrations/000008_player_inventory.sql`。它创建 `player_inventories（背包聚合根）`、`player_inventory_containers（容器）`、`player_inventory_items（物品实例）`、`player_inventory_quickbar（快捷栏）`和 `player_inventory_operations（幂等操作结果）`，并通过外键挂到现有 `player_profiles（玩家资料）`。

生产仓储位于 `Backend/internal/platform/database/postgres/inventory_repository.go`。每个 Mutation（变更）开启 PostgreSQL 事务，按 playerId+OperationId 获取 `pg_advisory_xact_lock（事务咨询锁）`，再 `SELECT ... FOR UPDATE` 锁定对应 Inventory 聚合根。发现同 OperationId 时先比较规范化请求：同内容返回已持久结果并标记 Duplicate，不同内容返回 `IDEMPOTENCY_CONFLICT`。

新操作必须满足 `ExpectedRevision == inventory_revision`。操作成功后物品/快捷栏变化、InventoryRevision +1、完整 MutationResult JSON 和 Operation 元数据在同一事务提交；事务中途失败会整体回滚。提交阶段返回错误时客户端必须把结果视为 OutcomeUnknown，并使用原 OperationId 查询/恢复，而不能生成新键。

Move/Swap 依赖 `player_id + container_id + slot_index` 的可延迟唯一约束，允许同一事务内交换槽位；Split/Merge 与 Quickbar 更新也在同一事务。当前正式事务范围只覆盖 Move/Swap、Split/Merge、Quickbar；Grant/Consume 尚未实现，不能写成已纳入事务。
