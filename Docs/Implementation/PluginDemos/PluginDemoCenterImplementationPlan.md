# SSLM 插件演示中心核心闭环实施计划（方案 A）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 本任务沿用用户已选择的 **Native**，使用 `superpowers:executing-plans`；用户已确认继续，并明确允许当前main原地修改、不创建工作树。实施进度见`ExecutionProgress.md`，不因源码写入标记运行通过。

**Goal:** 在现有正式工程中实现逐插件功能菜单、真实案例运行、三种样板及独立人工核验闭环。

**Architecture:** 增量升级现有 `DivineBeastsDemoClient`，编目与运行分离；平台提供 UI/Data/编辑器审核能力，项目层承担开发装配。资源沿用唯一 Data 服务和真实内容包，竞技扩展由竞技项目组合根注入，不反向污染公共客户端。

**Tech Stack:** 锁定 UE5.8、UE C++/GAS/UMG、Monolith MCP、PowerShell 7、Python 标准库；不新增第三方运行依赖。

**Spec:** [已通过的方案 A 设计规格](PluginDemoCenterSpecification.md)。日期：2026-10-13（Asia/Shanghai）；**计划已进入实施；T01离线检查已执行，T02源码已写，UE构建/运行/人工验证未执行**。

## Global Constraints（全局约束）

- 唯一正式工程 `Game/DivineBeastsArena.uproject`；依赖方向 `DivineBeasts → MobaCommon → GamePlatform`，项目可以直接依赖平台。
- 当前明确限制仍为不构建。不得启动 UBT、Live Coding、自动重编译、Cook/Stage 或用旧 DLL 声称新代码已验证；构建与发布验证须后续明确授权。
- 用户此前选定 Native，本计划不重新询问执行方法；提交、推送、生产写入、关闭其他任务编辑器均不在本次授权内。
- 新 UI 归 `DBAUIPack_Core` 开发专用范围；Widget、布局、样式、动画、编译、保存、重载全部通过正确工程上的 Monolith。
- 禁止 Monolith 0.23.0 的 `capture_scene_preview` 的 `widget` 路径；保留原稿 RGB、正确 sRGB 和白色乘色。
- 不创建新工程、空插件或每插件一张空地图；内容包不是第四逻辑层；禁止平台/MOBA 引用项目资产。
- 不以业务 Tick 刷新 UI，不新增资产管理器、VFX 执行器、审核框架或服务器权威捷径。
- 自动化不能代签人工通过；当前源码、实际二进制、内容版本与用例版本分别记录。
- 所有新改源码、脚本、配置、测试及合同提供完整中文说明；每次新增文件同步总体目录规划，不覆盖并行任务改动。
- 运行证据根为 `Game/Saved/GamePlatformValidation/<RunId>/Review/`；历史 `Saved/Validation/PluginDemos/` 只保留诊断/索引，不成为另一签收真源。

## Review Focus（重点复核风险）

1. 中文重名功能/地图、旧路径一对多：必须按稳定身份选择，不能按首项、数组顺序或显示文字启动。对应 T01/T03 的重复身份与兼容歧义测试。
2. 同步回调重入、旧世界晚回调、多 PIE：请求只完成一次，旧事件不能影响新案例或别人的租约。对应 T04/T07 的代次与共享租约测试。
3. 本次源码已变化但编辑器仍加载旧 DLL：禁止“未加载测试也算通过”。对应 T11 的构建证据和必需测试集合门禁。
4. 外部证据路径逃逸、缺图/缺日志、伪造审核人：必须拒绝不完整或非人工签收。对应 T08 的证据、版本与显式确认测试。
5. 复位误覆盖玩家存档/余额/设置，或返程触发正式登录：只操作隔离数据，停止演示不自动启动业务流程。对应 T04/T12 与接入计划 R04/R05。

---

## 1. 拆分范围与本轮基线

本规格同时覆盖核心机制和各插件业务适配，拆成两份计划：本文件实现可独立验证的核心闭环；[逐插件接入计划](PluginDemoAdapterRolloutPlan.md)在核心合同稳定后分批接入 47 个代码插件和 18 个内容包。不能在未审查插件真实接口前凭职责名称写完全部适配器。

本轮规划读取的 HEAD 为 `eac862fc38f82b7f51ebb7a14cc377cdec5106a4`，`main` 有并行后端、材质和文档修改；该 HEAD 是规划来源，不是运行二进制版本。规格中的上一轮 HEAD/37 张地图为历史扫描，执行 T00 时重新冻结实际集合。

已有且须复用：DemoCatalog/WorldSubsystem、HubWidget/BirdFlockWidget、启动策略、`RunPluginDemo.ps1`、门户地图与三个 WBP、中央 Data 租约、Editor Review 类型。

已确认的集成缺口：

