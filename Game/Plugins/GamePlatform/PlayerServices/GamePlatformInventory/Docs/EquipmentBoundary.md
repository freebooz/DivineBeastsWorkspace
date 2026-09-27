# EquipmentBoundary（装备边界）

本轮只保证 Inventory 能持久化 Unique（唯一）物品实例，并通过 ItemDefinitionId/ItemInstanceId 为未来 Equipment（装备系统）提供拥有关系查询基础。

没有实现 Equip/Unequip（装备/卸下）、Equipment Slot（装备槽）、Attribute（属性）、Mesh（模型挂接）或 GAS Effect（玩法能力效果）。`GamePlatformEquipment（游戏平台装备插件）`仍是后续插件，Inventory 不反向实现其职责。
