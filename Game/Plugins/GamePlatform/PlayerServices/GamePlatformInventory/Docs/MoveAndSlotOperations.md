# MoveAndSlotOperations（移动与槽位操作）

Move 请求包含 OperationId、ExpectedRevision、ItemInstanceId、TargetContainerId、TargetSlotIndex。Repository（仓储）锁定玩家 InventoryState（背包状态）并校验 Revision。

目标空槽时直接移动；目标已有 Item 时执行原子 Swap（交换）。`uq_player_inventory_slot（玩家背包槽位唯一约束）`被定义为 `DEFERRABLE（可延迟约束）`，事务内临时更新顺序不会因为两个合法槽交换而触发中间态唯一冲突。

Split（拆分）要求 `0 < SplitQuantity < SourceQuantity`、源定义可堆叠、目标槽为空；失败整体回滚。Merge（合并）要求同玩家、同 ItemDefinitionId、可堆叠且不超过 MaxStack；允许部分合并并返回 MovedQuantity/RemainingQuantity，不静默丢数量。