- `ScenarioId` 目前等于地图路径，WorldSubsystem 也拿它与当前地图比较；改变目录后必须同时修改这两个消费者。
- `RunPluginDemo.ps1` 在启动前检查旧 Hub WBP 文件；UI 迁移必须同步该预检，不能只改 C++ 路由。
- `GamePlatformReviewSubsystem` 是 `UEditorSubsystem`；不能把它添加为开发客户端公共依赖。
- `CookFoundation.ps1` 和 `CookFrontEndClient.ps1` 当前均不接受 `PluginDemoClient` 配置；不能在命令里凭空使用该参数值。
- `Assert-FoundationEditorBuildResult` 要求匹配当前源输入的完整 Editor 构建证据，`OnlyModules` 结果不合格。保持此门禁；无新构建授权时记录待验证，不降低标准。

## 2. 文件责任与命名

以下前缀是文档路径缩写，不是新增目录或环境变量；执行时展开为工作空间相对路径。

| 前缀 | 完整现有根路径 |
| --- | --- |
| DEMO | `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient` |
| FLOW | `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient` |
| REVIEW | `Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Source/GamePlatformDeveloperTools` |
| UIPACK | `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core` |
| DEMODOCS | `Docs/Implementation/PluginDemos` |

| 文件（新增，除非注明修改） | 唯一职责 |
| --- | --- |
| `DEMODOCS/PluginDemoManifest.json`（修改）、`PluginDemoManifest.schema.json` | v2 作者源及中文字段合同，保留旧快照来源 |
| `Tests/Architecture/ValidatePluginDemoManifest.py`、`TestPluginDemoManifest.py` | 无 UE 的编目结构、真实插件集合、身份及证据字段验证 |
| `DEMO/Public/Definitions/DivineBeastsDemoCatalogDefinition.h`、`DEMO/Private/Definitions/DivineBeastsDemoCatalogDefinition.cpp` | 继承 `UGamePlatformDefinitionBase` 的只读目录定义 |
| `DEMO/Public/Contracts/DivineBeastsDemoCatalogTypes.h` | 插件、功能、案例、参数及预检值类型，不公开注册表/缓存 |
| `DEMO/Private/Catalog/DivineBeastsDemoCatalogValidation.h/.cpp` | 定义结构与运行启用条件验证，不装载世界 |
| `DEMO/Public/Demo/DivineBeastsDemoCatalogSubsystem.h`、`DEMO/Private/Demo/DivineBeastsDemoCatalogSubsystem.cpp`（修改） | Data 目录租约、稳定查询、旧接口兼容及旅行协调，不新增平行目录子系统 |
| `DEMO/Public/Contracts/DivineBeastsDemoRunTypes.h` | 运行身份、状态、操作结果及证据索引值类型 |
| `DEMO/Public/Contracts/DivineBeastsDemoMenuTypes.h` | 筛选条件、只读列表/详情快照，不保存业务权威状态 |
| `DEMO/Private/Demo/DivineBeastsDemoRunState.h/.cpp` | 单次运行状态转换、幂等终态、代次检查 |
| `DEMO/Public/Demo/DivineBeastsDemoWorldSubsystem.h`、`DEMO/Private/Demo/DivineBeastsDemoWorldSubsystem.cpp`（修改） | 世界对象所有权、准备/运行/停止、委托及 Data 租约释放 |
| `DEMO/Public/API/IDivineBeastsDemoScenarioAdapter.h` | 最小案例适配合同，只公开操作和结果，不公开缓存或全局管理器 |
| `DEMO/Private/Scenarios/DivineBeastsDemoAdapterRegistry.h/.cpp` | 游戏实例内受控注册与工厂，按完整注册句柄撤销 |
| `DEMO/Public/UI/DivineBeastsDemoHubViewModel.h`、`DEMO/Private/UI/DivineBeastsDemoHubViewModel.cpp` | 项目只读界面投影与用户命令，继承 `UDivineBeastsViewModelBase` |
| `DEMO/Public/UI/DivineBeastsDemoHubScreen.h`、`DEMO/Private/UI/DivineBeastsDemoHubScreen.cpp` | 新页面桥接，继承平台 `UGamePlatformUIScreen`，不在 C++ 制作可视控件树 |
| `DEMO/Public/UI/DivineBeastsDemoHubWidget.h`、`DEMO/Private/UI/DivineBeastsDemoHubWidget.cpp`（修改） | 原蓝图桥接兼容，不直接批量更换旧父类 |
| `DEMO/Private/Scenarios/DivineBeastsDemoResourceBarScenario.h/.cpp` | UI 资源条真实组件样板 |
| `DEMO/Private/Scenarios/DivineBeastsDemoBirdFlockScenario.h/.cpp` | 包装现有鸟群公开操作，不重写正式表现链 |
| `DEMO/Private/Scenarios/DivineBeastsDemoDataLeaseScenario.h/.cpp` | 中央资源租约的正常、共享和取消样板 |
| `DEMO/Private/Evidence/DivineBeastsDemoEvidenceWriter.h/.cpp` | 脱敏、固定根下追加运行证据，不填写人工通过 |
| `REVIEW/Private/Review/GamePlatformReviewEvidenceValidation.h/.cpp` | Editor 证据完整性、根路径、用例/版本匹配验证 |
| `REVIEW/Public/Review/GamePlatformReviewTypes.h`、`REVIEW/Private/Review/GamePlatformReviewTypes.cpp`（修改） | 增量补齐已有人工审核入口，保留历史记录和旧接口迁移边界 |
| `Tools/Unreal/PluginDemos/AuthorPluginDemoCatalog.py` | 从 v2 源经 UE 生成只读定义，核对源指纹；不制作 Widget |
| `Tools/Unreal/PluginDemos/PluginDemoUIMonolithRecipes.json` | 菜单、控件树、状态、焦点及迁移的 Monolith 动作合同 |
| `Tests/Assets/TestPluginDemoAssetsMonolith.py` | 通过 Monolith 检查保存/重载后的真实资产，不用旧截图代验 |
| `Build/Game/TestPluginDemo.ps1`、`Tests/Foundation/Scripts/TestPluginDemoRunner.ps1` | 显式启动的无构建测试入口及报告/前置证据回归 |
| `Build/Game/RunPluginDemo.ps1`、`Build/Game/CookFoundation.ps1`（修改） | 启动预检迁移及开发 Cook 配置白名单，默认行为不变 |

