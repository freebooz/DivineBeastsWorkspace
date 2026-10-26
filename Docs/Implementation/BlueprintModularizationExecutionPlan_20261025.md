# SSLM 蓝图模块化优化整改执行计划（2026-10-25）

> 工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（SSLM 主工程）  
> 基线：`main` 分支 `b001de48`；本次开始时工作树有 252 项改动，包括其他任务。  
> 范围：全部项目和内容插件的蓝图及其承载 C++，重点为 UI、技能与场景复用。未经确认，不更名/删除已发布蓝图、玩法标签、主资产身份。  
> 原则：优先低耦合、小范围、可回退；不为降低文件行数重复造服务框架，不修改服务器权威玩法时序。

## 一、质量目标与验收口径

1. **职责单一**：正式 Widget Blueprint（界面控件蓝图）只负责展示和命令入口；Page（页面）负责状态订阅；Presenter（展示适配器）负责槽位内容映射；数据只来自平台客户端背包快照，禁用业务轮询 Tick（逐帧更新）。
2. **复用**：技能、背包、快捷栏可共享平台通用 SlotWidget（槽位控件）合同和可配置样式，但不同业务语义不强制共用一个资源；拥有双份同名资源时先分析消费者，不以文件名一致作为可删除依据。
3. **三层边界**：平台层 → 通用竞技可选扩展 → 项目层；不得有下层到项目层的依赖；客户端 UI 不反向改变 Gameplay（游戏玩法）权威状态。
4. **复杂度**：盘点全部原生 Blueprint、WidgetBlueprint、AnimBlueprint、技能资产和 PCG Graph（程序化图），导出每资产父类、事件图节点、函数、宏、Tick、硬软依赖、消费者。将高连接度/高事件图节点数列为复核候选，不能直接以 `.uasset` 大小判定复杂。
5. **不回退**：登录→选角→新手村、背包分页/移动/拆分/合并/快捷栏、八槽技能显示/冷却、角色换代/地图切换、组队/竞技、专服内容隔离必须保持原语义。
6. **证据分级**：源代码静态检查、模块编译、UE 原生蓝图编译/冷重载、PIE（编辑器内运行）、Client/Server（客户端/专服）、Cook（资源烘焙）及人工审核分别记账，缺一不可冒充已完成。

## 二、分阶段执行与门禁

### M0 基线冻结与冲突隔离（P0）
- [x] 检查 `AGENTS.md`、三层依赖与工程路径；当前 `main` 工作树较脏，保留其他任务的 252 项变动。
- [x] 先查待修改路径 `git status --porcelain`。发现 `DivineBeastsRootLayout.cpp` 已由其他任务修改，因此暂时不触碰此文件。
- [x] 记录上一轮审查发现的 6 组同名资源、85 个 `WBP_`、45 个 `BP_`、6 个 `ABP_`、205 个 `GA_`、252 个 `GE_` 文件名前缀候选。这不是全量按原生类型核实后的蓝图总数。
- [x] 已执行三层 C++ 继承检查：586 公开头、1271 类型、212 继承边，检查通过；不能代替蓝图资产引用检查。

### M1 背包展示模块化（P1，本轮先执行）
- [x] 已抽取 `FDivineBeastsInventoryCellPresenter`（背包格展示适配器）纯展示函数，统一背包格与快捷栏的名称、数量、待处理标记、选中态和布局；没有另建数据缓存或业务写入。
- [x] `UDivineBeastsInventoryScreen` 的稳定 UCLASS（虚幻反射类）身份、对外 Blueprint（蓝图）接口、背包客户端权威命令、事件订阅与资源请求保持现状，页面格子渲染代码已委托新适配器。
- [x] 已新增 `DivineBeastsInventoryCellPresenterTests.cpp`（背包格自动化测试源码），覆盖空槽/占用/图标有无/数量/未决/选中/快捷栏/越界；**测试源码已写，UE 原生运行未完成**。
- [x] M1 修改后 `DivineBeastsUIClient`（项目 UI 客户端模块）Client 的 `OnlyModules`（仅模块）增量构建退出 0，证据：`Saved/Validation/FoundationM0/4931e8f1-ec3e-4a56-89ff-800e90e0f2f8/Build`；**M3 后最新组合源码再编译被他人 Foundation 锁阻断，仍须复核**。
- [ ] 由 Monolith 在真实编辑器确认 `WBP_DBA_UI_Inventory` 控件树、命名控件、焦点和动态视觉表现；本轮如 Monolith 不可用则标记为阻塞，不通过文件伪造蓝图。

