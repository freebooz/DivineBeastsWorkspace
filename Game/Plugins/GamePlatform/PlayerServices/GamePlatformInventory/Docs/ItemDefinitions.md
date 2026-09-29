# ItemDefinitions（物品定义）

UE Client 定义 `UGamePlatformInventoryItemDefinition（客户端物品显示定义）`，字段包括 ItemDefinitionId、DisplayNameKey、DescriptionKey、Category、IconId、MaxStackSize 和 ClientTags。它只服务 UI/内容分类，绝不能成为服务端数量、堆叠、消耗或授予规则的权威来源。

本轮后端没有实现旧文档所写的 `PolicyCatalog`、`INVENTORY_ITEM_POLICY_FILE` 或正式 Grant/Consume 目录；这些名称此前只有文档声明，没有真实代码，现已撤销“已实现”表述。当前 PostgreSQL ItemInstance 会保存 `max_stack_size`，用于已有实例 Merge 的权威上限；该权威值已随 Gateway Snapshot 传输到 UE `FGamePlatformInventoryItemInstance（背包物品实例）`和 ViewModel，客户端只用于展示/预判，所有写操作仍由服务端重新验证。生产 Grant 如何从正式物品目录确定并冻结该值仍属于后续服务端授予设计。

项目表现资源不应硬塞进平台层。平台 Snapshot/ViewModel 只暴露 ItemDefinitionId / DisplayDefinitionHandle；神兽联盟项目层应通过 Asset Manager（资产管理器）把稳定 DefinitionId 解析到项目 DataAsset，再通过 `TSoftObjectPtr（软对象指针） + StreamableManager（流式加载管理器）`异步加载图标、网格和材质。平台层不得硬引用 DivineBeasts 资源。

`UGamePlatformInventoryItemDefinition`作为 `UPrimaryDataAsset`可以成为 Asset Manager 的数据入口；正式项目级扫描路径、神兽联盟真实物品目录和资源映射需随内容资产一起落地，不在没有真实资产时伪造空 Catalog。