所有 `.h/.cpp` 对均列出确切基名；只有确有独立职责才新增。每个 C++ 测试放相应模块 `Private/Tests`，具体文件见任务。上述文件表是计划，不表示文件已创建。

## 3. 执行顺序、状态与验证命令

顺序：T00 → T01 → T02 → T03 → T04；T05/T06/T07/T08 接受这些合同；T09/T10 接真实资产；T11/T12 完成验证入口和装配；T13 样板交付；随后接入计划 R01—R06；T14 最终收口。共享 Config、Build.cs、Monolith 和目录文档按 Native 串行修改。

每个任务分别记录 `CodeWritten`（代码已写）、`StaticVerified`（静态验证）、`RuntimeVerified`（实际运行）、`HumanReviewed`（人工核验）。在不构建期间，可继续完成不依赖新反射类的源码工作；C++ 红绿测试、资产生成和运行门禁未执行时不得勾成通过。每任务最后进行差异/中文注释审核、同步目录文档，保留未提交修改；不自动 `git commit`。

验证入口：

| 代号 | 命令/动作与前提 | 成功口径 |
| --- | --- | --- |
| V0（现有） | `git diff --check`；`pwsh -NoProfile -File Tests/Architecture/ValidateDesignBaseline.ps1 -WorkspaceRoot .` | 退出0；后者只证明静态结构，遇到其他任务错误不修改范围外代码 |
| V1（T01新增后） | `python Tests/Architecture/TestPluginDemoManifest.py`；`python Tests/Architecture/ValidatePluginDemoManifest.py --workspace . --manifest Docs/Implementation/PluginDemos/PluginDemoManifest.json` | 单测全部通过，目录错误0，未审计功能仍有明确状态 |
| V2（T11新增后） | `pwsh -NoProfile -File Build/Game/TestPluginDemo.ps1 -Execute -EngineRoot <锁定引擎绝对路径> -BuildResultPath <真实完整Editor构建结果> -Suite Demo` | 必需用例全部存在且Success、进程退出0、证据指纹稳定；当前无新构建时不执行 |
| V3（T11新增后） | 同V2，`-Suite Review` 或 `-Suite LaunchPolicy` | 对应必需用例完整；不靠零测试或只有退出0判断通过 |
| V4（现有，须人工允许启动） | `pwsh -NoProfile -File Build/Game/RunPluginDemo.ps1 -Start -Target EditorGame -EngineRoot <锁定引擎绝对路径>` | 新进程仅进入开发门户，无认证副作用；资产/二进制预检失败不得触发构建 |
| V5（T09/T10后） | 正确工程上的Monolith资产检查及独立重载；新测试只调用已发现的工具动作 | 控件树、父类、引用、编译/保存/重载证据有效；颜色另经真实RHI |

占位的路径参数必须由当次发现结果替换，不将示例当作现存报告。V2/V3 的脚本和新增测试尚不存在，不能提前宣称已运行。构建命令仅在 T14 后续授权时使用既有 `BuildFoundation.ps1`；本计划任何脚本均不得自发调用它。

## 4. 核心任务

### T00：冻结可追溯基线与文件责任

**Files:** 读取上述已有源码、根/局部规则、现行构建入口；增量更新已存在的 `DEMODOCS/ExecutionProgress.md`、`FeatureCoverage.md`、`AssetReuseMap.md`，修改总体目录规划。后二者是编目的中文导出/审计材料，不是第二套作者源。

**Interfaces:** 输入真实 `.uplugin`、内容登记、地图文件和当前差异；输出 `BaselineRevision`、文件责任表及 `Unreviewed` 功能范围。规划参考为47代码/18内容/37地图，变化必须解释，不能硬凑数量。

- [x] 记录分支/HEAD/未提交文件和当前编辑器/编译进程来源，不停止他人进程。
- [x] 对照真实描述与内容登记，生成身份和地图候选表；全部运行/人工列初始为未验证。
- [x] 在进度表区分已存在源码、待改代码、待生成资产与待运行检查；保留材质/背包任务的接线。
- [x] 用V0检查本任务文档差异，保存基线证据，不改正式默认地图。实际全局门禁退出1（既有Saved辅助工程被计入），此勾选只表示执行和记录，不表示通过。

