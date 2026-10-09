# 《神兽联盟》十大业务域与通用组件 Widget Blueprint 交付清单

> **最新实施状态（2026-10-09，覆盖后文历史规划阶段的「待创建」状态）：** Monolith MCP 0.23.0 已接通正式 UE5.8 编辑器并完成战斗核心12项真实Widget Blueprint创建、编译、保存，另新建通用视觉组件5项，共17份`.uasset`。项目原有6份Widget资产保留；新资产已通过磁盘静态扫描，部分通过Monolith同编辑器树回读，尚未做完整独立重启/PIE/联机/Cook验收。详见`Docs/AAACombatWidgetDelivery_20261009.md`及`Docs/AAACombatWidgetDelivery_20261009.json`。本页其余十域非战斗蓝图仍按实际资产存在情况验收。

> 视觉资产所有者：第三层`DBAUIPack_Core`（神兽联盟公共界面内容包）。
> 工具：项目要求仅允许 Monolith MCP 操作正式 UE5.8 编辑器。
> 2026-10-09 最新状态：原有6个Widget/2张纹理仍保留；新完成12项战斗P0蓝图和5项通用视觉资源，其他业务域的声明式布局规格仍不能视为真实.uasset。
> 本清单的“待制作”明确表示尚无真实蓝图，不能作为完成验收的证据。
>
> 初次审查时32份通用规格均未交付；最新静态回读显示其中5项已生成真实蓝图、27项仍待制作。另有12项战斗专项蓝图已完成（部分与通用清单重叠），不能把两套规划重复合计为29项。Monolith v0.23.0已可在锁定编辑器中实际创建和保存；本轮17项物理资产目录详见最新交付台账。

## 一、当前已真实交付的Widget（保留原资产）

- `UI/Root/WBP_DBA_UI_RootLayout.uasset`：核心根布局，项目层蓝图。
- `UI/Screens/WBP_DBA_UI_Login.uasset`：账号登录，具有原生密码处理和焦点契约。
- `UI/Screens/WBP_DBA_UI_CharacterCreate.uasset`：角色创建、生肖选择与预览。
- `UI/Screens/WBP_DBA_UI_CharacterSelect.uasset`：已创建角色选择与预览。
- `UI/Components/WBP_DBA_HeroChoice.uasset`：英雄选择视觉条目。
- `UI/Components/WBP_DBA_CharacterChoice.uasset`：角色选择视觉条目。

以上原有蓝图的真实Monolith校验记录保存在`MonolithGenerationManifest.json`，不因为本轮新建的JSON规格而更改历史验收记录。

## 二、十大业务域页面/HUD（待由Monolith实际创建）

1. Core（核心）：`WBP_DBA_UI_Boot`、`WBP_DBA_UI_LoadingTravel`、`WBP_DBA_UI_ErrorReconnect`；父类分别是`DivineBeastsBootScreen`、`DivineBeastsLoadingTravelScreen`、`DivineBeastsErrorReconnectScreen`。
2. Account（账号）：复用现有`WBP_DBA_UI_Login`，不重复制作或保存密码默认值。
3. World（世界）：`WBP_DBA_UI_OpenWorldHUD`、`WBP_DBA_UI_VillageMainHUD`、`WBP_DBA_UI_TutorialGuidance`、`WBP_DBA_UI_TrainingControls`、`WBP_DBA_UI_Quest`，父类复用世界HUD和Quest领域原生类。
4. Character（角色）：复用现有`WBP_DBA_UI_CharacterCreate`、`WBP_DBA_UI_CharacterSelect`，不重建重复角色数据库。
5. Inventory（背包）：`WBP_DBA_UI_Inventory`，必须包含`InventoryGrid`焦点控件；从平台InventoryClient消费数据。
6. Combat（战斗）：`WBP_DBA_UI_PlayerStatus`（生命/护盾/气势面板）与`WBP_DBA_UI_AbilityBar`（技能快捷栏），分别继承项目战斗状态与技能面板。
7. Social（社交）：`WBP_DBA_UI_Social`，使用`DivineBeastsSocialScreenBase`，实际社交数据未接入时不得展示假队友/好友。
8. Arena（竞技）：竞技视觉资产统一归`DBAArena`，不得放入本公共内容包；见`DBAArena/Docs/用户界面设计.md`。
9. System（系统）：`WBP_DBA_UI_SystemMenu`，继承`DivineBeastsSystemScreenBase`，带`ResumeButton`焦点和设备设置占位布局；业务保存经平台设置服务。
10. LiveOps（运营）：`WBP_DBA_UI_LiveOps`，继承`DivineBeastsLiveOpsScreenBase`，公告/活动/邮件来源未就绪时展示不可用，不伪造奖励。

