# SSLM 全插件体系优化实施计划 V2.0（2026-10-18）

> 依据：PluginQualityOptimizationDesign_V2_20261018.md（优化设计），CurrentPluginQualityMatrix.json（67插件静态真实台账）、RuntimeSafetyReview_20261018.md（已核实运行风险）及AGENTS.md（强制规则）。
> 本计划只建立交付任务与验收标准，尚未通过UE编辑器、Monolith（虚幻编辑器自动化工具）、真实Cook和人工签收。执行者不得因计划存在将任务自动设置为完成。
> 唯一工程：DivineBeastsWorkspace/Game/DivineBeastsArena.uproject。保留现有三层架构和67插件稳定身份，不允许为了清零缺口新增空插件、空地图、平行播放器或新的人工审核框架。

## 一、实施顺序与责任边界

采用`M0—M7`八批依赖式交付。P0=权威、安全、端侧与构建阻断；P1=核心功能和人工操作缺口；P2=性能热点候选与非核心测试；P3=体验/长期治理。当前质量台账的51项P1代表缺正式案例，不是51处代码故障；碰到新发现的P0可跨批次优先整改。所有性能修改先建对照基线，不能只以Tick计数作为证据。

| 批次 | 优先级/受影响插件 | 实施范围（不重复造框架） | 阶段结束条件 |
|---|---|---|---|
| M0 现状冻结 | 所有67插件、89模块、19内容包 | 核对HEAD/未提交文件、唯一注册表、作者编目、三层依赖；整理每个插件的功能候选分母与消费路径 | 有当前67条完整台账和可追溯的未审项；不把未知设为已通过 |
| M1 生命周期安全 | Session、Online、Server、Data、ApplicationFlow、Telemetry、World、PCG、VFX | 同步失败/取消、代次、所有权、Ticker、回调清理、世界退出、租约及资源边界 | 受影响真实源码与原生测试、正常/异常/取消/旧回调四路径通过 |
| M2 双端构建与世界 | DBAServer、DBAWorlds、GamePlatformWorld、PCG、Water、Weather、Arena、VFX、UI | Client/Server/Editor依赖闭包、权威Cook、三服务器角色、主竞技场五种人数模式 | 最新版本的独立Client/Server构建、真实Cook清单及角色配置验证 |
| M3 演示与审核闭环 | GamePlatformDeveloperTools、GamePlatformUI、DBAClient、DBAUIPack_Core | 复用现有DemoHub和ReviewCase/Report，完成版本、状态、参数、操作、报告和人工签收工作区 | 真实UI资源由Monolith制作、编译保存冷重载；独立人工签收流程可操作 |
| M4 首批正式视觉样板 | UI、VFX、PCG、Camera、SFX、Water、Weather、Surface、Presentation及对应项目内容 | 选既有真实诊断地图，补公开服务接线、操作参数、停止/重置/返回、自动/人工证据 | 每个正式样板至少具有一次可复现运行/失败/取消及人工待审报告 |
| M5 玩法与场景 | Combat、AbilitySystem、Character、Animation、AI、Interaction、Quest、Inventory、Equipment、Gameplay、Navigation、Input、Arena、DBAGameplay、DBAWorlds | 权威事实、生命周期、角色、AI、交互、玩法HUD、任务、道具与跨世界场景 | 开发地图中的操作只改变合法测试状态；服务器验证权威结果与重入 |
| M6 全部内容包 | 12个DBAHeroPack、Common、FrontEnd、UIPack、Presentation、SFXPack、Village、MainArena | 真实资产消费者、主资产软引用、蓝图父类、资源许可、画面、释放与目标Cook归属 | 19内容包逐包记录可加载、消费者、素材来源、端侧差异和人工证据 |
| M7 全量集成与签收 | 67插件+三服务器角色+已批准业务功能 | 版本化全部门禁、真实性能、场景/多人/设备复测、集中人工审核和发布资格 | 所有“必须项”获得当前版本真证据；剩余缺口明确阻断而不强行发布 |

**实施依赖**：M0→M1/M2；M1→M3；M3先完成可操作骨架才能进入M4正式案例；M4样板稳定后M5/M6滚动扩展；M7最后统一发布验证。不要求为进行源码和合同审查提前完成人工签收，但发布的对应必需案例不得绕过真人审核。

