# DBAClient：技能栏自动数据绑定与图标资源规范

> 当前状态：已实现 C++ ViewModel/技能栏面板绑定代码及客户端 UI 数据资产类型；2026-10-09 已制作十二生肖共60枚技能槽位PNG图源并保存到各英雄包，真实Texture2D导入状态见逐包MonolithGenerationManifest.json。没有正式技能栏Widget Blueprint（控件蓝图）或已批准的技能图标绑定，不代表屏幕显示已验收。

## 依赖层级

GamePlatformUIClient（游戏平台通用槽位） → DivineBeastsUIClient（神兽联盟客户端适配） → DBAUIPack_Core（项目公共 UI 内容包的真实 Widget 蓝图）。
DBAClient 只读 DBAGameplay/DivineBeastsAbilitiesRuntime（项目权威授权快照）；GamePlatformUI 不反向依赖神兽联盟。

## 自动初始化主流程

1. HUD（战斗信息界面）构造后，UDivineBeastsAbilityBarPanel（技能栏面板）绑定 owning PlayerController（拥有者控制器）变更 Pawn（玩家实体）事件。
2. 控制器当前 Pawn 存在 UDivineBeastsAbilityLoadoutComponent（技能装配组件）时，创建 UDivineBeastsAbilityBarViewModel（技能栏视图模型）并订阅 OnLoadoutChanged（技能授权状态变化）。
3. ViewModel 只使用服务器 OwnerOnly（拥有者定向）复制的 bReady/Slots/AbilityId/InputTag（是否已授予、槽位、技能身份、输入语义），构造现有 FGamePlatformUISlotState（平台通用槽位状态）。
4. 通过 HeroDefinitionId（英雄定义身份）定位 UDivineBeastsAbilityUIProfile（英雄技能界面表现主资产），异步加载本地化名称/软引用图标；每次回调检查 LoadGeneration（加载代次）和 HeroDefinitionId，不覆盖已切换的角色。
5. 将只读槽位状态提交既有 UDivineBeastsAbilityBarPanel::ApplyAbilitySlots；项目技能栏不执行 GiveAbility/伤害/服务器数据修改。
6. 未配置或加载失败的图标保持空软引用，使用正式 Widget 的缺省视觉；图标缺失不得将未获授权的技能伪装为可用。被动技能可以没有输入 Tag，但不允许点击后私自调用技能。
7. Widget 销毁或换 Pawn 时解绑 Controller / Loadout 委托，并取消当前图标配置资产异步句柄。多 LocalPlayer（本地玩家）使用各自控件和 ViewModel，禁止进程全局缓存玩家技能栏。

## 当前实现的准确功能边界

- 当前 C++ 槽位投影读取 OwnerOnly（拥有者定向）已授权的 AbilityId/SlotId、原生 GAS AbilitySpec（技能授权实例）与 AvatarGeneration（角色代次）。只有英雄身份匹配、已复制 Spec、等级一致并且 `CanActivateAbility`（GAS 原生无副作用资格判定）通过时才将槽位标为 `bEnabled=true`（允许交互）；失效英雄会立即清空旧槽位。`OverlayProgress`（冷却遮罩）由原生冷却 GameplayEffect 的剩余时间/总时长计算，缺失效果保持 0。
- 当前 ViewModel（视图模型）已订阅平台 `OnAbilitySpecListChanged`（原生技能列表复制）、`OnAvatarBindingChanged`（技能实体代次）、GameplayEffect 新增/移除（冷却变化）、Momentum（气势）属性、Silence/Stun/Dead（沉默/眩晕/死亡）标签及角色就绪事件，并在销毁时解绑。该逻辑仍需 UE 编译、真实联机与 GAS 用例验收；冷却比例仅在事件到来时更新，**秒级连续动画/剩余秒数、详细禁用原因、游戏手柄和触屏展示尚未正式实现**。界面判断是展示提示，最终技能激活仍由服务器 GAS 校验。
- 后续补充 `FDivineBeastsAbilitySlotDetails`（技能槽详细投影）与 `GetSlotDetails()`（蓝图只读入口）：同 `SlotId` 匹配已授权技能，提供 UI Profile 的中文名称/描述、服务器等级、GAS 最新冷却剩余/总时长及当前禁用原因文字。仅在事件变化时刷新，计时动画仍需正式 Widget 实现，不能视作完整客户端效果。死亡/眩晕禁用全部技能，沉默禁用非普攻动作（仅界面提示，不取代 GAS 判定）。
- 目前 Profile 数据资产类型位于客户端模块，真实图标资源约定归 DBAHeroPack_*（十二生肖英雄内容包）的 UI/Abilities 路径。客户端 FrontEndClient（正式前端烘焙覆盖）已经登记扫描和资源目录；不能把这份覆盖当成 Editor/其他客户端配置已验证。
- GUI 真正的 WBP_DBA_UI_AbilityBar（技能栏蓝图）、WBP_DBA_UI_AbilitySlot（技能单格蓝图）及其控件树和焦点导航，**必须**通过 Monolith MCP（UE 界面连接器）在真实 UE5.8 编辑器中制作、编译、保存、重载回读，并更新对应 MonolithGenerationManifest.json（资产操作清单）。
- 本轮 Monolith MCP 端显示 Unreal Editor not running（虚幻编辑器未启动）；真实蓝图资产和客户端 Cook/Stage（烘焙/暂存）仍为阻断事项。

## 2026-10-09 图源交付与绑定边界

用户选择每个生肖五枚：普攻、被动、两个主动与终极，共60枚。完整图源位于各 `DBAHeroPack_*/SourceArt/UI/SkillIcons/`，计划引擎纹理路径沿用本规范的 `UI/Abilities`；制作与逐项状态见 `Docs/Art/十二生肖技能图标制作与接入说明.md`（美术交付说明）和 `Docs/Art/ZodiacSkillIconPrompts.json`（完整提示词与预定资产路径）。

当前ArtMotif/Slot仅标识美术主题与槽位；真实AbilityId尚未确认。不得用文件名自动构造生产技能身份、填充未授权槽位或制造有效UI Profile。待玩法批准正式技能后，由客户端Profile以软引用绑定对应图标。PNG存在、Texture2D导入、Profile绑定和真实HUD运行必须分别验收。

## 2026-10-11 当前资产与视觉状态

以上早期“编辑器未运行”、图标身份未确认与Cook阻断为历史阶段记录；已由AnimationHUD开发资产审批与真实导入、绑定、编译和烘焙记录替代，不能用历史段落判断当前状态。开发开关下启用已有候选技能，正式英雄默认配置不变；41个伤害候选可执行，19个未实现候选仍明确拒绝，不把图标存在当技能机制完成。

自然藤蔓一体底座配真实英雄肖像/等级、绿色生命和蓝色气势。槽位显示顺序为普攻、主动一、终极、主动二、被动，中央终极高亮；快捷键、图标、冷却、资格和数值均从真实复制/属性事件读取，未绑定保持空。所有文字字号固定，不把参考图的Q/W/R/E/D、2580/2580或药水数量写进默认资产。技能栏换控制器及销毁时解除实际订阅来源，旅行后从当前Pawn重新绑定。
