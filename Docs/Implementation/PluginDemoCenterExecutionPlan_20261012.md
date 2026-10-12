# 《神兽联盟》统一插件演示中心实施执行计划（V1.0）

> 编制日期：2026-10-12  
> 工程：DivineBeastsWorkspace（神兽联盟工作空间），Game/DivineBeastsArena.uproject（正式主工程）  
> 引擎：Unreal Engine 5.8（虚幻引擎5.8）  
> 状态：**待实施**。本文件是实施计划，并不代表其中代码、蓝图、地图或测试已完成。

## 1. 目标与完成口径

目标：开发者在编辑器中一键进入 Plugin Demo Hub（插件演示中心），也能通过显式开发客户端模式直接进入，**无须登录账号、创建或选择角色、连接生产认证后端或进入新手村**。总览按平台层、MOBA通用层、项目层展示插件，支持搜索、功能列表、可交互真场景、参数面板、重置、返回、日志、性能和人工验收。

覆盖门槛：
- 当前47个代码插件、17个内容包必须全部登记在总览；内容包与代码插件分别统计。
- 每个具有视觉、听觉或交互功能的插件，其每项**实际已实现且可展示的功能**均应具备明确入口、真实效果、至少一种有效操作或触发方式、复位方式和验收项。多个功能可复用同一地图，但不能用一个静态综合页面代替逐项展示。
- 逻辑/服务型插件提供真实状态或编辑器诊断演示，不强行建立三维场景；本地模拟、测试夹具、服务器权威和真实联机必须分别标记。
- 未实现的功能、缺失资源、被阻塞项目如实显示状态，不能以占位资产、固定成功、空按钮冒充已交付。
- 完成状态必须同时具备实际运行证据和人工审核结论，不能以插件卡片数量、地图数量或自动测试通过代替美术验收。
- 非开发客户端与正式服务器启动/资源装配保持不变，演示系统绝不污染 Shipping（发行）及 Dedicated Server（专用服务器）。

## 2. 项目已核实基线

- 源码基线包含47个代码插件（GamePlatform平台40、MobaCommon通用2、DivineBeasts项目5）和17个内容包。正式目标现有 Editor、Client、Server 三类，不能假设独立DevHost工程真实存在。
- Game/Config/DefaultEngine.ini 的 GameDefaultMap（正式默认地图）为 /DBAFrontEndPack/Maps/L_DBA_FrontEnd（登录前端），此值不得替换为演示入口。
- DivineBeastsApplicationUIAdapter::Initialize（应用界面适配器初始化）有首次调用 StartFlow(false)（正式应用流程启动）的路径；单独变更启动地图不能确保绕过登录。
- /Game/Development（开发目录）下已有31张受版本控制的关卡，涉及BirdFlock（鸟群）、Weather（天气）、Fauna（蝴蝶鱼群）、FallingFoliage（花瓣叶片）、MossSnow（苔藓积雪）、Terrain（地形）等，部分为技术探针，必须逐个审查复用资格。
- GamePlatformDeveloperTools（平台开发者工具）是Editor模块；GamePlatformUI（平台UI）提供客户端控件基类；GamePlatformData（平台数据）负责唯一资产身份、异步加载与租约；DBAClient（项目客户端）拥有现有项目UI与应用流程组合。
- 所有神兽联盟UI蓝图/页面/主题/RootLayout（根布局）必须使用锁定编辑器的 Monolith MCP（编辑器操作连接）生成、修改、编译、保存、回读；.uasset/.umap禁止通过脚本伪造。
- 当前主工作树含其他会话未提交修改及活跃作业；开始前先检查git和进程/编辑器身份，文件责任明确后实施，绝不重置/清理他人改动。

## 3. 架构与技术决策

继续遵守 DivineBeasts（项目层）→MobaCommon（MOBA通用层）→GamePlatform（平台层）的单向依赖；演示中心是开发装配入口，不是第四层，不新增第48个代码插件。

**双入口、单目录**：A为 GamePlatformDeveloperTools 上的Editor工具菜单及 Editor Utility Widget（编辑器实用控件），点击后进入PIE（编辑器内运行）的演示中心；B为带 -DBAPluginDemo（项目插件演示标志）的显式非发行开发客户端，直接从开发专用地图启动。入口共享一份场景目录和真实演示资产；正式GameDefaultMap不变。

**模式隔离**：演示模式必须在应用流程任何副作用前被解析、冻结并校验非Shipping、非Server、非Commandlet（命令工具）、显式允许配置和正确开发地图。除已知 UIAdapter::Initialize 外，审计 DBAGameInstance（游戏实例）、Foundation启动协调器、其他在线/会话自动初始化，确保不会暗中发起认证、角色或正式世界流程。普通客户端无标志时流程保持原样。