### T01：v2 功能编目与无引擎验证器

**Files:** 文件表中的 JSON/Schema、两份 Python 门禁；测试夹具内置在 `TestPluginDemoManifest.py`，不创建伪 UE 资产。

**Interfaces:** `validate_manifest(document: dict, workspace: Path) -> list[dict]`，问题含 `code`、`location`、中文 `message`；不修改文档。v2 必含 `schema_version=2`、`plugins`、`features`、`scenarios`、`review_cases`、`legacy_map_routes`；四维状态按规格5节独立保存。功能字段采用规格4.3，不以普通文件存在标记资源可加载。

- [x] 写 `test_duplicate_feature_rejected`、`test_two_scenarios_share_map`、`test_unreviewed_is_not_complete`、`test_missing_plugin_visible`、`test_legacy_map_ambiguity`：分别断言重复拒绝、共享允许、未知分母不算100%、缺口保留、一对多不得自动选取。
- [x] 执行V1单测，记录缺验证实现时的失败；失败必须是合同断言而非解释器缺失。
- [x] 实现结构、引用、身份和目标检查；升级真实插件清单，尚未审计的功能不虚构，保留v1来源和迁移说明。
- [x] 再执行V1，使用变异输入证明重复/错端/缺用例会失败；生成中文覆盖导出，更新目录登记。当前27项离线回归通过，详见ExecutionProgress.md。

### T02：可加载的目录定义及版本校验

**Files:** 目录定义、CatalogTypes、私有校验文件；修改 `DEMO/DivineBeastsDemoClient.Build.cs`；测试 `DEMO/Private/Tests/DivineBeastsDemoDefinitionTests.cpp`。

**Interfaces:** `UDivineBeastsDemoCatalogDefinition : UGamePlatformDefinitionBase`；复用 `LogicalId`、`DataVersion`，可读Schema范围固定2。公开数组为 `Plugins`、`Features`、`Scenarios`、`ReviewCases`、`LegacyMapRoutes`，对应JSON派生数据；`ValidateDefinition() const -> FGamePlatformResult` 先调用Super。拟用逻辑身份通过 `FGamePlatformId::TryCreate("divinebeasts.demo", "catalog", 1, OutId)` 构造，并先查重，不手拼新主资产类型。

`CatalogTypes.h` 统一声明 `FDivineBeastsDemoFeatureEntry`、`FDivineBeastsDemoScenarioDefinition`、`FDivineBeastsDemoParameterValue` 与 `FDivineBeastsDemoPreflightResult`。前两者字段逐项映射规格4.3的功能/案例记录；参数由 `ParameterId:FName`、显式类型枚举和对应的布尔/整数/实数/标识值组成，仅允许与声明类型匹配的有效载荷，不接受脚本字符串；预检结果含 `bCanStart:bool`、`Result:FGamePlatformResult` 和逐项阻塞原因。未启用插件、缺资源与尚未就绪分别保留，不压成同一个“不可用”。枚举/字段中文语义与JSON Schema在同一任务完成。

- [x] 写 `DivineBeasts.PluginDemos.Definition`：Schema1/3拒绝、重复Feature/Scenario拒绝、两案例同地图允许、NaN/负超时拒绝、空操作能力不冒充支持。
- [x] 登记C++红测预期；无当前构建证据时标记RuntimeNotRun，不运行旧测试结果。
- [x] 定义稳定值类型及私有校验；公开头直接使用Core/UI类型时补真实Public依赖，避免仅靠传递头可见；不公开解析缓存。
- [ ] 执行V0/V1；授权构建后的V2须覆盖本测试，再标记运行验证。

### T03：目录服务、兼容映射及请求查询

**Files:** 修改既有CatalogSubsystem；测试 `DEMO/Private/Tests/DivineBeastsDemoCatalogTests.cpp`。

**Interfaces:** 新增 `GetFeatures(FName PluginId) const -> TArray<FDivineBeastsDemoFeatureEntry>`、`GetFeatureScenarios(FName FeatureId) const -> TArray<FDivineBeastsDemoScenarioDefinition>`、`PreflightScenario(FName ScenarioId) const -> FDivineBeastsDemoPreflightResult`。原 `GetPlugins/GetScenarios/OpenScenario/ReturnToHub` 保留兼容；新场景身份和 `MapAsset` 分开，`bAvailable` 只作预检投影，目录更新用 `OnCatalogChanged` 普通委托。

- [ ] 写 `DivineBeasts.PluginDemos.Catalog`：中文重名按ID区分、筛选不改总数、非法目录不半发布、禁用插件仍可查、旧地图一对多要求选择。
- [ ] 将 `AcquireDefinition`/`ReleaseDefinition` 接入现有游戏实例服务；只有完整定义通过且依赖满足后发布新快照，迟到加载用目录代次拒绝。
- [ ] 将扫描地图放入“未编目/诊断”，取消正式归属对 `GuessOwningPlugin` 的依赖；更新WorldSubsystem当前地图比较为 `MapAsset` 包路径。
- [ ] V0/V1检查；V2待授权，确认资源失败不清除可诊断信息、不发出虚假成功事件。

