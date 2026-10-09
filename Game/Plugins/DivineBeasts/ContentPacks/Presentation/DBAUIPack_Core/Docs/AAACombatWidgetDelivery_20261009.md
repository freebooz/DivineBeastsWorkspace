# 《神兽联盟》战斗UI Widget Blueprint 实物交付清单（2026-10-09）

> 工作区：DivineBeastsWorkspace（神兽联盟工作空间）；引擎：UE5.8（虚幻引擎5.8）；制作：Monolith MCP 0.23.0（虚幻UI工具）。
> 当前状态：**已创建并保存17份真实.uasset，其中12份覆盖原P0战斗视觉清单，另外5份为复用组件；实际联机、PIE与Cook仍待验证。**

## 1. 真实资产清单

| Widget Blueprint（蓝图英文名称/中文职责） | C++父类（英文/中文说明） | 属性及交付状态 |
| --- | --- | --- |
| `WBP_DBA_UI_CastBar`（施法/引导进度条） | `GamePlatformCastProgressWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_TargetFrame`（目标/焦点状态框） | `GamePlatformTargetFrameWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_CombatAlert`（重要战斗预警） | `GamePlatformCombatAlertWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_BuffTray`（增益效果分栏） | `GamePlatformStatusEffectTrayWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_DebuffTray`（减益效果分栏） | `GamePlatformStatusEffectTrayWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_ControlAlert`（硬控制提示） | `GamePlatformCombatAlertWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_StatusEffectIcon`（状态效果单图标） | `GamePlatformComponentWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_BossCastAlert`（首领关键施法预警） | `GamePlatformCombatAlertWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_TargetDebuffTray`（当前目标减益列表） | `GamePlatformStatusEffectTrayWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_DispelBadge`（驱散标识） | `GamePlatformComponentWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_ImmunityBadge`（免疫标识） | `GamePlatformComponentWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_EffectOverflow`（+N 溢出收纳标记） | `GamePlatformComponentWidget`（平台通用UI显示基类） | P0；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_HealthBar`（通用生命/护盾条） | `GamePlatformResourceBarWidget`（平台通用UI显示基类） | 复用；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_PlayerPortrait`（通用肖像） | `GamePlatformPortraitWidget`（平台通用UI显示基类） | 复用；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_AbilitySlot`（技能/道具槽位） | `GamePlatformSlotWidget`（平台通用UI显示基类） | 复用；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_PlayerStatus`（项目生命/护盾/气势面板） | `DivineBeastsPlayerStatusPanel`（项目第三层战斗面板） | 项目；Monolith创建/编译/保存，实际.uasset存在 |
| `WBP_DBA_UI_StatusEffects`（增益/减益/关键效果组合） | `GamePlatformStatusEffectTrayWidget`（平台通用UI显示基类） | 复用；Monolith创建/编译/保存，实际.uasset存在 |

全部新资产实际位于 `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI/Components/`（项目第三层公共界面内容包）。所有AssetPath（资产路径）、Native Parent（原生父类）、节点数、Monolith构建报告及未完成验收字段记录在 `Docs/AAACombatWidgetDelivery_20261009.json`（逐项机器可读资产交付台账）。

## 2. 三层边界与实际组合

- `GamePlatformUIClient`（平台通用客户端UI层）拥有资源条、肖像、槽位、施法、目标状态、Buff/Debuff托盘与关键战斗预警C++通用显示父类，**不读取服务器隐藏状态，不拥有玩法授权**。
- `GamePlatformArenaClient`（MOBA通用竞技客户端层）复用上述中立控件，接比赛阶段、阵容、比分及竞技观察者允许展示的快照；本轮未创建第二套Buff服务。
- `DivineBeastsUIClient`（神兽联盟项目客户端层）已有 `DivineBeastsPlayerStatusPanel`（玩家生命、护盾与气势面板）；第三层 `DBAUIPack_Core`（项目界面内容包）拥有本次17个具体控件蓝图。
- `WBP_DBA_UI_TargetFrame`（目标状态框）实际嵌套 `WBP_DBA_UI_PlayerPortrait`（肖像控件）和两个 `WBP_DBA_UI_HealthBar`（资源条），三个命名绑定分别为 `TargetPortrait`、`TargetHealthBar`、`TargetShieldBar`，已通过Monolith回读子类及变量标识。
- `WBP_DBA_UI_PlayerStatus`（玩家状态面板）真实嵌套 `HealthBar`、`ShieldBar`、`MomentumBar`，与项目C++基类的可选绑定同名，同编辑器回读通过。`WBP_DBA_UI_CastBar`（施法条）已回读 `CastProgressBar` 与 `CastNameText` 为蓝图变量。
- `WBP_DBA_UI_StatusEffects`（状态效果组合面板）实际含 `BuffItems`（增益列表）、`DebuffItems`（减益列表）、`CriticalItems`（关键机制列表）及分别对应的 `OverflowText`（折叠数量）文本框，Monolith树回读通过。

## 3. 验收证据与边界

- **真实编辑器连接：** Monolith状态响应 `version=0.23.0`、`server_running=true`、`port=9316`、`project_name=DivineBeastsArena`。实现采用 `build_ui_from_spec`（依据规范创建单个蓝图）及 `build_menu_from_spec`（批量创建），`mode=patch`（安全增量模式），未覆盖原有六个已交付Widget资源。
- **Monolith编译保存：** 17份新资产的制作调用均返回 `bSuccess=true`、`error_count=0`、`warning_count=0`；磁盘实际扫描存在17份非零长度 `.uasset`。
- **资产静态核验：** `Tests/Architecture/ValidateCombatUIImplementationSpecs.py`（战斗UI资源审计）显示12/12目标资产存在、缺失0、旧6份Widget保留；`Tests/Architecture/ValidateGameUIComponentInventory.py`（十大业务域通用UI设计资产核验）仍有另27份视觉规格待交付，不属于本次12份战斗核心的漏项。
- **仍未执行：** 独立编辑器重启后的全部17项类与控件回读、完整自动化原生测试、PIE（编辑器内运行）、双客户端联机、WindowsClient Cook（客户端资源烘焙）、移动端可访问性与真实3A视觉验收。Monolith与资源存在仅证明资产层创建成功。
- **动态战斗事件尚未闭环：** Buff/Debuff图标增删、叠层、持续时间、来源与关键机制的客户端授权快照仍需连接蓝图事件图；玩家当前目标显示也须接到真实的选中目标/可见性服务。不能通过假设或固定假数据显示完成。
- **并行开发提醒：** 另一工作流还创建了 `UI/Combat/WBP_DBA_UI_AbilitySlot`（项目战斗技能槽）及 `UI/Combat/WBP_DBA_UI_AbilityBar`（项目技能栏），与本批次通用 `UI/Components/WBP_DBA_UI_AbilitySlot` 分属不同目录。本批次未覆盖、移动或删除这些资产；后续应确定组合和复用关系，避免重复视觉实现。

## 4. 剩余优先任务

首先补齐真实战斗数据的只读ViewModel（视图模型）→ `BP_OnEffectDisplayGroupsChanged`（增减益视觉分组更新事件）→ 动态控件池/WrapBox（图标容器）和正确的作用域取消；然后完成头像/技能槽图标软引用、单位目标更换与迷雾撤销、PC/移动布局、PIE/联机、完整Cook及独立编辑器重启回读。新增的 `AAACombatWidgetDelivery_20261009.json` 记录的是已经发生的资产交付证据，而非全部功能已验收。