**代码归属**：优先在 DBAClient 内规划 DivineBeastsDemoClient（神兽联盟开发演示客户端模块），但在P0首先验证 UE5.8 模块 TargetConfigurationAllowList（目标构建配置白名单）等真实能力，仅允许开发配置；不满足安全隔离则转为现有项目模块的受控条件编译/编辑器扩展。Editor工具保持平台中立，项目适配器只依赖被演示插件已公开的真实能力，不复制VFX、天气、动画、战斗、Data资产管理器。

**核心数据**：PluginDescriptor（插件条目）→DemoScenario（功能演示定义）→DemoSession（演示会话）→DemoEvidence（验收证据）。场景定义至少记录 PluginId、FeatureId、ScenarioId、所属层级、类型、地图/Widget/资产软引用、前置条件、参数类型与范围、操作/重置接口、错误与取消、网络/权威要求、人工验收标准。唯一性和有效性由真实.uplugin与AssetRegistry（资产注册表）检查，不维护互相矛盾的双清单。

**作用域和释放**：GameInstance负责短期路由和当前ScenarioId，LocalPlayer负责焦点与输入，World负责场景Actor和特效；异步按世界ID/请求代次验有效性，场景退出取消请求、解绑事件、停止实例、释放本会话的GamePlatformData租约。不强引用已退出世界的Actor，不让一个演示卸载其他游戏实例仍使用的资源。

**五类模板**：UIWidgetGallery（UI控件展厅）、WorldVisualStage（三维视觉实验室）、GameplaySandbox（玩法沙盒）、ServiceDiagnostics（服务诊断台）、EditorAuthoring（编辑器制作工具）。这些是演示模板，不代表额外独立插件或额外系统执行器。

## 4. 分阶段实施WBS（工作分解结构）

### P0：审查与基线冻结

- P0-01 检查最新HEAD、git改动、活跃任务、编辑器/Monolith状态、局部工程规则、构建和Cook（资源烘焙）入口。
- P0-02 扫描47代码插件、17内容包、真实模块和API、资源、蓝图；审查31张开发地图，区分可直接复用、需补交互及仅用于问题定位的探针。
- P0-03 建立“插件→已实现功能→公开接口/真实资源→演示模板→ScenarioId→参数→验收状态”覆盖台账；没有实现的功能保留计划/阻塞状态。
- P0-04 锁定文件归属、数据身份、第三方/Monolith生成约束、开发模块发行排除、与PCG/天气/UI/材质并行任务的冲突边界。
- 交付：FeatureCoverage.md（功能覆盖表）、AssetReuseMap.md（现有关卡复用映射）、PluginDemoManifest.json（目录草案）、风险/文件所有权清单。
- **门禁**：47/17实际身份完整，所有可展示的已实现功能有计划演示或明确阻塞原因。

### P1：直达入口与正式流程隔离（最高优先级）

- P1-01 开发标志与地图校验；Editor/PIE、开发Client、普通Client、Server、Commandlet、Test/Shipping均有显式允许/禁止策略。
- P1-02 审查并修复最早应用启动分支，演示模式不执行 StartFlow(false)、后端认证、角色流程、正式Session准入及世界Travel（跳转）。
- P1-03 新建真实 L_DBA_PluginDemoHub（演示总览地图）及开发专用GameMode（游戏模式）/PlayerController（玩家控制器），错误启动给明确失败提示。
- P1-04 增量增加独立开发Cook配置，正式GameDefaultMap保持不变。
- **门禁**：开发模式无登录和认证请求；去掉标志即恢复正式登录；Server/Shipping不能启动演示。

### P2：统一目录、加载及生命周期

- P2-01 统一扫描/注册 PluginDescriptor 和 DemoScenario，稳定ID检查重复/缺路径/缺依赖，失败关闭而非静默选取。
- P2-02 复用GamePlatformData定义身份、AssetManager（资产管理器）和异步租约；地图、UI、Niagara、声音、材质等按需软加载。
- P2-03 参数数据含整数/浮点/枚举/布尔/资源/预设及合法范围、开始/停止/重置、状态回传、错误/取消语义。
- P2-04 进入、运行、返回、切地图、重复加载、取消、世界销毁时清理本会话资源，拒绝过期异步回调。
- **门禁**：任意两种实际案例可稳定进出，非法数据/缺资产/快速关闭不泄漏、不崩溃。

