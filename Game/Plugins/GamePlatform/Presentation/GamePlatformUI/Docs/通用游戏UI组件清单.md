# GamePlatformUI 通用游戏界面组件清单与工程状态

> 工程：DivineBeastsWorkspace（神兽联盟工作空间），UE5.8。
> 所有者：GamePlatformUI/GamePlatformUIClient（游戏平台用户界面客户端）。
> 原则：GamePlatform不引用任何生肖、竞技或神兽联盟资源；中立C++类提供数据快照校验、无业务Tick的事件更新和Widget Blueprint扩展点。视觉蓝图由使用该插件的具体项目内容包拥有，不把《神兽联盟》美术下沉至平台层。

## 2026-10-09 3A通用 UI 体系补充索引

已参考《魔兽世界》《最终幻想XIV》《命运2》《暗黑破坏神IV》《英雄联盟手游》及《无畏契约》官方公开界面资料，整理为`DBAClient/Docs/3A游戏UI十大业务域组件总清单_V1.0.md`（263个目标条目、十个业务域）。完整机器可读台账为`DBAClient/Docs/AAAGameUIBusinessDomainLedger_V1.json`。该清单属于**目标设计库**，不等同于263个新类/蓝图已实现。

Buff（增益）、Debuff（减益）、CC（控制）、DoT/HoT（持续伤害/治疗）、驱散、免疫、到期提醒、显示优先级和战斗关键警告的复用机制详见本插件`Docs/AAA状态效果UI设计规范_V1.0.md`。继续以现有`UGamePlatformStatusEffectTrayWidget`（状态效果托盘）为唯一公共基础类，不恢复/新建相互重复的Buff与Debuff业务服务，不把玩法权威状态放入UI。

研究分类/资料映射的机器台账门禁：`Tests/Architecture/ValidateAAAUIInventory.py`（十域研究目录静态校验）；真实页面及Widget资产门禁仍由现有`TestUIDeliveryInventory.ps1`独立负责。

## 现有通用基础组件

- `UGamePlatformWidgetBase`（普通控件基类）：ViewModel与设备适配、生命周期成对绑定。
- `UGamePlatformActivatableWidgetBase`（可激活控件基类）：屏幕/菜单/弹窗焦点与CommonUI激活生命周期。
- `UGamePlatformComponentWidget`（复用组件基类）：头像、资源条、槽位等视觉组件统一父类。
- `UGamePlatformPanelWidget`（业务面板基类）：HUD内的复合面板通用父类。
- `UGamePlatformResourceBarWidget`（资源条）：可展示生命值、护盾、法力、经验、施法进度等，关联标准`ProgressBar`控件。
- `UGamePlatformPortraitWidget`（头像/肖像）：玩家/队友/目标/英雄肖像，图标采用软引用。
- `UGamePlatformSlotWidget`（槽位）：技能、物品、装备和快捷栏槽位。
- `UGamePlatformFeedbackWidget`（即时反馈）：命中、治疗、伤害飘字等，服务层负责池化与去重。
- `UGamePlatformNotificationWidget`（通知）：系统通知与浮动提示。
- `UGamePlatformWorldWidgetBase`（世界投影）：名称板、血条和世界标记的公共机制。
- `UGamePlatformDialogWidget`（对话框）、`UGamePlatformTooltipWidget`（通用悬浮提示，新增）。
- `UGamePlatformUIManagerSubsystem`（界面管理）：统一Screen、HUD、弹窗、通知层及异步加载，禁止业务Widget直接AddToViewport。

## 本轮新增的通用组件

以下每个类均位于`Source/GamePlatformUIClient/Public/Components/`，并具有对应`Private/Components/*.cpp`实现；全部继承`UGamePlatformComponentWidget`。数据由应用/领域适配器以事件快照提交，不存第二套业务权威事实。

