# TransactionsAndIdempotency（事务与幂等）

`0004_player_equipment`建立 equipment state、slots 和 operations。Equip 先锁 Inventory State 与 ItemInstance，再锁 Equipment State；校验两个 Revision 后更新 Slot。

player_equipment_operations 持久保存 OperationId 结果，并使用 pg_advisory_xact_lock（事务咨询锁）避免同 Operation 并发重复副作用。同一个 OperationId 跨 Equip/Unequip 类型复用会拒绝。

EquipmentRevision 更新、Slot 更新、Operation Result 和 PlayerEquipmentChanged Outbox 在同一事务 Commit。