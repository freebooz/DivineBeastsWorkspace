# InventoryModel（背包模型）

`Snapshot（背包快照）`由 `InventoryRevision（背包修订号）`、`Items（物品实例）`和 `Quickbar（快捷栏）`组成。`ItemInstance（物品实例）`至少包含 ItemInstanceId、ItemDefinitionId、Quantity、ContainerId、SlotIndex、Revision、InstanceState、CreatedAt、UpdatedAt。

`ItemInstanceId（物品实例编号）`是持久实例身份；`ItemDefinitionId（物品定义编号）`是静态规则身份。Unique（唯一实例型）物品 Quantity 固定为 1；Stackable（可堆叠）物品受服务器 ItemPolicy（物品策略）的 MaxStackSize（最大堆叠）约束。

第一版只有 `main（主背包容器）`容器规则，SlotIndex（槽位编号）为非负整数。未来扩展仓库/公会仓库不属于本轮。