## 二、详细 WBS（工作分解）及验收产物

### W00　插件身份、成熟度与当前版本证据（M0）

主文件：Docs/Implementation/PluginQualityAudit/CurrentPluginQualityMatrix.json（现有只读派生台账）、PluginDeliveryManifest.json（交付身份真源）、PluginDemoManifest.json（演示作者真源）。

- W00.1 核对67个真实.uplugin、89个Build.cs模块及19个内容包身份；模块用途按Runtime（双端运行）、Client（客户端）、Server（专服）、Editor（编辑器）标注。保留当前已审的41平台/2MOBA/24项目身份分类，禁止跨层反向依赖。
- W00.2 对每个插件列出真实Public API（公开接口）、默认配置、调用方、资源资产、运行作用域、异常/取消、实际模块内测试和未覆盖的功能分母。仅登记真实存在或有业务批准的能力。
- W00.3 对GamePlatformLobby（通用大厅）、GamePlatformVillage（通用村庄）、GamePlatformLocalization（本地化）等保留模块明确现有消费者与欠缺能力；不能把IMPLEMENT_MODULE（模块注册）认作正式功能完成。
- W00.4 把上一轮已发现的三类问题（Session取消、Telemetry重试、VFX鸟群）保留已修源码与待实跑状态，后续任务不得重复覆盖其并行修改。

验收：编目唯一、无缺Build.cs、层级方向无环；功能未知时明确`未审`，不能只用文件数作为功能完成率。

### W01　同步回调与状态机安全（M1）

- W01.1 GamePlatformSession（会话）复核BeginTransfer同步失败、SessionChanged中取消、OnBindingPrepared/OnTravelCommitted/OnFact同步完成、ClientTravel和握手Ticker后置校验；旧操作不能重新变Ready。先保留当前修复和原生用例，再在实际UE Editor Automation中运行。
- W01.2 GamePlatformData（唯一数据加载）核对租约Owner、共享请求合并、内容目录重复、异步发布代次、世界释放不误卸载其他用户需求。
- W01.3 GamePlatformTelemetry（遥测）核对Pending容量、RetryAfterSeconds（重试等待时间）、一次性句柄触发即移除、重复Shutdown预算唯一、网络断开/恢复/长时待处理时的计数与内存；现有新增RetryHandleLifecycle与ShutdownIdempotent两原生用例尚未实跑。
- W01.4 GamePlatformPCG/World/Weather（世界与天气）核对闲时Ticker、世界退出、重入Observer（订阅者）和正式地图结果；不允许重建第二套调度器。
- W01.5 GamePlatformVFX（特效）核对BirdFlock（鸟群）实例化网格更新率、停止释放、10/20/60Hz案例、Niagara权限/取消、效果类型和池化；不能把每鸟独立Actor作为规模方案。

验收：每项高风险路径至少有正常、失败、取消、代次过期、世界退出的适用测试；静态合同与正式UE Automation状态独立显示。

### W02　客户端/专服与三世界内容（M2）

- W02.1 使用锁定UE5.8与现有Build/Game/BuildFoundation.ps1按最小变更增量构建，不换引擎、不Clean/Rebuild无关模块、不因失败移除插件。Editor与Client/Server使用当前AGENTS批准参数，并记录目标、源码/内容指纹、原生日志、退出码。
- W02.2 验证Server端只拥有碰撞、导航、出生、角色权威与准入所必需的数据/动画；审核SFX、UI、VFX、Surface、Camera、Demo和编辑器资产没有误进入Dedicated Server真Cook（非仅配置检查）。
- W02.3 OpenWorld（常驻世界）、Village（新手村）、MainArena（竞技场）分别验证地图/游戏模式/目标关系及最小启动。竞技1v1至5v5共用主竞技服务器；训练是Village体验，不创建新角色。
- W02.4 PCG（程序化生成）必须区分图/蓝图存在、UE真实生成、静态烘焙、导航碰撞、AssetRegistry读取、Client/Server Cook。OpenWorld缺正式内容包时先完成经批准的地图设计，不能以空插件冒充覆盖。
- W02.5 测试连接断开与重入后内容租约、UI、Niagara、世界实例是否按各自Owner释放。

