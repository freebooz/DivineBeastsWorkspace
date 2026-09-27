# SlotModelAndRules（槽位模型与规则）

SlotId 使用稳定字符串/Name（名称）标识。后端 Equipment Catalog（装备目录）定义每个 EquipmentDefinition 允许的 SlotIds，并通过 CompatibleItemDefinitionId 将 Inventory ItemDefinition 映射到装备定义。

数据库以 game/player/character/slot 唯一保存长期槽位；同一 ItemInstance 另有玩家级唯一约束，第一版禁止同一个物品实例同时占多个槽或多个角色。