### T04：案例生命周期与可扩展适配合同

**Files:** RunTypes/RunState、Adapter接口/私有Registry，修改Catalog/WorldSubsystem；测试 `DEMO/Private/Tests/DivineBeastsDemoLifecycleTests.cpp`。

**Interfaces:** `FDivineBeastsDemoRunToken` 含 `RunId:FGuid`、`WorldId:FGuid`、`Generation:uint64`；终态/代次由私有RunState管理。适配器合同为 `Prepare(const FDivineBeastsDemoScenarioContext&, FDivineBeastsDemoOperationCompletion)`、`Execute(FName OperationId, const TArray<FDivineBeastsDemoParameterValue>&, FDivineBeastsDemoOperationCompletion)`、`Stop(FDivineBeastsDemoOperationCompletion)`，回调携带完整RunToken和真实 `FGamePlatformResult`；每个回调至多一次、只在游戏线程。

`FDivineBeastsDemoScenarioContext` 含运行Token、世界弱引用、只读案例快照和单调时钟截止秒数；参数值类型与T02一致，异步不得保留调用栈上的引用。`RunTypes.h` 声明运行Token、Context、注册Handle与 `using FDivineBeastsDemoOperationCompletion = TFunction<void(const FDivineBeastsDemoRunToken&, const FGamePlatformResult&)>`。API头声明 `using FDivineBeastsDemoAdapterFactory = TFunction<TUniquePtr<IDivineBeastsDemoScenarioAdapter>(UWorld&)>`。已有CatalogSubsystem提供 `RegisterScenarioAdapter(FName AdapterId, FDivineBeastsDemoAdapterFactory Factory) -> FDivineBeastsDemoAdapterRegistrationHandle` 与 `UnregisterScenarioAdapter(FDivineBeastsDemoAdapterRegistrationHandle Handle) -> FGamePlatformResult`，均为游戏线程C++接口，不暴露为蓝图函数。公开Handle含实例ID、注册ID和代次；重复身份拒绝且返回无效Handle，私有Registry持有工厂，不保存进程全局世界状态。

- [ ] 写 `DivineBeasts.PluginDemos.Lifecycle`：同步回调重入、两次完成、停止中返回、准备后超时、世界退出后回调、跨实例注销、重复停止、两可写案例竞争。
- [ ] 实现状态机和每世界唯一可写案例；启动/停止超时从案例字段读取，必须有限正数，单测用可控时钟，不以真实睡眠拖延。
- [ ] 先拒绝新命令，再停止实例、取消/解绑/释放所有权，最后确认返程；取消保存为取消，清理失败保存为失败，不以 `OpenLevel` 受理代表成功。
- [ ] V0/V1执行；V2延期。只持有本案例可恢复状态，无法安全复位的服务拒绝复位命令并给原因。

### T05：只读 ViewModel 与菜单桥接

**Files:** 新HubViewModel/HubScreen，修改旧HubWidget兼容；测试 `DEMO/Private/Tests/DivineBeastsDemoMenuTests.cpp`；Build.cs补真实 `DivineBeastsUIClient`、`GamePlatformUIClient` 依赖，先检查无反向环。

**Interfaces:** VM为 `UDivineBeastsViewModelBase` 派生，提供 `BindCatalog(UDivineBeastsDemoCatalogSubsystem*) -> bool`、`UnbindCatalog() -> void`、`SetFilter(const FDivineBeastsDemoFilter&) -> void`、`SelectFeature(FName) -> bool`、`RequestScenario(FName) -> bool` 和只读 `GetMenuSnapshot() const -> const FDivineBeastsDemoMenuSnapshot&`。Filter含搜索/层级/实现/运行/人工状态，快照含总数、选中ID、列表、详情和阻塞原因，声明于 `Public/Contracts/DivineBeastsDemoMenuTypes.h`。

- [ ] 写 `DivineBeasts.PluginDemos.Menu`：筛选无结果、未审计、禁用插件、刷新保留ID、中文重名、详情关闭恢复焦点、失活解绑及旧VM事件忽略。
- [ ] 实现事件订阅与命令转发；屏幕不扫描资产、不发HTTP，不在NativeTick刷新业务；数字分别显示四维统计。
- [ ] 保留旧蓝图父类/反射字段；新HubScreen采用平台页面契约。命名控件采用T09的固定合同，按钮是否可用来自真实操作能力。
- [ ] V0/V1检查；V2与真实焦点测试在可加载新父类后执行，不用C++临时控件树替代Monolith。

### T06：鸟群真实视觉样板适配

**Files:** BirdFlockScenario；复用/必要修改 `DEMO/Private/Demo/DivineBeastsDemoBirdControls.cpp`；测试 `DEMO/Private/Tests/DivineBeastsDemoBirdScenarioTests.cpp`。

**Interfaces:** 注册 `AdapterId=BirdFlock`；编目案例 `DBAClient.BirdFlock.Count` 与 `DBAClient.BirdFlock.Reset`，主功能在项目客户端，关联VFX/Presentation各有独立ReviewCase。调用已有 `ApplyBirdCount(int32)`、`GetRequestedBirdCount()`、`GetActualBirdCount()`、`ResetDemoChanges()`；若需按组件所有权收窄，增量改该公开操作实现，不另建播放器。