验收：真实目标构建日志+真实Cook产物索引+服务器黑名单排除比对；无上述材料时`Cook待验证`。

### W03　唯一DemoHub与人工审核页面（M3）

UI全部在原DivineBeastsDemoClient（SSLM演示客户端）的DemoHub及GamePlatformDeveloperTools（平台开发审核工具）增量设计，不造第二套UE编辑器工具。页面建议拆成`插件质量总览`、`功能/案例目录`、`真实操作台`、`测试与性能面板`、`Editor人工审核台`五个逻辑页面，可继续复用既有页面、Widget和ViewModel，而不是强制创建五张地图。

需要的操作合同：
- 列表：稳定插件ID、层与模块、是否启用、功能已审数/总数或`未知`、端侧、诊断地图/正式案例、已测/待审、问题严重度；搜索可用中文及稳定英文身份。
- 正式案例：参数表、校验提示、前置依赖检查、`预检/启动/执行/停止/重置/返回`；只有真实功能支持的控制才可启用。所有按钮异步失败应展示错误，不把`请求已接受`当`功能执行成功`。
- 场景区：当前World、所加载组件、真实特效/物体/音频/角色及实验参数；不可用的资源显示`未绑定/不可加载`而非占位假图。
- 性能区：运行设备、CPU/GPU耗时P50/P95/P99、实例数、DrawCall、显存、目标画质；没有当前量测直接显示`未测`，不默认判绿。
- 人工审核台：只在Editor经平台UGamePlatformReviewSubsystem（审核子系统）产生待审报告，审核人需实名确认、校验当前版本必需证据；Passed/Failed/Blocked追加修订，不覆盖旧记录。
- 交互：使用事件驱动、UI行虚拟化、键盘/手柄焦点、适配移动端安全区。所有项目视觉Widget Blueprint由Monolith制作、编译、保存、冷重载，不用文本假.uasset。

**优先三项正式操作样板**（仅候选身份，登记前核对Schema2唯一性，不声称已经创建）：`UI.LayerFocusAndReopen`（层栈/输入焦点）、`VFX.IceArrowCastImpact`（冰箭施法/命中与质量控制）、`PCG.GoldCropGenerateAndBake`（Gold农田生成/排除/复现）。每项必须包含正常、失败、停止与场景退出；实际资产和数据准备失败时保留阻断状态。

验收：最新UE客户端可打开入口并返回；正式案例具备实际Adapter（运行适配器）与可复现状态改变，Edit/Apply显式审核的Report（报告）和Evidence（证据）均由当前版本真源生成；不允许AI或脚本代签。

### W04　表现与环境批次（M4）

先完成三样板，再按复用程度接入：GamePlatformSFX（音效）、Camera（镜头）、Water（水体）、Weather（天气）、Surface（环境表面）、Animation（动画）、Presentation（表现请求）、MobaPresentation（竞技表现）、GamePlatformUI（基础界面）。同一诊断地图允许被多个域使用，但每个域必须有独立场景步骤和签收归属。

视觉审核覆盖：摄像机角度/焦距/视野、当前实际PBR材质、加载/淡出、音频启动与停止、天气过渡、海拔与边界、实例数量、画质降级、资源失败回退。正式颜色以SSLM全局美术V3和UI策略为唯一真源；正确sRGB/线性对照及真实客户端采样，不使用Monolith 0.23已知异常widget截图颜色路径。

验收：人工可以真正操作，能够回退至初始场景且没有其他世界残留；画质变体需有真实截图+性能条件，不能靠说明图片。

### W05　权威玩法与角色（M5）

优先在隔离测试World中组织`角色→移动/动画→能力→战斗→交互/任务→背包/装备→竞技结算`的真实链路。保留GAS（玩法能力系统）的服务器权威许可及回滚；UI仅展示服务端复制结果，不自行发伤害、装备货币、奖励或比赛结果。