### P3：总览UI与首个完整交互闭环

- P3-01 Monolith创建 WBP_DBA_PluginDemoHub（插件总览控件蓝图），呈现三层筛选、插件卡片、搜索、功能个数、状态和进入按钮。
- P3-02 Monolith创建 WBP_DBA_PluginDemoDetail（插件功能列表）、WBP_DBA_PluginDemoControls（参数与触发面板）、WBP_DBA_PluginDemoReport（日志和人工验收）；复用现有GamePlatformUI主题、焦点和页面层栈。
- P3-03 实现“总览→插件→功能→真实演示场景→参数操作→重置/停止→返回”；支持Esc、键鼠焦点、1920×1080/2560×1440/3840×2160。
- P3-04 先用 BirdFlock（鸟群）和 Weather（天气）完成两个端到端样板，不创建假的Niagara或天气执行器。
- **门禁**：不用登录可点击两项真实演示并返回；相关Widget经Monolith编译、保存、重新打开验证。

### P4：五类演示模板和控制台

- P4-01 完成UI展厅、3D视觉台、玩法沙盒、服务诊断、编辑器工作台模板，统一交互区、真实资源视口、参数、状态、性能和验收面板。
- P4-02 实现参数输入校验、预设切换、显式应用/重置、启动/暂停/停止、相机/音频/画质控制（按对应插件真实能力）。
- P4-03 通过适配器访问各插件真实公开接口，遵守平台/项目单向依赖；禁止建立平行VFX执行器、天气调度器、物品/伤害权威逻辑。
- P4-04 资源错误、缺插件和不支持的端侧明确告知；保持世界生命周期和UI输入状态一致。
- **门禁**：五类模板有可执行样板或“不适用”说明，循环切换不残留实例或委托。

### P5：视觉、自然环境与材质插件接入

- P5-01 VFX演示覆盖已实现的 Instant（瞬时）、Attached（附着）、Projectile（视觉投射）、Beam（光束）、Area（区域）、Shield（护盾表现）、Portal（传送门表现）、Trail（拖尾）、World（世界环境）、Composite（复合）十种行为；缺资源者保持待完成。
- P5-02 复用 BirdFlockIntegrationReview（鸟群链路）、FaunaReview（生态动物）、FallingFoliageReview（落英）、WeatherReview（天气）、MossSnowReview（积雪苔藓）等现有真实审核地图；技术Probe（探针）只归诊断。
- P5-03 Weather演示晴/雨/雪/状态与转场；Surface演示苔藓/积雪/湿润/岩石PBR；PCG分别演示Editor生成与真实烘焙结果，不能混淆。
- P5-04 SFX/Animation/Camera/Presentation（音效、动画、相机、表现协调）逐现有功能接入音量、姿态、镜头、视觉反馈和暂停/恢复等真实控制。
- **门禁**：每项可展示功能可选择、操控、复位，资源缺失和性能问题真实报告。

### P6：UI、玩法交互及MOBA/项目专属演示

- P6-01 GamePlatformUI/Input/Settings/Loading/Localization/CommerceUI（界面、输入、设置、加载、本地化、商业UI）展示已实现的十大UI领域控件、页面、焦点/主题/状态和响应。
- P6-02 Character/AbilitySystem/Combat/Gameplay/AI/Interaction/Navigation（角色、技能、战斗、玩法、AI、交互、导航）展示移动、角色动画、技能释放、命中反馈、状态改变、AI行动、物体交互和导航。
- P6-03 Inventory/Equipment/Quest/Progression/Entitlement/LiveOps（背包、装备、任务、成长、权益、运营）展示真实状态与受控命令；本地Fixture（测试夹具）不得声称服务端写入成功。
- P6-04 GamePlatformArena、MobaPresentation、DBAArena（通用竞技、竞技表现、项目竞技）展示1v1—5v5模式、组队/竞技HUD和真实表现链；服务器权威和联网验证单独列门禁。
- P6-05 DBAClient、DBAGameplay、DBAWorlds、DBAServer及GamePlatformLobby/Village等分别提供可见功能或真实状态入口，不建立向项目的反向依赖。
- **门禁**：已实现的交互功能逐项有真实行为入口，模拟/联机/生产能力标签准确。

### P7：服务功能与17个内容包完整纳管

