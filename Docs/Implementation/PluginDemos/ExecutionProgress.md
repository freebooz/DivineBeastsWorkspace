# 插件演示中心实际实施进度

## 2026-10-13 方案A Native增量执行（当前）

- 用户已通过规格、要求继续，并明确允许当前main原地修改、不创建工作树。开始HEAD为`eb1fe0e932967ce97078df12c8080cf5121c99b4`，开始时无未提交改动；后续其他任务的材质/背包改动独立保留。
- 当前限制：不构建、不启动Live Coding、不Cook/Stage、不提交或推送；未启动或停止编辑器/编译进程。以下旧P0—P8和历史构建段落只描述2026-10-12当时状态，不代表当前仍在构建或已验收。
- T00已重新扫描47代码、18内容和37张开发地图文件；现有进度/覆盖/复用文档增量维护。功能数保持待审计，不沿用旧API数量作为功能完成数。
- T01已实现Schema2、只读验证器及中文导出；完整v1内容保留History。当前23项Python回归已运行通过；作者源、真实身份集合和派生文档一致。重复、错端、缺用例、路径越界、超时/参数非法和歧义路由均有回归。
- T02已写入项目只读Data目录定义、公开值合同、Private结构校验及`DivineBeasts.PluginDemos.Definition`原生测试；公开Core依赖已补齐。**尚未构建/运行，所以只记CodeWritten，不记RuntimeVerified或HumanReviewed。**
- T03—T14及R01—R06尚未完成：没有把v2接到实际菜单、没有新DataAsset/Widget/关卡、没有逐插件功能接线或人工签收。
- 全局基线门禁失败1项：`Game/Saved/BirdFlockAuthorHost/BirdFlockAuthorHost.uproject`被计入正式工程数，结果为2；三端声明装配与三层继承通过。不删除该工程、不修改门禁，不宣称全项目检查通过。
- 中文说明及迁移合同见[Schema2接入说明](PluginDemoManifestContract.md)。本轮说明审查范围限新改的编目/校验/定义/测试/文档，不声称历史全工程已合规。

## 2026-10-12历史阶段记录（保留追溯）

> 2026-10-12｜开始时HEAD：bad8db3a63c7a24f2fee8257387153d41c376b0e｜工作区：DivineBeastsWorkspace（SSLM工作空间）。
> 以下仅记录**真实创建的文件和已运行检查**，未做的Editor编译、地图/视觉验收不得标为通过。

## P0 插件审查（进行中）

- 已读取真实.uplugin（插件描述）和Git跟踪关卡，登记47个代码插件、17个内容包、31张开发地图。
- 已生成 PluginDemoManifest.json（插件目录快照）、FeatureCoverage.md（能力线索）、AssetReuseMap.md（候选地图复用说明）。
- 已扫描508个Public头文件中的592个UFUNCTION（蓝图反射调用）候选，生成PublicApiCandidates.json与PublicApiAudit.md；这是源码可见性线索，**不是592项已完成功能**。
- 待完成：按真实功能语义拆分FeatureId（功能身份），审核具体蓝图及资源在UE AssetRegistry的有效性，逐项确认地图能运行。

## P1 演示模式与登录隔离（源码已实现，编译/运行待验证）

- 新增 FDivineBeastsPluginDemoMode（演示启动策略），要求显式-DBAPluginDemo与精确总览地图参数；Test/Shipping/专服/Commandlet不允许旁路。
- 项目应用流程在演示模式跳过正式Online/Session适配初始化，StartFlow有防御性拒绝。
- 项目UI子系统在演示模式跳过正式根布局和业务页面，应用UI适配器不会触发首次StartFlow(false)。
- 新增Build/Game/RunPluginDemo.ps1（开发启动脚本），地图不存在就拒绝；增加PluginDemoClient定制Cook配置和正式DefaultGame.ini中演示目录排除规则。
- 已通过：插件描述JSON校验、模块配置白名单、目录数目、PowerShell启动脚本语法（PS5读取UTF-8临时BOM副本的解析测试）、已跟踪Git差异格式检查。
- 未通过/未运行：真实Editor/Client编译和不经登录的游戏实例启动；原生单测编译运行；Client/Server Cook与发行资源审计。

## P2 开发演示目录（基础代码已落地，待编译验证）