涉及插件：GamePlatformCharacter、Animation、AbilitySystem、Combat、Gameplay、AI、Interaction、Quest、Inventory、Equipment、Input、Navigation、Progression、DBAGameplay、DBAArena、GamePlatformArena。合同需验证多人、出生、换角色、技能撤销、导航阻塞、异常资源与重入，以及输入设备变化。任何真实业务数据修改只能用隔离服务或合法补偿，复位不得删除真实账本或玩家数据。

验收：UE独立客户端/专服双端演示真实权威结果；视觉与游戏逻辑可相互对应，服务取消/地图切换没有悬挂状态。

### W06　19个内容包人工验收（M6）

12个DBAHeroPack英雄内容包可共用同一`模型/动画/技能/效果/材质/LOD/碰撞/服务器引用`审核模板，不必建12张空地图；每个包分别产生其实际资产清单、消费者、软引用、许可、加载/释放、画面和签收。

其他7类项目内容：DBAContentPack_Common（公共角色/素材）、DBAFrontEndPack（登录/角色编辑器）、DBAUIPack_Core（UI及主题）、DBAPresentationPack_Core（公共表现/VFX）、DBASFXPack_Core（公共音效）、DBAWorldPack_Village（新手村）、DBAWorldPack_MainArena（竞技场）。这些包内部部分未加载的外置Actor/动画/Widget必须由UE原生AssetRegistry及可视化场景检查，不仅靠文件头。未批准OpenWorld正式地图不应创建空包填补数量。

验收：逐包与真实资产一致、视觉/音效合法、项目/平台依赖下行、Server Cook按职责拆分；存在但尚未接线的Asset标`待消费者验证`。

### W07　发布前全系统回归（M7）

- 静态：67身份/89模块、依赖环、目标装配、中文注释、模块公开API、Demo编目与工作单。
- 编译：受影响模块→完整Editor增量→真实Client/Server增量→相应Cook/Stage，均保存当前RunId，禁止引用旧HEAD二进制。
- 原生：会话、遥测、鸟群、PCG、技能、UI、战斗与各功能的正常/失败/取消/跨世界自动化。
- 真实性能：固定场景与设备，MainArena 1v1—5v5、Village、OpenWorld、四画质、多人特效并发、切世界、10分钟稳定及断线重连。
- 人工：必须由真实审核者操作和签收现有ReviewCase（人工审核用例）；允许失败或阻断报告作为事实保存，不能自动清零剩余问题。

验收：现有G0—G5总门禁按各自已实现规则汇总，尤其G4人工通过及当前版本证据；任一必需项未审则ProductionReady（生产就绪）为false。

## 三、统一工作单字段与交付模板

每次仅选一插件/功能或受控少量关联插件执行，不大包大揽。工作单应包含：

| 字段 | 核查目标 |
|---|---|
| WorkOrderId（工作单号） | 稳定编号与对应M0—M7阶段、P0—P3风险 |
| Plugin/Module/Target（插件/模块/端侧） | 真实.uplugin身份、唯一模块及Client/Server/Editor |
| VerifiedSource（真实源） | 当前源码路径、Public API、真实消费者、现有测试与资产 |
| Scope/Owner（作用域/所有权） | World、LocalPlayer、GameInstance、租约、资源与时间 |
| Defect/Change（问题与最小修改） | 可复现行为、根因、不变量、准确文件/函数 |
| TestCases（测试案例） | 正常、异常、取消、旧代次、清理、网络/性能适用项 |
| VisualUI（可视化作业） | 已有真实场景、原生Widget/Adapter、允许的操作 |
| Evidence（证据） | 当前源码、原生编译、测试、日志、截图、真Cook、性能、人工结论 |
| Rollback（回退边界） | 修改前差异、旧接口兼容、资产只由引擎回退 |
| Status/Blocker（状态/阻断） | 未审/部分/通过/失败/阻断/当前版本过期 |

`“有测试源码”`只代表可运行的设计基础，不等于该工作单测试已在真实UE执行；工作单必须记录运行命令、退出码、原生日志或明确未运行原因。

### 3.1 全量67插件工作单与当前设计快照

本方案已由真实`CurrentPluginQualityMatrix.json`（静态质量台账）派生并保存`PluginOptimizationWorkOrders_V2.json`（机器可审工作单）和`PluginOptimizationWorkOrders_V2.md`（中文工作单表），可直接按插件执行和验收。它们仅用于**规划任务**，不改`PluginDemoManifest.json`（正式演示作者源），不生成UE二进制资产或人工通过记录。