- [ ] 写 `DivineBeasts.PluginDemos.BirdScenario`：无真实组件拒绝、非法鸟数拒绝、设置失败保留原值、退出只恢复本案例组件、GPU未读回显示未知。
- [ ] 核对现有正式组件允许范围后登记参数，不自行编造最大鸟数或性能预算；请求数量与GPU存活数分开。
- [ ] 连接真实场景/Presentation/Data/VFX调用链，保留其他材质/天气案例资源，正常和复位各有事件。
- [ ] 静态检查后等待V2/V5及真实场景观察；调用受理不能作为“鸟群画面正确”的签收。

### T07：UI 与 Data 两种不同性质的样板

**Files:** ResourceBarScenario/DataLeaseScenario；测试 `DEMO/Private/Tests/DivineBeastsDemoSampleScenarioTests.cpp`；资源条WBP在T09制作。

**Interfaces:** `AdapterId=ResourceBar` 以 `UGamePlatformResourceBarWidget::ApplyResourceState`/`GetResourceState` 为真调用；案例 `GamePlatformUI.ResourceBar.Normal`、`.InvalidMaximum`。`AdapterId=DataLease` 经 `IGamePlatformDataService::Get/AcquireResources/GetLeaseState/ReleaseResources` 操作实际 `/Engine/BasicShapes/Cube.Cube`，案例 `GamePlatformData.Resources.SharedLease`、`.Cancel`。样例数值是明确开发夹具，不展示成玩家事实。

- [ ] 写 `DivineBeasts.PluginDemos.Samples`：资源50/100比例0.5，最大值0不除零且文本清空；非法有限性测试不写入真实资产默认值；两个调用者共享同一资源、释放A后B仍Ready、加载取消只完成一次。
- [ ] 实现只更新样板控件的输入与复位；资源请求只持本案例完整租约，失败/取消/World退出正确释放。
- [ ] Data服务未初始化时明确阻塞；不得在演示模式创建假的服务或把旧缓存当成功。
- [ ] 运行可执行的静态验证；V2/V5在新二进制与资源就绪后执行，三类样板均需独立运行证据。

### T08：证据保存与人工签收增强

**Files:** EvidenceWriter、ReviewEvidenceValidation、既有ReviewTypes；测试 `DEMO/Private/Tests/DivineBeastsDemoEvidenceTests.cpp`、`REVIEW/Private/Tests/GamePlatformReviewEvidenceTests.cpp`；修改既有 `GamePlatformDeveloperToolsTests.cpp` 兼容断言。

**Interfaces:** `WriteRunEvidence(const FDivineBeastsDemoRunEvidence&, FString& OutIndexPath, FString& OutError) -> bool`；RunEvidence含RunToken、案例/用例ID、源码/构建/内容版本、环境、原始终态、期望匹配、证据索引。Editor新增 `ValidateReviewEvidence(const UGamePlatformReviewCase&, const UGamePlatformReviewReport&, FString& OutError) -> bool`，按必需证据类型逐项核对。旧 `ApplyHumanDecision` 保留签名，加入真实合同与版本检查；旧无绑定报告不能被新门禁误签通过。

`FDivineBeastsDemoRunEvidence` 在 `RunTypes.h` 声明，仅含中立值和受管证据引用，不含Editor对象；Writer在其私有头暴露上述函数给本模块调用。`ValidateReviewEvidence` 为既有ReviewSubsystem的新增C++成员，由私有验证文件完成检查，不新建第二个审核子系统。

- [ ] 写 `DivineBeasts.PluginDemos.Evidence` 与 `GamePlatform.DeveloperTools.ReviewEvidence`：路径逃逸、缺文件/类型、内容指纹变化、错误Case/Run、未人工确认、原结果为预期拒绝、历史修订保留。
- [ ] 实现限定证据根、脱敏和追加修订；不允许客户端填Reviewer或直接标记Passed，导入外部报告仅生成待审状态。
- [ ] Editor桥只在Editor编译条件内消费Review模块；客户端公共头不包含EditorSubsystem或Review类型；保持一套审核模型。
- [ ] V0/V1与V3（延期）核对；正常/失败案例的原始运行结果不可被人工签收改写。

### T09：目录 DataAsset 与演示 UI 的 Monolith 制作

**Files:** AuthorPluginDemoCatalog.py、PluginDemoUIMonolithRecipes.json、TestPluginDemoAssetsMonolith.py；修改 `UIPACK/Docs/MonolithGenerationManifest.json`。计划资源根 `/DBAUIPack_Core/Development/PluginDemos` 下 `Definitions/DA_DBA_PluginDemoCatalog`、`UI/WBP_DBA_PluginDemoHub`、`UI/WBP_DBA_PluginDemoDetail`、`UI/WBP_DBA_PluginDemoControls`、`UI/WBP_DBA_PluginDemoReport`、`UI/WBP_DBA_PluginDemoResourceBar`；均需引擎真正生成。