## 三、跨业务域通用组件（待制作12个神兽视觉蓝图）

全部位于`Content/UI/Components/`，使用`WBP_DBA_UI_`英文稳定前缀，父类均来自`GamePlatformUIClient`（底层平台）或明确的第三层项目业务面板。

- `WBP_DBA_UI_HealthBar`：血条、盾条、能量条共用`GamePlatformResourceBarWidget`。
- `WBP_DBA_UI_PlayerPortrait`：头像、肖像共用`GamePlatformPortraitWidget`。
- `WBP_DBA_UI_AbilitySlot`：技能/物品/装备单格共用`GamePlatformSlotWidget`。
- `WBP_DBA_UI_SlotBar`：横向/纵向快捷栏共用`GamePlatformSlotBarWidget`。
- `WBP_DBA_UI_Minimap`：小地图共用`GamePlatformMinimapWidget`。
- `WBP_DBA_UI_PartyRoster`：队伍成员列表共用`GamePlatformPartyRosterWidget`。
- `WBP_DBA_UI_Countdown`：比赛/任务/技能倒计时共用`GamePlatformCountdownWidget`。
- `WBP_DBA_UI_StatusEffects`：Buff/Debuff状态托盘共用`GamePlatformStatusEffectTrayWidget`。
- `WBP_DBA_UI_QuestTracker`：任务追踪共用`GamePlatformQuestTrackerWidget`。
- `WBP_DBA_UI_InteractionPrompt`：交互提示共用`GamePlatformInteractionPromptWidget`。
- `WBP_DBA_UI_SettingRow`：显示图形、音量、键位等值共用`GamePlatformSettingRowWidget`。
- `WBP_DBA_UI_Tooltip`：装备/技能/道具提示共用`GamePlatformTooltipWidget`。

补充已有基础机制：世界名称板、世界血条、伤害飘字、屏幕通知、弹窗及加载均由`GamePlatformUI`拥有C++服务与基础类，第三层视觉实例需要通过Monolith分期生产。当前未交付的组件不得假定能在屏幕上出现。

## 2026-10-09 AAA增减益与战斗状态蓝图候选

- 当前`WBP_DBA_UI_StatusEffects`（状态效果总托盘）已通过Monolith创建、编译、保存，真实.uasset存在且同编辑器Widget树回读通过；增减益列表的数据适配、动态子项事件仍待接入。
- 已经实物创建`WBP_DBA_UI_BuffTray`（增益列表）、`WBP_DBA_UI_DebuffTray`（减益列表）、`WBP_DBA_UI_StatusEffectIcon`（效果图标）、`WBP_DBA_UI_ControlAlert`（控制效果警报）、`WBP_DBA_UI_DispelBadge`（驱散）、`WBP_DBA_UI_ImmunityBadge`（免疫）、`WBP_DBA_UI_EffectOverflow`（+N溢出）。原提议的`WBP_DBA_UI_CrowdControlAlert`未创建：使用已实物交付的`WBP_DBA_UI_ControlAlert`保持单一稳定资源身份，避免重复组件。
- 此处只声明设计目标，不擅自覆盖32份现有布局规格。真实制作优先复用平台`GamePlatformStatusEffectTrayWidget`（通用状态托盘）、`GamePlatformCountdownWidget`（倒计时）及`GamePlatformTooltipWidget`（悬浮提示）等基础类；Boss（首领）、队友与竞技的组合由第三层分别承载。
- 总清单与所有跨域UI能力见`DBAClient/Docs/3A游戏UI十大业务域组件总清单_V1.0.md`，状态效果扩展、优先级和可见性审核见`GamePlatformUI/Docs/AAA状态效果UI设计规范_V1.0.md`。