1. `UGamePlatformMinimapWidget`（小地图）：`FGamePlatformUIMinimapState`支持地图软纹理、归一化玩家位置、旋转、缩放、最多128个标记及校验。世界投影和视野权限由上层决定。
2. `UGamePlatformPartyRosterWidget`（组队成员）：`FGamePlatformUIPartyRosterState`支持最多32个成员、头像、生命/护盾百分比、就绪/在线/队长提示，使用来源FGuid+Revision（作用域/修订）防跨账号串号。
3. `UGamePlatformCountdownWidget`（倒计时）：`FGamePlatformUICountdownState`支持比赛阶段、任务、技能、训练等倒计时，严格校验时间与修订，`CountdownText`可选绑定；不产生游戏Tick、Timer或伪剩余进度。
4. `UGamePlatformSlotBarWidget`（技能/快捷栏）：`FGamePlatformUISlotBarState`支持最多32个`FGamePlatformUISlotState`槽位，自动验证唯一标识、进度和修订；不决定能否施法。
5. `UGamePlatformStatusEffectTrayWidget`（状态效果托盘）：`FGamePlatformUIStatusEffectTrayState`展示最多64个正负状态、图标、叠层、剩余时间，不执行效果。
6. `UGamePlatformQuestTrackerWidget`（任务追踪）：`FGamePlatformUIQuestTrackerState`支持最多16个可视任务目标与进度，接受Quest客户端事件投影。
7. `UGamePlatformInteractionPromptWidget`（交互提示）：`FGamePlatformUIInteractionPromptState`统一交互目标、输入语义、操作名称及禁用态；不直接派发交互命令。
8. `UGamePlatformSettingRowWidget`（系统设置项）：`FGamePlatformUISettingRowState`展示业务服务已确认的名称、值、错误、忙碌状态，平台UI绝不替代GamePlatformSettings/Input/SFX持久化。
9. `UGamePlatformTooltipWidget`（通用悬浮提示）：`FGamePlatformUITooltipState`承载标题、正文和软纹理，支持隐藏及修订隔离。

## 2026-10-09 战斗M1新增的三个平台级显示组件

以下是本次实际新增并经过UE5.8 `DivineBeastsArenaEditor Win64 Development -SingleFile`（编辑器目标单文件编译）验证的三种通用显示类，公开头文件和Private源文件都放在本插件`Source/GamePlatformUIClient/Components`相应Public/Private目录中。

1. `UGamePlatformCastProgressWidget`（通用施法与引导进度条）：`FGamePlatformUICastProgressState`（施法显示快照），支持CastInstanceId（施法实例身份）、SourceScopeId（来源作用域）、总/剩余时间、Channeling（引导）、Interruptible（可打断）、Critical（关键施法）与递增Revision（修订）；父类不持有权威技能、不使用逐帧Tick。
2. `UGamePlatformTargetFrameWidget`（目标/焦点/首领状态框）：`FGamePlatformUITargetFrameState`（目标显示快照）复用`GamePlatformPortraitWidget`（肖像）与`GamePlatformResourceBarWidget`（资源条）。权限源撤销目标可见性时除自身状态外也清空内部头像/资源条缓存，防止迷雾切换泄漏旧目标。
3. `UGamePlatformCombatAlertWidget`（关键战斗预警）：`FGamePlatformUICombatAlertState`（警告快照）支持Info/Warning/Critical（提示/警告/关键）、AlertInstanceId（实例身份）、倒计时与动作建议；它是常驻预警组件，不代替通知服务，也不从UI预测首领机制。

本轮`UGamePlatformStatusEffectTrayWidget`（状态效果托盘）已在源码中包含EffectInstanceId（实例身份）、SourceDisplayId（授权施放者）、Polarity（正负/中性/条件显示性质）、Mechanic（控制/持续/免疫机制）、Importance（重要性）、DispelCategory（驱散类别）、SourceScope（来源作用域）、Critical（关键机制）、容量排序、Buff/Debuff/警告分区与Overflow（溢出）投影。当前仍缺少完整的业务事件适配、可见性源验证和实际Monolith蓝图资产；不应再把这些已经存在的C++字段标记为“尚未编码”。

