# SSLM 蓝图模块化：重名资源及消费者裁定台账（2026-10-25）

> 原则：以 UE 资产类型、当前硬/软消费者、真实父类和行为差异为准。**同名文件不必然是重复蓝图**。本次为只读审计；未经 Monolith 编辑器独立验证，不直接更名、迁移或删除任何 `.uasset` 或 `.umap`（地图资产）。

## 一、技能槽两种实现：保留，优先治理（P1）

- 正式战斗版：`/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot`（技能槽）。Monolith 离线项目索引记录为 `WidgetBlueprint`（界面控件蓝图），最新索引 2026-10-21；具备槽位状态变化函数、图标软纹理/冷却遮罩/可用状态投影节点。
- 通用组件版：`/DBAUIPack_Core/UI/Components/WBP_DBA_UI_AbilitySlot`（早期通用槽位）。索引记录 `WidgetBlueprint`，最后索引 2026-10-12；仅列出事件默认节点（不代表当前编辑器下完整图表）。
- **已确认实际消费者**：离线硬依赖中 `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar` → 正式战斗版；`DivineBeastsAbilityHotkeyTests.cpp` 与 `DivineBeastsWorldHealthAndCooldownTests.cpp` 按正式战斗版稳定路径加载；早期文档 `AAACombatWidgetDelivery_20261009.json` 记录组件版的通用槽位合同。
- **裁定**：不得因同名批量合并。后续通过 Monolith 核对两者真实控件树、父类、函数图、动态引用与 Cook，确认是否一个是合法通用容器、另一个是战斗特化；如存在重复逻辑，仅抽取通用无状态能力并分批改消费者，不动正式路径。

## 二、插件演示总览两种入口：保留，分别验收（P2）

- `/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub`（早期开发演示页面）：`DivineBeastsDemoWorldSubsystem.cpp` 有精确软类路径消费者。
- `/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub`（项目公共界面内容包演示页面）：`DivineBeastsDemoPanelTests.cpp` 和内容清单保留不同消费者。
- **裁定**：来源不同、入口不同，并非可证明的“重复废弃品”；需先复核是否有互相重叠的功能逻辑、演示模式生命周期和最终发布剥离策略。离线查询结果无反向引用不能证明没有动态加载。

## 三、新手村 PCG/水体四组同名路径：实际是重定向器（P2）

Monolith 离线项目资产索引（2026-10-24 15:01:15）确认四份旧 `Development/PCG/HillsideForest` 路径均属于 `ObjectRedirector`（对象重定向器），不是第二套蓝图/图实现：

- `BP_DBA_ReviewLake_NoCarve`（湖岸不雕刻审核蓝图）：旧路径是重定向器，新 `/PCG/HillsideForest/Water` 路径为实际 `Blueprint`（蓝图）。
- `BP_DBA_ReviewRiver_NoCarve`（河岸不雕刻审核蓝图）：旧路径是重定向器，新路径为实际 `Blueprint`。
- `PCG_DBA_HillsideForest_Review`（山林 PCG 审核图）：旧路径是重定向器，正式图位于 `/PCG/HillsideForest`。
- `PCG_DBA_ReviewSpatialSource`（空间数据来源审核图）：旧路径是重定向器，正式图位于 `/PCG/HillsideForest`。

**裁定**：将上述四组从“重复蓝图重构”问题降级为“迁移重定向与引用兼容性检查”。没有确定所有软引用、旧地图/实例和烘焙入口时**不得删除**旧重定向器，避免破坏已有软路径。

## 四、执行门禁与后续动作

1. 恢复 Monolith 实时编辑器只读查询后，逐项回读上述八类路径的实际类型和引用；记录真实父类/Widget 树/事件图、软引用、硬引用、运行使用者和打包产物。
2. 若未发现消费者但仍可能有字符串拼接、注册表或外部地图软路径，则先标记隔离复核，不据“引用查询为空”删除。
3. 任何资产迁移必须先确认稳定 ID、引用替换及回退方案；实际资产保存/编译/冷重载仅允许 Monolith；新手村地图与 Client/Server Cook 需要单独验证。
4. 当前状态：**资产源头/消费者静态核查完成；原生当前会话复核和重构未完成，未删除任何资产**。