- **当前分批分配**：M1 11插件、M2 9插件、M3 8插件、M4 7插件、M5 13插件、M6 19内容包；M0与M7对所有67插件横向生效。
- **优先推进的首批真实工作单**：`SSLM-PLG-GamePlatformSession`（会话同步取消、双端旅行）、`SSLM-PLG-GamePlatformTelemetry`（遥测重试内存/Shutdown）、`SSLM-PLG-GamePlatformPCG`（Gold真实生成与三世界烘焙）、`SSLM-PLG-DBAClient`（唯一人工演示入口及证据链）、`SSLM-PLG-GamePlatformVFX`（Niagara与鸟群的画面/性能）。按各工作单主批次、依赖和现有编辑器资源安全条件执行，不要求它们并发运行。
- **正式视觉样板的能力判定**：UI层栈/焦点、冰箭VFX、Gold PCG等案例只有在真实消费者、操作参数、启动/停止/重置/返回及原生证据准备完毕后，才允许登记为`可启动`；旧13张诊断地图仍只是观察入口。
- **复现与防漂移命令**：`python Tests/Architecture/GeneratePluginOptimizationWorkOrders.py --check --summary`（只读核对）；当源台账真实发生经过审查的变化时才显式使用`--write`再生成两份派生工作单，并运行`python Tests/Architecture/TestPluginOptimizationWorkOrders.py`（对应回归）。
- **当前证据状态**：67份工作单均为未实施、人工审核未完成；不覆盖先前已实际修复的代码记录，另在工作单`alreadyChangedNotes`（已知先前整改）注明需要最新UE验证的事实。

## 四、推荐的首轮并行策略与停机条件

- 可以并行：不同插件的静态审查、定义/文档、纯值单元回归、无资源写操作的测试。
- 必须串行：同一个UE编辑器实例/Monolith、同一蓝图或地图包、共享Config、全目标UBT、正式Cook/Stage、当前审核证据的Publish（提升）。
- UE已有其他任务正在加载原生资源时，保留它的资产与源码，先完成独立的代码设计/低风险静态校验；不能强杀、抢占、覆盖或反复排队重复任务。
- 接口、结构或测试不满足时直接记录该插件缺口，不为了让菜单“好看”生成空Provider/虚构资源；第三方资产权限和正式艺术批准不明时保持阻断。

## 五、可直接用于后续AI执行的工作提示词

> 你是SSLM项目的UE5.8三层插件架构、性能与原生资源实施工程师。使用当前唯一DivineBeastsWorkspace工作空间，先阅读AGENTS.md及PluginQualityOptimizationDesign_V2_20261018.md，核对真实插件清单、工作树和运行中UE任务。按本计划当前阶段选择有明确复现证据且未被其他会话占用的最优先工作单；先读取真实接口和调用链，再以最小改动实施，中文注释必须说明所有权、取消、失败和回退。所有可视化任务复用DemoHub及ReviewHarness，项目Widget蓝图必须通过Monolith，禁止文本伪造UE资产、空演示或人工通过。只执行本次改动的真实可用构建/回归，分别报告静态、C++语法、正式UBT、UE原生自动化、Cook、性能和人工状态，并更新作者编目与当前工作单。不自行修改已发布模块/反射身份，不并发写同一地图，不停止别的任务，不擅自提交主分支；遇到客观阻断时保留已完成的真实成果及精确证据。

## 六、风险与资源安排建议

优先由负责运行/网络的工程师处理M1/M2，再由技术美术与客户端UI工程师处理M3/M4，角色/玩法开发负责M5，资源技术美术负责M6，质量负责人组织M7。单人开发时使用同一顺序而非同时编译多个宿主。人日/成本及FPS达标比例必须在真实功能分母、设备清单、资产成熟度和性能基线明确后估算；本设计不捏造成本和交付日期。

**当前状态：仅交付优化设计及实施规范，未创建/修改UE二进制资产，未更新PluginDemoManifest作者源，也未替任何案例填写人工签收。**
