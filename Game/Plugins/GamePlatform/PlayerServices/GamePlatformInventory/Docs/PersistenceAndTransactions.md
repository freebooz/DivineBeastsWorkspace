# PersistenceAndTransactions（持久化与事务）

`0002_player_inventory.up.sql（背包数据库迁移）`增量创建 `player_inventory_state（玩家背包状态）`、`player_inventory_items（玩家物品实例）`、`player_quickbar_slots（快捷栏槽位）`和 `player_inventory_operations（幂等操作）`；没有修改 Quest 的 `0001`迁移。

所有 Mutation（变更）先按 `game/player/OperationId`获取 `pg_advisory_xact_lock（事务咨询锁）`，再锁 `player_inventory_state FOR UPDATE（背包状态行锁）`。客户端 Mutation 校验 ExpectedRevision（预期修订号），成功后 InventoryRevision 单调 +1。

Move/Swap 使用可延迟 Slot 唯一约束；Split/Merge、Grant/Consume、Quickbar 与 InventoryRevision 更新都在同一数据库事务。任何中途错误会回滚整个事务。
