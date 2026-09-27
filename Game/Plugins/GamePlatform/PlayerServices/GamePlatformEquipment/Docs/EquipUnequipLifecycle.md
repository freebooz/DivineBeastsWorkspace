# EquipUnequipLifecycle（装备/卸下生命周期）

Equip：UE Server 预检请求和本地 Slot → PlayerData 事务校验 InventoryRevision/Item 所有权/Slot → EquipmentRevision++ → Operation 幂等记录 + Outbox → 返回 Snapshot → UE Server 应用 GAS → 成功后发布复制状态。

Unequip：后端只删除 Equipment Slot 引用并提升 EquipmentRevision，不删除 Inventory ItemInstance。UE Server 只撤销该槽自己记录的 GrantHandle。

若持久化成功而 Runtime Apply（运行时应用）失败，服务器进入 RuntimeApplyFailed/Error 状态，不发布伪成功，并要求 Reconcile（对账）或后续补偿。