### M2 重复蓝图消费链核查（P1）
- [x] 已核对两种技能槽的离线索引节点和静态消费者：Combat（战斗版）是技能栏硬引用的正式资产，Components（通用组件版）保留历史合同。**当前编辑器父类、软引用和 Cook 仍待 M4**，不删除旧版本。
- [x] 已确认两套 `WBP_DBA_PluginDemoHub`（插件演示总览界面）分别有不同的 C++ 实际消费者，保留两者，后续再核对运行模式与发布剥离。
- [x] 已用 Monolith 离线资产索引复核 Village（新手村）的四组水体/PCG 同名文件：旧 `Development` 路径全部为 `ObjectRedirector`（对象重定向器），新路径才是实际 Blueprint/PCGGraph（蓝图/程序化图）；不是四套重复实现。**不删除重定向器**。
- [ ] 通过 Monolith 执行所需 Widget 资产修改、重新编译、保存和重载；所有变更需人工视觉核验，不作无依据批量合并。

### M3 UI 组合根与技能栏职责拆分（P2）
- [ ] 待并行任务释放后复核 `DivineBeastsRootLayout.cpp`（界面根布局）；逐步把世界 HUD 路由选择、战斗状态投影、施法展示、生命条适配从布局层委托给既有领域适配器，保持原有生命周期。
- [x] 第一子批已从 `DivineBeastsAbilityBarViewModel.cpp`（技能栏视图模型）抽出 `FDivineBeastsAbilityBarProjectionDiff`（技能栏投影差异比较），保留0.0001遮罩、0.01秒详情时间容差与事件通知；新增纯逻辑自动化测试源码。**仍需补编译与原生测试；资源解析和GAS生命周期未动，后续子批待排期**。
- [ ] 对 `DivineBeastsUIClientSubsystem.cpp`（界面客户端子系统）只抽出无副作用协调帮助对象，保留唯一 LocalPlayer（本地玩家）组合入口。
- [ ] 将登录测试账号界面从正式登录页面抽成开发专用组件，仍保持非 Shipping（正式发布）条件限制和口令清除规则。

### M4 全量原生蓝图审计与发布门禁（P1/P2）
- [ ] 解决 Monolith 实时查询不可用问题并排除 `DivineBeastsArenaClient` 模块加载失败阻断，不关闭或重启其他任务编辑器。
- [x] 已实现并运行 `Tests/Architecture/ReportBlueprintModularization.ps1`（蓝图模块化静态报告工具），生成 `Saved/Validation/BlueprintModularization/20261025/BlueprintDuplicateAssetAudit.json`：4248资产文件、609命名候选、6组同名，其中4组为重定向器。**这是静态与离线索引阶段，非完整 AssetRegistry/Monolith 图表审计；原生图表节点、完整软引用和依赖采集仍待完成**。
- [ ] 验证原生蓝图编译/保存/独立重载与界面输入、焦点、响应式布局，三角色和五模式回归、Dedicated Server（专用服务器）不包含纯表现资产。
- [ ] 仅在回归完整且回退方案获批准后处理旧资产，不直接删除、不盲目执行 FixUpRedirectors（重定向器清理）。

## 三、验收标准与回退

- M1：纯展示函数单测通过，`InventoryScreen` 对外函数/反射身份与既有 Widget 名称不变；无新 `Tick`、同步资源加载、服务端数据写入；必要时复原局部 C++ 差异即可回退。
- M2：形成实际引用清单，确认保留/迁移/删除的逐资产裁定；未完成 Monolith 冷重载前旧资产不删除。
- M3：每拆分一项，对比前后事件订阅数量、解绑、世界代次、交互结果并做精准回归，避免“功能先拆，运行后测”。
- M4：源代码、蓝图资产、编译、PIE、联机、Cook、人工审查分别具备可追溯证据；不得宣称仅凭静态审查完全达标。

## 四、执行安全边界

- 本轮原位修改仅限已验证无他人改动的相关文本源码、测试、实施文档；对当前脏文件先标记暂缓。
- 正在运行的其他 Editor（虚幻编辑器）、UnrealBuildTool（构建工具）及长时间作业保持不变。
- 用户要求“按计划执行”授权分期实施，但不等同于允许大规模删除/重命名资产、覆盖其他任务或未经验证合并到生产发布。
- 每轮记录：目标、文件、方法、实测结果、失败与遗留风险；不执行 Git 提交或推送，除非另行提出。

## 五、本轮执行状态

**已执行：** M0基线隔离；M1背包展示组件化与测试源码；M2重名资源/消费者复核、资产裁定台账；M3技能栏投影差异首批解耦与测试源码；M4静态报告工具和完整重名资源报告。相关裁定详见 `Docs/Implementation/BlueprintModularizationAssetDecisions_20261025.md`。

