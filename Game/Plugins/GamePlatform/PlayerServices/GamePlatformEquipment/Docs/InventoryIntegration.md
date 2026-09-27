# InventoryIntegration（背包集成）

Equipment 不复制 Inventory Item，也不修改 Quantity。Equip 事务先锁 player_inventory_state 并校验 ExpectedInventoryRevision，再锁目标 ItemInstance，确认玩家所有权与 active 状态。

player_equipment_slots 通过外键引用 player_inventory_items，并使用 ON DELETE RESTRICT（禁止删除被引用物品）。

Inventory Consume 增加 ErrItemEquipped（物品已装备）保护；被 Equipment 引用的 ItemInstance 必须先 Unequip。Inventory 与 Equipment 通过数据库/application 编排协作，而不是互相导入领域实现。