## 2026-10-09 战斗M1十二份正式蓝图制作输入

已生成并静态审核`Saved/Monolith/CombatUIImplementationSpecs/Manifest.json`（战斗视觉规格索引）和十二份JSON，并经Monolith正式制作成对应十二份真实.uasset，原32份通用布局规格和六份历史Widget未覆盖；详细实物清单单列在`Docs/AAACombatWidgetDelivery_20261009.json`。

| 目标Widget Blueprint（控件蓝图） | 平台真实C++父类 | 功能及资源状态 |
| --- | --- | --- |
| `WBP_DBA_UI_CastBar` | `GamePlatformCastProgressWidget` | 普通施法/引导进度；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_TargetFrame` | `GamePlatformTargetFrameWidget` | 目标肖像、生命、护盾；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_CombatAlert` | `GamePlatformCombatAlertWidget` | 固定关键战斗预警；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_BuffTray` | `GamePlatformStatusEffectTrayWidget` | 增益效果分区；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_DebuffTray` | `GamePlatformStatusEffectTrayWidget` | 减益效果分区；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_ControlAlert` | `GamePlatformCombatAlertWidget` | 眩晕、沉默等硬控警报；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_StatusEffectIcon` | `GamePlatformComponentWidget` | 单图标、叠层和倒计时；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_BossCastAlert` | `GamePlatformCombatAlertWidget` | 首领关键技能预警；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_TargetDebuffTray` | `GamePlatformStatusEffectTrayWidget` | 目标授权减益；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_DispelBadge` | `GamePlatformComponentWidget` | 驱散类别符号；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_ImmunityBadge` | `GamePlatformComponentWidget` | 免控/免伤符号；Monolith创建/编译/保存已通过 |
| `WBP_DBA_UI_EffectOverflow` | `GamePlatformComponentWidget` | +N效果溢出展开入口；Monolith创建/编译/保存已通过 |

**设计与业务边界：** Monolith执行每份布局规格时必须校对真实C++父类、BindWidgetOptional（可选命名子控件）类型、ViewModel事件图、焦点、安全区及PC/移动端布局，并通过蓝图编译、保存、编辑器重载后才能改变对应“待创建”状态。图片、头像、效果和游戏时间均来自授权事实，不能在设计规格里写成已存在业务数据。

**故障与恢复记录：** 早期正式编辑器启动曾被缺失模块和`FailedDueToEngineChange`阻断；此后Monolith v0.23.0和正式UE5.8编辑器连接成功，十二项战斗专项和五项通用视觉蓝图已真实编译保存。此前完整编辑器独立构建曾因`LNK1181`（部分引擎静态库在当时无法链接）失败，这与本次Monolith创建成功是两个独立证据；完整UE构建、PIE和Cook仍需复验。当前MonolithGenerationManifest新增独立交付台账引用，不覆盖历史六个Widget的旧编译证据。

## 四、布局规格与制作门禁

- 规格生成器：`Tools/Unreal/UI/GenerateGameUIComponentSpecs.py`。只在`Saved/Monolith/GameUIComponentSpecs/`生成32份JSON规格与一份Manifest，不写任何二进制资产。
- 工具执行：`Monolith MCP/ui_query`下的`build_ui_from_spec`等正式可用动作。必须先`monolith_status`（服务状态）和`project_query`（工程身份）验证引擎处于`Game/DivineBeastsArena.uproject`。
- 创建后对每份蓝图按“父类 → Widget Tree → BindWidget必须控件 → 焦点 → CommonUI层 → 编译 → 保存 → 重载/回读”的顺序留证。
- 优先复用已有主题颜色、背景、Logo，不把项目美术复制到平台层；PC/移动端初期共享公共布局，专属移动版必须真实交付才增加软引用。
- 验证通过后方可更新`Docs/MonolithGenerationManifest.json`的真实Assets记录；没有Monolith回应不得填写`Saved=true`、`CompileErrors=0`等虚假通过项。
