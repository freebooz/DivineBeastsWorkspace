# EquipUnequipLifecycle（装备/卸下生命周期）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

Equip：UE Server 预检请求和本地 Slot → PlayerData 事务校验 InventoryRevision/Item 所有权/Slot → EquipmentRevision++ → Operation 幂等记录 + Outbox → 返回 Snapshot → UE Server 应用 GAS → 成功后发布复制状态。

Unequip：后端只删除 Equipment Slot 引用并提升 EquipmentRevision，不删除 Inventory ItemInstance。UE Server 只撤销该槽自己记录的 GrantHandle。

若持久化成功而 Runtime Apply（运行时应用）失败，服务器进入 RuntimeApplyFailed/Error 状态，不发布伪成功，并要求 Reconcile（对账）或后续补偿。
