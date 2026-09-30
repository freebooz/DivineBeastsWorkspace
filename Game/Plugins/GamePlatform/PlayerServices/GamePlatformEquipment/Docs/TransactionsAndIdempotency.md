# TransactionsAndIdempotency（事务与幂等）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

`0004_player_equipment`建立 equipment state、slots 和 operations。Equip 先锁 Inventory State 与 ItemInstance，再锁 Equipment State；校验两个 Revision 后更新 Slot。

player_equipment_operations 持久保存 OperationId 结果，并使用 pg_advisory_xact_lock（事务咨询锁）避免同 Operation 并发重复副作用。同一个 OperationId 跨 Equip/Unequip 类型复用会拒绝。

EquipmentRevision 更新、Slot 更新、Operation Result 和 PlayerEquipmentChanged Outbox 在同一事务 Commit。
