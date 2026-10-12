# DBAClient：技能栏自动数据绑定与图标资源规范

> 当前状态：已实现 C++ ViewModel/技能栏面板绑定代码及客户端 UI 数据资产类型；2026-10-09 已制作十二生肖共60枚技能槽位PNG图源并保存到各英雄包，真实Texture2D导入状态见逐包MonolithGenerationManifest.json。没有正式技能栏Widget Blueprint（控件蓝图）或已批准的技能图标绑定，不代表屏幕显示已验收。

## 依赖层级

GamePlatformUIClient（游戏平台通用槽位） → DivineBeastsUIClient（神兽联盟客户端适配） → DBAUIPack_Core（项目公共 UI 内容包的真实 Widget 蓝图）。
DBAClient 只读 DBAGameplay/DivineBeastsAbilitiesRuntime（项目权威授权快照）；GamePlatformUI 不反向依赖神兽联盟。

## 自动初始化主流程

1. HUD（战斗信息界面）构造后，UDivineBeastsAbilityBarPanel（技能栏面板）绑定 owning PlayerController（拥有者控制器）变更 Pawn（玩家实体）事件。
2. 控制器当前 Pawn 存在 UDivineBeastsAbilityLoadoutComponent（技能装配组件）时，创建 UDivineBeastsAbilityBarViewModel（技能栏视图模型）并订阅 OnLoadoutChanged（技能授权状态变化）。
3. ViewModel 只使用服务器 OwnerOnly（拥有者定向）复制的 bReady/Slots/AbilityId/InputTag（是否已授予、槽位、技能身份、输入语义），构造现有 FGamePlatformUISlotState（平台通用槽位状态）。
4. 通过 HeroDefinitionId 只读查询已登记 Profile 主资产软路径，使用 GamePlatformData 的世界租约异步加载 Profile，再批量去重加载图标；回调验证原 Loadout、弱 ViewModel、英雄、代次及租约身份。
5. 将只读槽位状态提交既有 UDivineBeastsAbilityBarPanel::ApplyAbilitySlots；项目技能栏不执行 GiveAbility/伤害/服务器数据修改。
6. 未配置或加载失败的图标保持空软引用，使用正式 Widget 的缺省视觉；图标缺失不得将未获授权的技能伪装为可用。被动技能可以没有输入 Tag，但不允许点击后私自调用技能。
7. Widget 销毁或换 Pawn 时解绑 Controller / Loadout 委托，撤销当前代次并仅释放本 ViewModel 的 Profile 与图标两份 Data 租约。多 LocalPlayer 使用各自控件和租约，不全局卸载其他调用者资源。

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

第四张原稿增加左上PlayerPortrait/PlayerStatus头像框，底部显示改名为ActionBarPortrait/ActionBarStatus，仅是同一本地玩家数据的两处显示实例。图腾复用当前英雄肖像；未知角色等级显示“—”，技能Spec等级不等于角色等级。四份原稿与十一份UI材质直接采样原图RGB，只有动态内容区和背景透明度被遮罩。技能石框固定72像素、底座固定880×293，均不按窗口比例缩放字体；最终客户端观感仍需人工核验。

## 2026-10-12 UI资源租约与默认根布局异步化

同轮补齐 Profile 已有说明契约：`ValidateProfile` 拒绝缺失或纯空白 `Description`，保留两条既有说明回归全部断言；不代填名称、介绍或修改作者资产。`AuthorZodiacDevelopmentAbilities.py` 的现行作者路径为主动和非主动候选分别填写非空开发说明，尚须主代理用已保存 Profile 的引擎回读确认当前资产值。此收紧会让缺说明 Profile 走现有安全缺省，而不会改变 GAS 授权。

技能栏现在通过 `UAssetManager::GetPrimaryAssetPath` 只读查询原有 `DivineBeastsAbilityUIProfile:HeroDefinitionId`，随后用现有 `IGamePlatformDataService::AcquireResources` 申请普通资源租约。Profile 仍继承 `UPrimaryDataAsset`，不改为平台 Definition，也不改主资产、反射或图标软引用身份。Profile 成功并通过当前校验后，按条目图标路径去重申请第二份资源租约；所有对象都从已加载软路径解析，无同步读盘。未登记/失败 Profile 保持缺省文本；图标批次失败保留已验证文本，向槽位保留空图标，即便该纹理恰由其他调用者驻留也不会掩盖本次失败。

每个 ViewModel 的两份租约以真实 Loadout 为弱调用者，期限为该来源所属 World；回调另行校验弱 ViewModel、原 Loadout、英雄、加载代次及完整签发身份。Widget 创建的 ViewModel 不一定自行提供 GetWorld，因此不把无世界的 VM 误作 Data 的世界调用者。换英雄、重新绑定、解绑与 BeginDestroy 先撤本地代次及状态，再分别释放本 VM 的 Profile/图标租约；另一个本地玩家或控件的需求保持独立，不调用全局 UnloadPrimaryAsset。

默认根布局保留 `/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C`。当前源码真值是匿名 namespace 内的软类常量，未曾存在可配置反射字段；本轮保留此结构和所有原有反射字段，不新增配置来源。申请人为所属 LocalPlayer，使用 Instance 期限及 Idle / Pending / Failed / Ready 四态：同一控制器合并在途请求；失败在同一上下文不自动重试；控制器真实更换才允许新代次。已安装根与成功租约跨同一玩家的 Travel 保留。控制器撤销、外部安装抢占和 Deinitialize 会撤销相应在途请求；外部安装成功后才释放旧已显示根的自有租约。完成只用 ResolveObject 取得已加载 UClass，类型检查后安装；同步安装边界内禁止递归请求或主页面同步，Ready 发布后再恢复主路由。

新增原生行为回归位于 `Source/DivineBeastsUIClient/Private/Tests/DivineBeastsUIResourceLeaseTests.cpp`：

- `DivineBeasts.UI.Resources.AbilityProfileAndIcons.HeroSwitchReleasesBoth`
- `DivineBeasts.UI.Resources.AbilityProfileAndIcons.StaleCompletionAndDestroy`
- `DivineBeasts.UI.Resources.AbilityProfileAndIcons.MissingResourceFallback`
- `DivineBeasts.UI.Resources.RootLayout.PendingFailureAndControllerChange`
- `DivineBeasts.UI.Resources.RootLayout.ReadyExternalInstallAndReentry`
- `DivineBeasts.UI.Resources.RootLayout.LocalPlayerIsolationAndInvalidClass`

测试直接驱动真实组件事件、ViewModel 绑定/销毁以及本地玩家子系统的控制器/安装/退出入口；Data 替身仅控制资源终态及记录释放，根布局最后的视口挂载是显式测试边界。测试不伪造真实角色 Ready 或 GAS 授权；因此缺资源行为验证不能替代已授权技能栏的最终视觉验收。源码、Automation、真实 UI 资产、Cook/Stage、多人/多PIE画面分别留证。新增用例当前已写入，实际构建/运行结果以 `Docs/Implementation/PluginArchitectureOptimization/task-3-report.md` 及主代理统一验证记录为准。