- P7-01 Core/Data/ApplicationFlow/Save/Online/Server/Session/Debug/Telemetry/DeveloperTools等（基础/数据/流程/存档/在线/服务器/会话/调试/遥测/开发工具）提供状态、事件、失败/恢复和诊断台。
- P7-02 17个内容包：公共角色、12生肖英雄、前端包、通用表现包、公共UI包和Village世界包，通过真实资产画廊和消费者插件案例展示，不能复制资产或单独实现播放器。
- P7-03 增加CI（持续集成）清单一致性校验：新增插件、功能无Scenario或明确已审核豁免时标缺失；拒绝重复ID、空演示卡、错目标/错层级。
- **门禁**：47/17全部可查，每个条目提供有效演示/诊断入口或清晰缺口、责任和状态。

### P8：集中构建、自动化、人工审核及发行隔离

- P8-01 做Git差异、中文注释、三层依赖、反射/资产引用、目标白名单及资源清单检查。
- P8-02 串行编译Editor/Client/Server，运行UE Automation（自动化）验证启动旁路、正式登录回归、目录校验、异步加载/取消、输入与页面、World生命周期、多次进退。
- P8-03 独立开发客户端实测输入设备、多个分辨率、画质档位、帧耗、内存/显存和长时间连续运行；使用UE原生截图或验证通过的UI色彩采集路径。
- P8-04 人工逐插件、逐功能签署：材质与尺度、动作/音效、按钮与焦点、视角遮挡、资源完整性、性能与交互正确性；无签署不算人工通过。
- P8-05 实际干净Cook/Stage（资源烘焙/暂存）审计正式Client和Dedicated Server，禁止演示地图/演示UI、纯表现资源误入服务端；开发包仅在显式自定义配置纳入必需资源。
- P8-06 形成真实命令、目标、版本、退出码、报告路径和回退信息，不擅自提交/推送或清理并行任务文件。
- **门禁**：两种开发入口直达、正式业务流程无回归、47/17目录完整、每项真实视觉/交互功能有演示与状态、人工审核可追溯、发行产物无开发污染。

## 5. 推荐交付批次

**M1：P0—P3**。先交付可运行总览、鸟群与天气两项真实演示、参数操作/重置、返回和截图/日志，作为所有后续插件的标准样板。

**M2：P4—P5**。统一五类模板，批量接入VFX、SFX、天气、环境表面、PCG、动画、相机等视觉能力。

**M3：P6**。接入UI、角色、技能、战斗、AI、交互、背包/任务、竞技和项目专属可视化。

**M4：P7—P8**。补齐逻辑服务和17个内容包，完成47/17覆盖率、构建、真实Cook和人工验收。

依赖顺序：P0→P1→P2→P3→P4→P5/P6/P7→P8。P5/P6/P7可按**互不重叠文件**分工；Monolith操作、共享Config/Target修改、地图保存、构建和Cook需串行协调。

## 6. 计划目标路径（非已创建）

- Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/：项目开发演示模块（是否创建以P0白名单验证为前提）。
- Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Source/：平台中立的编辑器入口与质量检查。
- Game/Content/Development/PluginDemos/Maps/：演示总览与共享实验室地图。
- Game/Content/Development/PluginDemos/UI/：通过Monolith生成的演示界面。
- Game/Content/Development/PluginDemos/Definitions/：通过真实UE资产工具创建的演示定义。
- Docs/Implementation/PluginDemos/PluginDemoManifest.json：插件及案例导出/核验清单。
- Docs/Implementation/PluginDemos/FeatureCoverage.md：插件-功能-演示-验收台账。
- Docs/Implementation/PluginDemos/ManualAcceptance.md：人工审核记录。
- Saved/Validation/PluginDemos/：运行产生的报告与截图证据。

## 7. 风险、保护与最终完成标准

高风险：隐藏登录但后台仍调用StartFlow；开发模块或关卡污染Shipping/Server；演示复制假逻辑；硬加载全部高精度资源；跨世界回调或租约残留；并行PCG/天气/材质/UI任务之间的文件冲突；把本地沙盒认作联网权威完成。

控制：最早副作用前冻结模式、模块/配置/Cook三重隔离、真实服务调用链、按需软加载、世界与请求代次校验、Monolith串行保存、保存工作树基线、细分本地和联网验收。失败必须留证，禁止固定成功或强制清理已有任务。

**最终验收**：①编辑器和开发客户端无登录直达；②47个代码插件、17个内容包可查；③每项已实现视觉/交互功能均可实际触发并展示；④每项具备参数/复位和人工审核；⑤未完成/阻塞公开可追溯；⑥正式登录无回归；⑦实际构建和正式Cook证明没有开发演示污染。

本轮只制定执行计划。正式实施从P0重新核实最新工程状态开始，先完成代码与真实蓝图/资源，再集中执行完整自动化和人工验收。