**Interfaces:** 作者脚本 `build_catalog(manifest_path: str) -> dict` 仅在真实UE执行，按T02填定义并回读源指纹；Widget制作只经Monolith。固定命名控件合同：`PluginSearchInput`、`LayerFilterCombo`、`PluginList`、`FeatureList`、`ScenarioList`、`FeatureDetailsText`、`OperationStatusText`、`RefreshButton`、`LaunchScenarioButton`、`StopScenarioButton`、`ResetDemoButton`、`ReturnButton`、`EvidenceButton`，Tab/方向键顺序按规格6节。

- [ ] 先记录预期父类、命名控件、可操作性和每个状态；只读测试必须对缺真实资产/错父类/缺控件失败。
- [ ] 确认正式工程、UE5.8、Monolith版本和动作目录；核对新C++父类已加载且匹配构建。未就绪时停在BlockedByEditorBinary，不触发编译。
- [ ] 经Monolith创建/增量编辑界面与布局，采用现有自然玉石风格及固定字号；资源条样板继承平台基类，不烘焙玩家数值。
- [ ] 对1920×1080、2560×1440、3840×2160及较小安全区检查折叠/滚动、固定字号和非颜色状态提示；减少动态效果时禁用装饰动画，不丢业务状态。诊断采样仅在展开面板时启动，关闭/失活即取消。
- [ ] 编译、保存、重开、回读，记录真实结果；新彩色纹理另做导入阻断审计和RHI颜色验证，拒绝禁用的Widget截图路径。

### T10：共享关卡、旧资产迁移与重载

**Files:** 修改既有 `Tools/Unreal/PluginDemos/AuthorDBAPluginDemoHubMap.py`、`ValidateDBAPluginDemoHubMap.py`；真实门户与BirdFlockIntegrationReview地图只在需要时增量编辑；更新WorldSubsystem和RunPluginDemo中的资源路由。

**Interfaces:** T03的 `MapAsset`、T04的ScenarioContext及T09资源身份是唯一输入；旧地图路径只通过显式兼容表解析，不再决定Widget类别。

现WorldSubsystem要求所加载类继承旧HubWidget；新HubScreen不是该类的子类，因此必须一并修改类校验、实例保存和激活/失活接线。新路由按平台Screen合同创建/清理，旧路由显式走HubWidget兼容分支；两种实例只共享 `UUserWidget` 级所有权，不对新页面做旧父类强转，不直接改旧WBP父类。新旧分支分别测试，完成引用迁移和重载前不删除旧资源。

- [ ] 建立同地图切换两个案例/退出恢复的检查用例，记录迁移前软/硬引用、旧父类和可恢复副本。
- [ ] 在 `DEMO/Private/Tests/DivineBeastsDemoCatalogTests.cpp` 扩展 `DivineBeasts.PluginDemos.Catalog`：新Screen类可加载、旧HubWidget兼容有效、不支持父类明确拒绝、失活后租约/委托不泄漏；运行验证等待匹配二进制。
- [ ] 通过Monolith/引擎处理旧Hub/Overlay/BirdFlock UI迁移或受控兼容引用；重复运行作者脚本不得复制既有场景对象。
- [ ] UI样板和Data样板使用门户诊断区，视觉样板复用已有真实鸟群地图；未编目探针继续独立显示，不计入正式覆盖。
- [ ] 独立重载后执行V5；旧路径启动、重置、返回、新旧目录兼容都验证，不以文件复制代替UE引用修复。

### T11：不自动构建的测试运行器

**Files:** 新TestPluginDemo.ps1/TestPluginDemoRunner.ps1；复用 `FoundationTools.psm1` 与 `FoundationBuildEvidence.psm1`，不放宽原门禁。

**Interfaces:** 参数 `-Execute`、`-VerifyOnly`、`-EngineRoot`、`-BuildResultPath`、`-Suite Demo|Review|LaunchPolicy`、`-ReportPath`、`-RunId`、`-TimeoutSeconds`（默认900，仅是可配置进程上限，不是性能预算）。Execute/VerifyOnly互斥；VerifyOnly不启动UE。Demo过滤 `DivineBeasts.PluginDemos`，Review过滤 `GamePlatform.DeveloperTools`，LaunchPolicy必须含现有 `DivineBeasts.PluginDemos.LaunchPolicy`。

- [ ] 写PowerShell替身回归：缺当前完整Editor证据、旧DLL、零测试、缺必需用例、失败用例、超时、报告过程中输入变化一律拒绝，VerifyOnly不创建进程。
- [ ] 实现调用 `Assert-FoundationEditorBuildResult -Workspace ... -Engine ... -BuildResultPath ... -RequiredModules ...`；只用现有进程管理器启动独占测试进程，不自行构建或停止他人UE。
- [ ] 解析原生Automation JSON，核对T02—T08的必需测试名逐一出现并Success，结束再核对指纹；输出未执行/失败/通过及真实退出码。
- [ ] 实施阶段可运行 `pwsh -NoProfile -File Tests/Foundation/Scripts/TestPluginDemoRunner.ps1` 的替身测试；V2/V3等待真实构建授权与证据，不把替身当UE通过。

