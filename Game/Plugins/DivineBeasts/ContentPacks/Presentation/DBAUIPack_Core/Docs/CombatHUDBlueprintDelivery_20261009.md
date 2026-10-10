# 《神兽联盟》战斗用户界面真实蓝图交付与运行接入记录

> 2026-10-09；固定工程 DivineBeastsWorkspace（神兽联盟工作空间）/ Game/DivineBeastsArena.uproject（神兽联盟虚幻工程）。
> 所有真实.uasset（虚幻资产）均由已连接该项目的 Monolith MCP 0.23.0（虚幻编辑器资产操作服务）创建/调整；本记录只保留当前可核对事实，**未把蓝图编译通过冒充完整UE C++编译或实际联机成功**。

## 一、已创建及更新的实际蓝图

| 三层逻辑归属 | Widget Blueprint（控件蓝图） | 父类、真实Widget树与更新 | 当前编辑器编译 |
| --- | --- | --- | --- |
| 第三层DBAUIPack_Core（项目客户端界面内容包） | `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD`（战斗主HUD） | 新建；父类 `DivineBeastsCombatPanelBase`（项目战斗面板），CanvasPanel（画布）+ PlayerStatus（玩家状态）、PlayerPortrait（头像）、TargetFrame（目标）、StatusEffects（增减益托盘）、AbilityBar（五技能槽）和CastBar（施法条），最新共10个树节点，新增小地图、队伍列表与倒计时三个可选子控件 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout`（客户端根布局） | 在已存在的 `HUDLayer`（HUD层）里真实增加`CombatHUD`（战斗主界面）子控件，Slot为Fill（填满），默认Collapsed（隐藏）以免遮挡登录/选角；根树共11节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Components/WBP_DBA_UI_PlayerStatus`（玩家状态） | 删除 `ShieldBar`（旧护盾条）与 `ShieldBarLabel`（护盾标签），只保留同名且可绑定的HealthBar（生命条）、MomentumBar（气势条）；重新压缩高度和重排位置，共7节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Components/WBP_DBA_UI_TargetFrame`（当前目标框） | 删除 `TargetShieldBar`（旧目标盾条），保留授权生命、头像与状态标签；压缩布局，共6节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Components/WBP_DBA_UI_StatusEffects`（状态托盘） | 保留已存在的BuffItems（增益区）、DebuffItems（减益区）、CriticalItems（关键机制区），标题中文化；Critical在这里表示重要控制/关键机制，**不是被删除的伤害暴击属性**；共11节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar`（五槽技能栏） | 复用已存在5个真实命名子控件及 `DivineBeastsAbilityBarPanel`（项目技能面板），不复制创建第二套技能条；6节点 | 旧资源原有编译状态，现已复核树结构 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_MatchCountdown`（比赛倒计时） | 新建；继承 `GamePlatformCountdownWidget`（平台倒计时组件），真实绑定 `CountdownText`（时间文本），4节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_Minimap`（战斗小地图） | 新建；继承 `GamePlatformMinimapWidget`（平台小地图组件），具备 `MapImage`（已加载地图纹理）变量及`PlayerMarker`（仅按授权位置显示的玩家标记），5节点 | 0错误、0警告 |
| 第三层DBAUIPack_Core | `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_PartyRoster`（组队成员栏） | 新建；继承 `GamePlatformPartyRosterWidget`（平台组队显示组件），预留 `PartyMembers`（成员列表）变量，5节点 | 0错误、0警告 |

以上前5个包通过Monolith `save_packages`（保存实际引擎资源）**成功保存5/5**，并由 `get_widget_tree`（回读真实控件树）再次确认身份、父类、控件数量及嵌套关系。没有通过纯文本生成或伪造uasset资产。

新一轮已通过Monolith另外创建并保存倒计时、小地图、队伍成员栏3份真实蓝图，并将其作为3个可选子控件组合到原`CombatHUD`（主战斗界面）：主蓝图从7个节点增至10个节点。该轮4份受影响的.uasset（3新建＋主界面更新）均成功保存；全部使用第一层通用UI基类，不创建第二套同领域C++控制器。地图、队伍和倒计时缺少可靠授权数据时均默认折叠，不展示虚构内容。具体Monolith回读结果已更新资产清单的`LatestCombatHUDSecondaryWidgets`（P0补充三控件记录）。

## 二、蓝图布局与运行数据

- HUD布局：玩家头像+生命/气势位于左上；目标身份与生命位于上方中央；增益/减益/关键机制位于右上；技能栏与施法条位于底部中央。对应使用CanvasPanel锚点和Alignment（对齐点），不是固定1920屏幕绝对坐标。移动平台仍需安全区/DPI实机验收。
- 新增小地图放在右下、队伍成员放在左侧、竞技倒计时位于目标框下方；采用相对窗口四边或中线的锚点。它们当前只实现真实WBP及输入快照通用父类，尚未从World（世界）、Party（队伍）、Arena（竞技）领域订阅已授权的地图、成员和对局截止时间。没有合法数据时控件保持Collapsed（隐藏）；不得声称业务已联通或有可用的实时倒计时。
- 追加通用控件源代码行为：`GamePlatformCountdownWidget`（通用倒计时）已接入可信快照的`bActive`（活动标志）显示/隐藏和文本刷新；`GamePlatformMinimapWidget`（通用小地图）只展示已异步加载到内存的底图、按0～1位置把`PlayerMarker`移动到对应纹理内，不主动同步加载；`GamePlatformPartyRosterWidget`（通用队友栏）以最大32人的正式快照生成显示名称及生命百分比，切换来源或退出时清理条目，**不展示已经移除的护盾GAS数值**。当前只有数据投影和视觉更新入口，并未接通游戏世界、组队、竞技的真实来源；不把静态蓝图等同完整玩法运行。
- 目标框TargetFrame（目标状态控件）与CastBar（施法条）在新的CombatHUD蓝图中**初始Collapsed（隐藏）**；GamePlatformTargetFrameWidget（平台目标框）仅在收到已授权bVisible（当前目标可见）时展开，GamePlatformCastProgressWidget（平台施法条）仅在bActive（当前施法活跃）时展开；清空来源/结束/取消后恢复隐藏。真实目标与施法事实适配尚待运行测试，当前不生成假数据或永远空白的边框。
- 第二层MobaCommon（MOBA通用层）不复制技能栏与GAS基础属性，不添加MOBA专用必选HUD。第一层GamePlatformUI（平台通用界面层）复用ResourceBar（资源条）、Slot（技能槽）、StatusEffectTray（效果托盘）、TargetFrame（目标框）中立基类；第三层只组合生肖界面与只读数据适配。
- `UDivineBeastsRootLayout`（项目根布局C++）增加事件驱动CombatHUD子控件绑定和本地Pawn OnPossessedPawnChanged（被控制角色变化事件）。当前Pawn同时持有GamePlatformAbilitySystemComponent（GAS组件）与GamePlatformCombatComponent（平台战斗组件）时显示战斗HUD；Boot/Login/CharacterSelect（启动/登录/选角）没有正式角色时隐藏，不在UI中增加Tick（逐帧更新）。
- `UDivineBeastsPlayerStatusPanel`（玩家状态面板C++）自主管理一个临时`UDivineBeastsPlayerStatusViewModel`（只读视图模型），按当前玩家Pawn绑定现有GAS复制属性Health/MaxHealth（生命/上限）与Momentum/MaxMomentum（气势/上限），换人立即解除旧委托、清除旧显示；不会在客户端创建第二套Gameplay属性。
- `UDivineBeastsRootLayout`（项目根布局C++）对当前拥有者ASC订阅State_Shielded（临时护盾）、Control_Stun（眩晕）、Control_Silence（沉默）标签和DamageBonus/DamageReduction（增伤/减伤）属性变化，形成有ScopeId（来源代次）与递增Revision（修订号）的平台只读状态快照。实际托盘以中文文字回退显示「临时护盾、眩晕、沉默、伤害增强/削弱、减伤/易伤」，不推测隐藏敌人的GAS属性，也不虚构时长或硬编码技能图标。状态类型不变时不反复构造图标，无Widget Tick。
- 能力栏仍由 `UDivineBeastsAbilityBarPanel`（项目技能栏C++）按拥有者Pawn与服务器正式GAS AbilitySpec（技能授权规格）事件刷新；真实AbilitySet授权、技能UI Profile与图标映射尚有正式资产交付缺口。目标框与施法条的Gameplay真实事件来源同样需要独立联调，静态存在蓝图不等于游戏中已经动态显示目标/施法。

## 三、验证边界及下一步门禁

1. 蓝图：Monolith编译当前编辑器5个被修改资源均0错误/0警告；源树回读成功；保存5/5。当前编辑器的`list_errored_blueprints`（已加载蓝图错误扫描）为0项。需要在最新C++ UE5.8模块编译链接完成后**独立重启编辑器**，再次编译RootLayout/CombatHUD/PlayerStatus/TargetFrame/StatusEffects等资源并确保BindWidgetOptional（可选绑定）和父类无失效。
2. C++：本次已对`DivineBeastsRootLayout.cpp`（项目HUD入口）、`DivineBeastsPlayerStatusPanel.cpp`（生命/气势自动绑定）、`GamePlatformTargetFrameWidget.cpp`（平台目标框）、`GamePlatformCastProgressWidget.cpp`（平台施法条）执行UE5.8 `DivineBeastsArenaEditor Win64 Development`（Windows编辑器开发构建）真实单文件编译，共4/4个编译动作成功、UBT退出码均为0。**单文件编译不等于完整模块链接。** 另尝试`DivineBeastsArenaClient Win64 Development`（客户端开发构建）模块级同时编译`GamePlatformUIClient`（平台UI模块）与`DivineBeastsUIClient`（项目UI模块），进入6个编译动作队列后长时间没有结果，本会话停止任务；没有观察到C++错误，也未取得客户端链接成功证据，最新编辑器DLL尚未独立重载。
   - 原生Automation（自动化）结果：Monolith在当前已加载的编辑器执行`GamePlatform.UI.CombatPresentation`（平台战斗界面展示）2项全部通过；`DivineBeasts.UI.Combat.PlayerStatusSnapshot`（项目状态）1项失败，旧测试夹具未能创建Combat/Momentum（战斗/气势）属性集。磁盘源码已改为`AddAttributeSetSubobject`（显式向ASC登记属性集），但当前编辑器仍加载旧DLL，必须在最新模块链接及独立重启后复验。
3. Gameplay：Buff/Debuff（增减益）、护盾标签是否真正能显示取决于专用服务器授予的GameplayEffect与客户端ASC合法复制。必须测试1/2客户端、角色切换、断开重连、同一状态叠层、Stun/Silence与护盾到期。状态持续秒数未公开时显示未知，不制作假倒计时。
4. UI表现：目标与施法进度的源数据适配器、移动端锚点和安全区、键盘/触摸输入、国际化、失焦重连等未完成人工验收。历史Root Layout（登录/选角）仍作为唯一根，主HUD只在合法战斗Pawn后激活。
5. 源码归属：UI类放`DBAClient/DivineBeastsUIClient`（第三层客户端界面模块），资源放`DBAUIPack_Core`（第三层公共UI内容包），不在服务端Cook纯界面资产，不重建GamePlatform/MobaCommon插件结构。

审计主记录：`Docs/MonolithGenerationManifest.json`（Monolith生成清单）下的`LatestCombatHUDWidgetDelivery`字段；程序化静态门禁另见`Tests/Architecture/ValidateCombatHUDBlueprints.py`（战斗HUD蓝图结构与事件绑定防退化测试）。
