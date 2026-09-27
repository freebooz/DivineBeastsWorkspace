# Troubleshooting（故障排查）

PlayerData 启动失败先检查 DATABASE_URL、INVENTORY_ITEM_POLICY_FILE、ENTITLEMENT_DEFINITION_FILE 与 EQUIPMENT_DEFINITION_FILE。

Equip 返回 InventoryRevisionConflict/EquipmentRevisionConflict 时客户端/服务器应刷新对应 Snapshot，不盲目覆盖。ItemEquipped 表示必须先 Unequip 后再 Consume/Delete。

GameplayGrantFailed/RuntimeApplyFailed 优先检查 ASC 是否存在、Equipment Definition 是否与后端一致，以及 AbilitySet Resolver 是否真实实现。VisualLoadFailed/SocketMissing 仅影响表现。