**已验证：** M1后 Client（客户端）目标的 UI 模块定向增量编译通过（exit 0）；静态审计工具执行通过（exit 0），全局 `git diff --check` 无错误，仅其他工作树文件有 LF/CRLF 提示。

**当前阻断：** 并行 Foundation 构建/烘焙占用引擎锁，后续 Editor（编辑器）模块编译和最新 M3 合并后的 Client 复编译均返回 exit 2/NotExecuted（未执行），不能冒充测试通过；HWebCodex 内的 Monolith 服务注册列表仍为空；此前 UE 命令行补扫因 `DivineBeastsArenaClient` 加载失败终止；当前根布局源码另有并行任务改动。未更改 `.uasset`/`.umap`，未执行 PIE、Cook 或人工签收，也没有 Git 提交/推送。

**后续优先级：** 先排除并行构建锁和编辑器模块装载问题→补最新 Client/Editor 编译及两份新测试原生执行→Monolith 逐资产读取和冷重载→再推进技能槽 UI 资产与根布局重构，最后三世界/竞技/服务器剥离验收。

## 六、M5 公用组件基类优先改造（2026-10-25 同日续行）

本阶段按用户“所有通用功能均考虑设计为公用组件基类”的要求执行，详见 [公共组件优化实施记录](BlueprintPublicComponentContinuation_20261025.md)；长期唯一规范已增补至 [三层类继承与扩展规范](../Architecture/三层类继承与扩展规范.md) 第九章。

- [x] 平台已有 `UGamePlatformComponentWidget`（公共组件）及 `UGamePlatformSlotWidget`（公共槽位基类），选择扩展现有实现，不再新建平行槽位UObject类型。
- [x] 平台槽位公共化了状态等价、命名控件显示与不持有业务真源的展示值；项目背包 `FDivineBeastsInventoryCellPresenter`（背包展示适配器）和技能栏 `FDivineBeastsAbilityBarProjectionDiff`（技能栏差异比较）都已改为实际消费公共能力。
- [x] 提炼 `FGamePlatformUISnapshotScope`（通用来源版本规则）并让平台队伍、战斗警告、项目领域视图三个已存在消费者复用。该跨异构继承树纯规则使用值策略，而不是机械创建UObject基类。
- [x] 新增平台原生测试源码及 `Tests/Architecture/ValidateSharedUIComponentReuse.ps1`（公共组件复用静态门禁）；静态门禁实际14项检查通过；三层公开继承门禁587头文件/1275类型/212继承边检查通过。
- [x] Client（客户端）模块独立构建：`GamePlatformUIClient` 与 `DivineBeastsUIClient` 均 exit 0，分别保留 Foundation 构建证据。
- [ ] Editor（编辑器）模块已成功编译受影响平台源码与新增测试，但最终 DLL 被另一运行中的编辑器占用，LNK1104 无法链接，需后续补正式 Editor 构建和原生自动化实际执行。
- [ ] Monolith 连接、真实 WidgetBlueprint（控件蓝图）继承/控件树/冷重载与界面人工验收未完成，未修改任何 `.uasset`；两份旧技能槽蓝图不擅自合并、重命名或删除。
- [ ] 角色/战斗、水体/PCG、音频、摄像机、VFX等领域按“已存在公共Component → 可组合策略/接口 → 定义数据”逐域复核；只有有真实复用需求和验收证据才能创建新基类。

本次续行没有执行Git提交/推送，没有强行关闭其他任务编辑器或修改现有服务器权威玩法。

## 七、M6 角色/战斗/世界/特效公共基础复审与逻辑定义ID收敛（2026-10-25，本轮不编译）

详细证据及原生门禁缺口见：[《蓝图及公共能力模块化第二轮审查与优化记录》](BlueprintCommonBaseSecondPass_NoBuild_20261025.md)。