### T12：启动与发行隔离闭合

**Files:** 修改 `Build/Game/RunPluginDemo.ps1`、`Build/Game/CookFoundation.ps1`、`Game/Config/DefaultGame.ini`、`Game/Config/Custom/PluginDemoClient/DefaultGame.ini`、`Build/Game/ServerPresentationExclusions.json`；必要时增量修改 `DBAClient.uplugin`/Demo Build.cs。测试 `Tests/Foundation/Scripts/TestPluginDemoPackaging.ps1`，扩展FLOW已有 `Private/Tests/DivineBeastsPluginDemoLaunchPolicyTests.cpp`。

**Interfaces:** 保留两项实际开发参数 `-DBAPluginDemo` 和 `-DBAPluginDemoMap=<原Hub包路径>`、保留正式GameDefaultMap。CookFoundation只新增 `PluginDemoClient` 显式白名单分支且只允许Client目标；默认Foundation配置/Server流程不变，开发资源集合从经验证编目导出。

- [ ] 写发行/Server/Commandlet/冲突参数拒绝、旧UI文件路径失效、普通登录不被旁路、PluginDemoClient误用于Server拒绝的回归。
- [ ] 同步启动预检的新UI位置和真实二进制门禁；脚本缺资源/模块则失败，不弹出自动编译补救。
- [ ] 默认Cook排除开发目录/UI定义，开发配置显式纳入所需集合；依赖未知不硬塞全项目资源。保持三种服务器角色排除策略及并行材质库条目。
- [ ] 运行纯脚本门禁；实际启动V4、V3及干净Cook留到相应授权阶段，不以INI文字存在证明产物隔离。

### T13：三个样板的可复核交付

**Files:** 修改进度表、覆盖导出、资产复用表；运行证据只写规格规定的Saved根。

**Interfaces:** 使用T03功能菜单、T04运行令牌、T08证据及Editor人工入口；不新增“样板专用成功”旁路。

- [ ] 逐一核对UI正常/非法上限、Data共享/取消、鸟群调参/恢复的操作步骤和预期；缺任何真实运行条件则保持阻塞。
- [ ] 在用户允许且当前二进制有效时运行V2/V3/V4/V5，保存三类独立证据和清理结果；本阶段不自行恢复构建。
- [ ] 人工检查焦点/返回/多视口、真实声音/画面及状态说明；请求用户在Editor审核，不代填Reviewer。
- [ ] 本批按CodeWritten/StaticVerified/RuntimeVerified/HumanReviewed分别交付；仅核心样板完成不得宣称47插件全覆盖。

### T14：全范围集成和最终证据收口

**Files:** 合并接入计划R01—R06的作者源/派生文档及报告索引；只在授权后使用既有构建/发布入口，不改低层业务以让演示通过。

**Interfaces:** 全范围交付采用规格D01—D14；“逐功能审计完成”和“当前版本人工签收完成”都满足才能声明总体验收。

- [ ] 对照所有65身份、实际已审计功能和每项适用案例检查遗漏、未实现、阻塞、例外审核，不以插件卡片数量代替功能覆盖。
- [ ] 检查平台最小配置、关闭MOBA、竞技组合、三角色与五模式；缺服务器/地图时保留阻塞，不模拟联网通过。
- [ ] 用户后续明确授权后，分别执行Editor/Client/Server构建、自动化、干净Cook/Stage和实际容器检查；记录确切版本/参数/退出码/设备，不用旧模块构建替代完整证据。
- [ ] 汇总当前版本人工结论与残余风险，保存所有失败和历史记录；未授权不提交/推送，不将进度表全勾为完成。

## 5. 规格追踪与回退

| 规格门禁 | 本计划/接入计划责任 |
| --- | --- |
| D01 | T00/T01，R01—R06身份与功能审计 |
| D02 | T01/T03/T04 |
| D03 | T01/T03/T05 |
| D04 | T05/T09/T10 |
| D05 | T04/T06/T07 |
| D06 | T03/T04/T10 |
| D07 | T04/T07 |
| D08 | T11/T12 |
| D09 | T08/T11/T13 |
| D10 | T02/T05/T12，R05 |
| D11 | R05、T14 |
| D12 | T09/T10/T13 |
| D13 | T12/T14 |
| D14 | 每个任务的中文说明自审及R06汇总 |

回退以任务为单位撤销本人变更：编目保留旧Schema来源与ID映射；旧目录读取兼容维持到新资产重载验证；UI迁移仅通过引擎和受管副本恢复；构建/配置故障不删除旧可用产物。禁止清空工作树、批量删除开发地图或直接改二进制引用。

当前已开始Native实施，逐任务证据以`ExecutionProgress.md`为准，不代表T00—T14整体完成。任务需区分源码、静态、运行与人工维度；未执行项保持未勾选。实际构建限制保持不变。

2026-10-13文档级复核：核心15任务、接入6批和D01—D14追踪完整；接入身份与本轮真实描述文件扫描的47代码插件、18内容包集合一致；规格/两份计划/旧计划的本地Markdown链接有效。该检查只验证计划与身份集合，不验证任何插件功能、UE资源或运行结果。