- 在既有DBAClient内加入DivineBeastsDemoClient（开发客户端模块），由.uplugin白名单限开发目标，不创建第48个代码插件。
- 新增 UDivineBeastsDemoCatalogSubsystem（演示目录子系统），从UE真实IPluginManager（插件管理器）读取发现插件，从AssetRegistry（资产注册表）读取开发地图，保留插件是否启用、地图待验证和诊断地图标签。
- 新增 UDivineBeastsDemoHubWidget（演示总览C++蓝图父类），提供插件列表、场景列表、选择场景与返回总览的接口，不在C++伪造用户界面资产。
- 当前实现属于**演示发现/地图路由基础**；尚未实现各插件的详细参数控制、统一数据资产Definition、完整资源租约和场景返回覆盖层，不能宣布P2完成。

## P3—P8 剩余部分

- P3：真实 L_DBA_PluginDemoHub.umap（演示总览地图）与Monolith创建WBP资产、鸟群/天气完整可交互演示均**未制作**；Monolith已注册但当前UE Editor未运行。
- P4—P7：五类通用模板、逐插件演示案例及17内容包资产画廊尚待实现。
- P8：正在执行UE5.8 Editor开发构建：Cwebcodex作业 wc_job_T0gTkSIfDe1dr9ju；截至本次记录状态为运行中，不代表编译成功。自动化实跑、客户端/服务器Cook及人工视觉签署未执行。

## 变更与风险控制

- 未修改正式默认GameDefaultMap（登录前端）；不清理其他任务未提交修改，不自动提交/推送。
- 2026-10-12本次Cwebcodex续做：在演示Widget C++基类增加只读分类/搜索接口，在演示World源码中增加真实Widget到位后的鼠标焦点和离开时的输入清理；6个文件增量修改（4份C++、1份测试、1份交付说明），不创建虚假UI。
- 双模式静态回归已运行10项，全部通过（Python unittest）；真正UE编译及游戏运行未因此获得通过。
- 尝试执行 `UnrealEditor-Cmd.exe -ExecutePythonScript=.../AuthorDBAPluginDemoHubMap.py` 创建真正地图，退出码1；UE日志证实无法找到新模块 `DivineBeastsDemoClient` 导致整个DBAClient插件加载失败。必须先补齐模块Editor构建才可生成地图及连接Monolith。总览地图与Widget资产均仍未证明存在。
- Runner默认PowerShell 5.1，`pwsh.exe`（PowerShell7）未发现，双模式统一启动脚本的实际运行尚受执行环境阻塞。
- 后续已补充 `DivineBeastsDemoClient.Build.cs` 对 `GamePlatformCore` 的直接链接依赖并通过Editor Development定向模块构建；经官方UnrealBuildTool WriteMetadata检查后，DemoClient已登记到Editor modules清单。
- 已以UBT定向成功编译 `GamePlatformVFXClient`（27动作）和 `DivineBeastsPresentationClient`（26动作），排除至少7项编译DLL与最新导出符号不匹配。无虚假编译通过声明。
- UE编辑器当前仍因开发工具 `GamePlatformDeveloperTools` 新模块缺失而在进入地图作者脚本前退出，其定向构建仍需读取实际终态。独立试验的 `-DisablePlugins=DBAArena` 不写任何正式项目默认配置。`L_DBA_PluginDemoHub.umap` 与 `WBP_DBA_PluginDemoHub.uasset` 仍待真实UE与Monolith完成。


- 实际编译通过后再确认真实UE Editor和Monolith身份、制作地图及UI，不能将P0清单或UFUNCTION数量当成验收证明。

## 2026-10-12 本会话真实更新

- UE5.8 `GamePlatformDeveloperTools` 编辑器模块定向构建成功（UBT：Succeeded）；真实总览关卡 `L_DBA_PluginDemoHub.umap` 已经UE生成，6个环境/出生点Actor及创建报告均落盘。
- Monolith 0.23.0 已连接真实项目编辑器；经UISpec先模拟后写入，生成并保存总览/通用覆盖层/鸟群专题共3份真实WBP，节点数分别29/9/43。三份蓝图由Monolith重新编译均为无错误/无警告，再回读父类、WidgetTree、磁盘状态确认。新增文字绑定变量重新保存。
- 主界面使用项目层原生控件事件适配提供目录过滤、层级选择、插件选择、真实场景选择、请求打开/返回、状态与手动刷新；鸟群参数仍由已有真实世界子系统执行。新版C++桥接的原生编译尚未获得成功报告，不能认定UI点击已在PIE验证。
- 演示启动脚本增加Hub WBP存在性拒绝条件；静态回归已运行11项通过，另新增资产与绑定测试待执行。用户原来的正式登录地图、Shipping/Test/Server开发资源隔离约束保持不变。
- 后续剩余：原生模块和全部目标完整构建、PIE与Debug/PluginDev真实进退、天气等第二个完整样板、其余插件功能单独场景、PS7启动脚本实跑、正式/开发Cook审计以及人工视觉验收。其他会话持续作业不可强行清理。

