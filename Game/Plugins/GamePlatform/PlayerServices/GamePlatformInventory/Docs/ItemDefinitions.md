# ItemDefinitions（物品定义）

UE Client（客户端）定义 `UGamePlatformInventoryItemDefinition（客户端物品显示定义）`，字段包括 ItemDefinitionId、DisplayNameKey、DescriptionKey、Category、IconId、MaxStackSize 和 ClientTags；这些字段用于显示/分类，不是服务器权威规则。

Go 后端使用 `PolicyCatalog（物品策略目录）`验证 ItemDefinitionId、MaxStackSize、Stackable、Unique、Consumable。运行时可通过 `INVENTORY_ITEM_POLICY_FILE（物品策略文件路径）`加载受控 JSON；缺少真实生产策略时未知物品 Grant 明确返回 DefinitionNotFound（定义不存在）。

`Shared/Contracts/Games/DivineBeasts/Schemas/item-catalog.v1.schema.json（神兽联盟物品目录结构）`定义项目级目录格式。当前仅提供 `Development.*（开发测试）`示例配置，不冒充正式生产物品。