- [x] 只读核查既有 PCG Actor（程序化放置器）、Water Interactor（水体交互）、Cinematic Camera（电影机位）、SFX/VFX（音效/特效）和项目V6技能的基类/组件，确认适宜继续复用原公共组件、数据定义与服务，不增加平行玩家/特效/水体框架。
- [x] 把 SFX/VFX 重复的逻辑编号→主资产身份构造收敛到 `UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId`（平台主资产公共转换）。保留领域错误码、主资产类型及失败时清空ID。
- [x] 已完成3个真实消费者接线：`GamePlatformSFXPolicy`（平台音效策略）、`GamePlatformVFXWorldSubsystem`（平台世界特效服务）、`DivineBeastsPresentationClientSubsystem`（项目表现预载协调）。
- [x] 新增后续可运行的 `GamePlatformDefinitionAssetIdResolutionTests.cpp`（平台身份转换测试源码）；本轮仅写代码、**不编译/不运行测试**。
- [x] 新增并实际运行 `Tests/Architecture/ReportReusableBlueprintCandidates.ps1`（蓝图离线候选盘点），共136个命名候选：85个WidgetBlueprint（界面控件蓝图）、43个Blueprint（普通蓝图）、6个AnimBlueprint（动画蓝图）、2个ObjectRedirector（重定向器）。静态结果存储于 `Saved/Validation/BlueprintModularization/20261025/ReusableBlueprintCandidates.json`（离线审计报告）。
- [ ] `ABP_DBA_WorldLocomotion`（世界移动动画蓝图）优先原生复审，离线摘要52节点（51非事件节点），后续核对 AnimLayer（动画层）与真实骨骼共享；没有Monolith完整图信息时不修改。
- [ ] `DivineBeastsPresentationClientSubsystem`（本地玩家表现协调器）和 `DivineBeastsVillageSnowWorldSubsystem`（新手村雪地表现）应分领域封装协调逻辑及分离开发审查，但本轮不修改事件/委托生命周期。
- [ ] `DivineBeastsV6GameplayAbility`（V6服务器技能原语）和世界天气定时器仍涉及网络权威行为，未经后续正式验证不修改。
- [ ] 未执行任何 Client/Editor/Server（客户端/编辑器/专服）编译、UE自动化、PIE、联机、Cook、Monolith编辑器蓝图保存/冷重载。不能宣称新源码已通过构建或蓝图资产已优化。

本轮只修改公共数据身份策略、三个安全消费者、测试源码、离线静态盘点工具和中文实施文档；原有技能资产、地图、视觉资源以及并行任务改动均原样保留。未主动Git提交/推送。

## 七、M7 公共UI修订策略与全量离线蓝图审计（2026-10-26）

本轮实际过程与阻断条件以 [《蓝图模块化状态报告（2026-10-26）》](BlueprintModularizationStatus_20261026.md) 为准。用户的“继续修改，直到完成”要求在既有 **不编译验证** 限制下尽可能完成安全源码重构；以下状态不可与编辑器资产级终验混淆。

- [x] 既有 `FGamePlatformUISnapshotScope`（平台UI作用域快照校验）增加公共命名资源修订方法，与原有GUID来源校验并用。
- [x] 施法条、目标状态、Buff/Debuff（增减益效果）、小地图、任务、倒计时、槽位栏、交互提示、设置项、悬浮提示的C++代码实际复用公共校验，不改变既有反射类和蓝图资产。
- [x] `DivineBeastsRootLayout.cpp`（项目根布局）移出 `FDivineBeastsWorldHUDRoute`（世界HUD槽位路由策略），保留常驻世界0、新手村1、教学2、训练3的已有作者顺序；新增未执行的原生测试源码。
- [x] 背包状态/错误码的项目中文文本映射抽离为 `FDivineBeastsInventoryTextCatalog`（背包本地化信息目录），原LOCTEXT翻译键和两项公开Blueprint接口不变，补充测试源码但不执行。
- [x] 公共 UI 复用静态门禁扩展至32项，公共数据身份转换静态门禁11项，静态检查实际运行通过。
- [x] 使用 `Tests/Architecture/ReportAllBlueprintAssetsOffline.ps1`（全类蓝图离线盘点）核对591份已索引蓝图的当前磁盘路径，591份全部存在，类型和详情查询0异常；按命名划分：205 GA、252 GE、85 WBP、43 BP、6 ABP。
- [x] 使用 `Tests/Architecture/ReportBlueprintOfflineDependencies.ps1`（蓝图离线引用盘点）逐份查询591份已索引蓝图，591份完成、异常0；仅12份出现任何离线索引引用关系，证明引用索引严重不足，**不能对其他579份得出无依赖的判断，更不能自动删除**。
- [ ] 当前真实 UE5.8 蓝图父类、Widget/AnimGraph（控件/动画图）连线、动态消费者与冷重载未取得 Monolith 实时证据；591份本轮原生父类及图校验为0，**资产级重构和全部工程最终验收不具备可声称完成的条件**。
- [ ] 本轮严格不编译、不运行UE自动化、不PIE/联机、不Cook，新增C++应在后续允许时按模块顺序进行原生验收，随后执行三世界和服务器资源隔离核查。
- [ ] 需恢复授权的 Monolith MCP 实时服务后，按真实引用及稳定资产ID逐项审查/改造技能槽、通用移动动画和其它蓝图；不可绕过工具规定修改二进制资源。

本轮没有主动提交或推送，不覆盖并行 VFX 等其他任务资产。
