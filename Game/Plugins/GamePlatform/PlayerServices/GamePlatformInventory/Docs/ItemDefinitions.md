# ItemDefinitions（物品定义）

UE Client 定义 `UGamePlatformInventoryItemDefinition（客户端物品显示定义）`，字段包括 ItemDefinitionId、DisplayNameKey、DescriptionKey、Category、IconId、MaxStackSize 和 ClientTags。它只服务 UI/内容分类，绝不能成为服务端数量、堆叠、消耗或授予规则的权威来源。

2026-10-14已实现 Go 端 `LoadItemPolicyCatalog`（可信规则目录导入）、`GrantTrusted`（可信授予）、`ConsumeTrusted`（可信消耗）和 `Backend/config/inventory_item_policies_v1.json`（SSLM后端只读物品规则），连同PostgreSQL事务与Outbox；正式服仍必须有真实玩家会话授权、迁移执行及业务奖励事件证据。`ItemInstance.max_stack_size`（实例权威堆叠上限）随后端快照发给客户端仅供展示，客户端显示定义中的同名值不参与判定。后端测试通过不意味着实际专服拾取已闭环。

项目表现资源不应硬塞进平台层。平台 Snapshot/ViewModel 只暴露 ItemDefinitionId / DisplayDefinitionHandle；神兽联盟项目层应通过 Asset Manager（资产管理器）把稳定 DefinitionId 解析到项目 DataAsset，再通过 `TSoftObjectPtr（软对象指针） + StreamableManager（流式加载管理器）`异步加载图标、网格和材质。平台层不得硬引用 DivineBeasts 资源。

`UGamePlatformInventoryItemDefinition`（平台中立物品显示定义）仍属于 `UPrimaryDataAsset`。2026-10-14已用Monolith在 `/DBAUIPack_Core/UI/Inventory/Definitions`（项目层定义目录）真实创建并保存8份数据资产，按项目配置 `DefaultGame.ini` 的 AssetManager（资产管理器）扫描对应的 PrimaryAssetType（主资产类型）。另已导入8份512×512 RGBA图标 `Texture2D`（纹理），并加入项目层背包C++异步软加载。实际运行时扫描与客户端/服务器Cook分包、真实物品使用效果仍待最终验证。
