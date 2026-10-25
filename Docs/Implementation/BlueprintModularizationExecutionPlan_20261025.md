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
