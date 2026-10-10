# DBAClient（神兽联盟客户端组合插件）

2026-10-10项目主题入口已接入现有`DivineBeastsUIClientSubsystem`（项目本地玩家UI组合根），读取配置后交给平台主题服务。当前默认主题ID为空，不改变现有登录、LOGO、字号与命名控件；真实主题/样式须经Monolith创建验收后再启用。详见[UIThemeIntegration（项目主题接入）](Docs/UIThemeIntegration.md)。


正式位置：`Game/Plugins/DivineBeasts/DBAClient/`。插件按模块宿主类型隔离客户端代码；虽然包含一个`Runtime`项目表现语义模块，服务器模块不得依赖任一`ClientOnly`模块。

| 模块 | 宿主 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsApplicationFlowClient` | 客户端 | 登录后角色创建/选择、项目准入适配和流程状态 |
| `DivineBeastsInputClient` | 客户端 | 项目输入语义、项目Profile校验、平台输入事件桥和移动Touch项目适配 |
| `DivineBeastsPresentationRuntime` | 双端 | 项目表现上下文、目录和内容包稳定语义 |
| `DivineBeastsPresentationClient` | 客户端 | 项目表现适配与平台注册 |
| `DivineBeastsUIClient` | 客户端 | 项目界面契约、路由和ViewModel |

插件直接声明模块所需的`DBAGameplay`与平台插件依赖，不依赖DBAArena、GamePlatformArena或MobaPresentation。竞技客户端模块已迁入DBAArena；它单向消费本插件的公开流程扩展，注册与注销仍按GameInstance作用域处理。通用流程执行器、平台UI与VFX播放实现不归本插件。

项目用户输入设计见 `Docs/InputArchitecture.md`。`DivineBeastsInputClient` 单向组合 `GamePlatformInputClient`，使用 `DivineBeasts.Input.*` 稳定Tag，不复制Enhanced Input/租约/重绑定实现；旧平台攻击/技能枚举只保留兼容，神兽联盟Profile明确禁止继续使用。

2026-09-30单体客户端启动修复：项目输入标签不再于CRT静态初始化阶段调用`UGameplayTagsManager`，改为引擎初始化后的游戏线程首次调用时缓存。标签仍归`DefaultGameplayTags.ini`，身份与GAS映射不变。平台输入生产标签与Development测试标签遵循同一边界；必须以真实Cook客户端启动回归补证，编辑器模块加载成功不能证明单体客户端可启动。

之前引用的`DivineBeastsApplicationContracts.generated.hpp`在仓库和生成目录均不存在，代码未使用其中声明；已移除该孤立include。后端请求继续走现存 Shared HTTP（共享HTTP）契约与真实HTTP适配代码，不恢复重复协议层。

### ApplicationFlow 当前正式状态（2026-09-27复核）

`DivineBeastsApplicationFlowClient（神兽联盟应用流程客户端）` 已完成到现行平台流程契约的源码迁移：项目层不再调用旧 `StartRun / TransitionTo / BeginOperation / IsOperationCurrent / InvalidateRun / RegisterNode / UnregisterNode / AllowedNextNodes`，而是通过 `RegisterNodeFactory（注册节点工厂） + UGamePlatformFlowDefinition（流程定义） + GamePlatformData Lease（数据租约） + StartFlow（启动流程） + SubmitEvent（提交事件） + FGamePlatformFlowNodeToken（流程节点令牌）` 使用唯一平台状态机。

项目层保留 `UDivineBeastsApplicationFlowContext（神兽联盟应用流程上下文）` 作为跨节点业务载荷；上下文只保存稳定业务值，世界、Actor、Widget 不跨地图强持有，Endpoint 与 TransferTicket 仅在私有字段中短暂存在并在交给 Session 后清除。Authentication、CharacterEntry、WorldReady、InWorld 已改为通过平台 `GamePlatformApplicationFlowNodes::CreateAwaitEventFlowNode（创建外部事件等待流程节点）` 复用现有 Callback Flow Node，原项目专属被动节点及其基类已删除；其余步骤通过平台 Callback Flow Node（回调流程节点）执行。整个模块没有业务级 `Tick` 或 `FTSTicker`。

世界进入继续采用真实 Loading（加载）屏障：六项就绪事实绑定 ObservationId（观察身份），世界逻辑身份通过 Data 租约加载 `UDivineBeastsWorldDefinition（神兽联盟世界定义）`，再以 `MapIdentity（地图资产身份）` 校验当前实例真实世界。`MapId` 仍是后端/会话逻辑标识，不直接当磁盘地图包路径。Session 已提供公开 `BeginTransfer / CancelTransfer / ReportLocalFact / OnSessionChanged` 契约；客户端不得自行伪造 NetworkConnected 或 AdmissionConfirmed。

本轮真实验证：`Tests/Architecture/ValidateProjectHeaders.ps1` 检查 293 处自有头文件引用、0 缺失；`DivineBeastsApplicationFlowClient`、`GamePlatformUIClient` 与 `DivineBeastsUIClient` 已使用 UE5.8 客户端目标完成定向编译；平台 `GamePlatformApplicationFlow` 原生生产调度核心加入按需唤醒与下一唤醒时间契约后，Debug/Release 均为 `Cases=32 Failed=0`。完整 `DivineBeastsArenaClient` 构建本轮使用 `-NoUBA -MaxParallelActions=1` 重新执行后，最新 UHT 阻断位于本任务范围外的 `GamePlatformAbilitySystem/Types/GamePlatformAbilityGrant.h`：无法找到 `UGamePlatformGameplayAbility` 与 `UGamePlatformAttributeSet`。UHT 在此提前终止，因此不能据此判断后续模块状态，也不能宣称完整客户端或端到端世界进入已通过。

正式现行设计见 `Docs/ApplicationFlowArchitecture.md`；迁移前文档与旧描述快照继续保留在 `Docs/Legacy/`，不得作为恢复旧接口的依据。

### 当前用户界面设计

项目层用户界面当前设计基线见 `Docs/用户界面设计.md`。该文档定义了分类基础类继承、事件驱动更新、PC/移动端适配、目录规划、界面清单、命名规范和分阶段实施顺序。

2026-10-09 新增 `UDivineBeastsAbilityBarViewModel`（技能栏视图模型）和 `UDivineBeastsAbilityUIProfile`（客户端英雄技能图标/名称主资产类型），现有 `UDivineBeastsAbilityBarPanel`（技能栏面板）已接入服务器授予快照的只读状态。仅源码可用，**尚无真实 Widget 技能栏蓝图、技能图标或完整 GAS 冷却投影**，不得称已在游戏画面展示。详见 [AbilityBarAutoBinding.md（技能栏自动绑定说明）](Docs/AbilityBarAutoBinding.md)。

### 十二生肖技能 VFX（视觉特效）架构

项目层十二生肖技能 VFX 设计基线见 Docs/ZodiacSkillVFXArchitecture.md。DivineBeastsPresentationRuntime 只保存 HeroDefinitionId（英雄定义编号）、AbilityId（技能编号）、SkinId（皮肤编号）等稳定表现上下文与 Hero VFX Profile（英雄视觉特效配置）；具体 Niagara（粒子特效）、Material（材质）、Texture（纹理）、Mesh（网格）和 Decal（贴花）归各 DBAHeroPack_* 内容包。竞技事实由 MobaPresentation 转为中立表现语义，最终仍由唯一 GamePlatformVFX 执行器播放。当前真实 Ability 资产尚未交付，不得为了填充目录虚构生产技能 ID 或伪 .uasset。

2026-10-09新建 `DivineBeastsPresentationRuntime/Definitions/DivineBeastsCombatFeedbackCatalog`（项目英雄技能命中反馈DataAsset类），精确关联英雄定义ID＋技能定义ID到平台反馈Profile软引用以及VFX/SFX逻辑定义ID，支持重复键与缺失项校验。**未生成真实.uasset，也未在DBAArena竞技组合根注入**；不能视作全部十二生肖已自动加载。详见 `Docs/Implementation/CombatFeedbackExecutionPlan_20261009.md`。

同一专项的 `DivineBeastsInputClient`增加按本地Pawn视觉顿帧自然恢复后的动作回放，消费 `GamePlatformInputClient` 中立有限输入缓冲；按当前BindingGeneration（绑定代次）和有效期筛选，再交给现有GAS `AbilityInputPressed/Released`进行合法性处理。只在Client执行；焦点丢失、切换角色和跨World旧输入不得回放，实际技能取消窗口与Client/Server联机仍需专项验证。

本次还为DBAClient的Client/Editor目标声明GamePlatformSFX依赖，以启用现有SFX表现Provider。项目技能目录目前只是可复用类型，不存在12生肖真实Profile完整资产；竞技组合根应按当前HeroDefinitionId与AbilityDefinitionId从GamePlatformData已加载目录选择Profile和VFX/SFX逻辑ID，在角色切换及世界销毁时撤销租约。实施与阻断见`Docs/Implementation/CombatFeedbackWorkOrders_20261009.md`。

2026-10-09结构落实：`UDivineBeastsCombatFeedbackCatalog`升级为`UGamePlatformDefinitionBase`正式主资产契约，并将每行Profile软路径转换为`FPrimaryAssetId ProfileDefinitionId`，实现与平台DataService租约一致的稳定逻辑身份；所有未知、重复及非法映射发布前拒绝。项目公共客户端保持不依赖Moba；真实按技能加载与注入归可选`DBAArena`客户端组合根。

`F:\\VFX Lib` 的复用映射见 `Docs/ZodiacReuseMatrix.md（十二生肖VFX复用矩阵）`。首批优先 Rabbit（卯兔）、Horse（午马）、Goat（未羊）、Rooster（酉鸡）、Boar（亥猪）；当前只完成平台母版能力与复用规划，未创建任何虚构技能 `.uasset`。

P0 UI 底座已开始落地：GamePlatformUI 已新增普通/可激活分类基类、LocalPlayer 自适应子系统和 SafeZone 支持；DivineBeastsUIClient 已新增项目分类基类，并建立登录、真实加载、RootLayout 和五类 HUD 的 C++ / Blueprint 父类。ApplicationFlow 的 Blueprint `uint64` 反射阻断和 GamePlatformUIClient 生成代码错误已经消除；早期完整客户端构建曾阻断于主工程 Online/PCG 头依赖及 GamePlatformWorld 测试源码；本次2026-10-09完整Client构建已退出0，旧阻断不再代表当前状态。UI 与 Flow 仍须保持事件驱动、禁止逐帧轮询。

2026-10-09 联机验证前修复：DivineBeastsPresentationRuntime直接使用平台身份与结果的DLL导出方法，Build.cs现显式声明GamePlatformCore公开依赖。原链接缺失FGamePlatformId/FGamePlatformResult符号的复现日志与修复后退出0日志位于Saved/Validation/VillageFlow/20261009/EditorModulesBuild.json所指证据目录。登录和非竞技世界的依赖方向不变。

同次独立编译整改：角色外观与预览的.cpp直接包含其使用的AnimInstance、SkeletalMesh及SkeletalMeshComponent完整类型，不依赖Unity文件顺序或共享PCH补齐模板类型。这些修改仅补充编译边界，不调整角色身份、资源租约或运行时流程。

本次完整Native构建证据：Client退出0见Saved/Validation/FoundationM0/0d516f9c-7d9c-4a49-b509-167c307aec0e/Build/Client；Server退出0见Saved/Validation/FoundationM0/8e56f639-5f43-4097-9d05-8ad14d337cbc/Build/Server。它们不代表Cook、真实WorldReady或功能体验验收通过。



## 2026-10-09 公共流程Retry合同

UI允许Retry时，命令实际调用RetryFailedFlow：取消本流程/后端等待、取消转移、清空连接材料并经Online注销事件重启人工登录。只在无活动运行、不忙、装配完整且错误可恢复时发布Retry；CharacterCreateOutcomeUnknown不发布重试，避免重复创建。true仅表示恢复受理，后续失败/页面投影仍通过既有ViewState事件交付。此改动不涉及Widget/主题/动画资产，实际界面视觉仍须既有Monolith流程验收。