新增边界测试：`Private/Tests/GamePlatformUICombatPresentationTests.cpp`（施法、目标可见性、预警输入与Buff/Debuff分组），并将`GamePlatformUIComponentCatalogTests.cpp`（组件继承审查）更新为15个中立组件。测试源码已定向编译，**原生Automation尚未真实运行**。

12份新的战斗视觉规格由`Tools/Unreal/UI/GenerateCombatUIImplementationSpecs.py`生成至`Saved/Monolith/CombatUIImplementationSpecs`。该目录仅含JSON规划，真正神兽主题蓝图只能在`DBAUIPack_Core/Content/UI/Components`由Monolith MCP创建、编译、保存和回读。

## 基类使用与蓝图所有权

```text
GamePlatformUI（平台通用机制）
  └── UGamePlatformComponentWidget（平台通用组件基类）
       ├── UGamePlatformResourceBarWidget（资源条）
       ├── UGamePlatformPortraitWidget（头像）
       ├── UGamePlatformSlotWidget（技能/物品槽）
       ├── UGamePlatformSlotBarWidget（技能/快捷栏）
       ├── UGamePlatformMinimapWidget（小地图）
       ├── UGamePlatformPartyRosterWidget（队伍）
       ├── UGamePlatformCountdownWidget（倒计时）
       ├── UGamePlatformStatusEffectTrayWidget（状态效果）
       ├── UGamePlatformQuestTrackerWidget（任务追踪）
       ├── UGamePlatformInteractionPromptWidget（交互提示）
       ├── UGamePlatformSettingRowWidget（设置项）
       ├── UGamePlatformTooltipWidget（悬浮提示）
       ├── UGamePlatformCastProgressWidget（施法/引导条）
       ├── UGamePlatformTargetFrameWidget（目标/焦点状态框）
       └── UGamePlatformCombatAlertWidget（关键战斗预警）
            ↓
DivineBeasts/DBAUIPack_Core（第三层项目美术蓝图）
  └── /DBAUIPack_Core/UI/Components/WBP_DBA_UI_*（神兽主题实物Widget）
```

其他MOBA项目可以另设自己的内容插件，只引用上述中立父类，不复用神兽联盟专属视觉根路径。竞技记分板和比赛计分是`GamePlatformArenaClient`（MOBA领域）与`DBAArena`（项目竞技领域）职责，不进入平台UI。

## 状态与验收标准（截至2026-10-09）

- 既有ResourceBar、Portrait、Slot、Feedback、Notification和WorldUI：原有C++类存在。
- 新增9个通用C++组件：源码、中文注释及蓝图接口已写入。UE5.8已完成UHT（虚幻头文件工具）解析和9个相关.cpp的SingleFile（单源文件）编译，其中技能栏局部变量Slot命名冲突已修复并复编通过；完整模块链接及实际Automation执行仍未完成。
- 真实蓝图：平台插件自身不提供《神兽联盟》视觉蓝图。项目`DBAUIPack_Core`目前已有6个页面/卡片/根布局Widget蓝图；`WBP_DBA_UI_HealthBar`、`WBP_DBA_UI_PlayerPortrait`、`WBP_DBA_UI_Minimap`等12个通用组件的神兽视觉蓝图须由Monolith MCP生成、编译、保存和回读后才能标记“已交付”。
- 检查入口：`GamePlatform.UI.Components.Hierarchy`（平台组件类层级自动化测试）及`Tests/Architecture/ValidateGameUIComponentInventory.py`（静态资源清单检查，待/已分开）。
- 不允许以规格JSON、资产软路径、C++单文件编译或静态检查的存在声称蓝图已完成。
