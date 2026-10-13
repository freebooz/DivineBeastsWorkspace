# 脚本与命令文件审查清理记录（2026-10-12）

状态：已完成本轮审查和确认退役项的清理。共盘点360项脚本及构建入口，清理7个脚本，保留353项。两个清理前已有的静态校验失败仍保留，不宣称全工程测试通过或全部潜在闲置文件已被穷尽。

## 一、范围与执行边界

固定 DivineBeastsWorkspace（神兽联盟工作空间），通过 HWebCodex（代码工作区连接器）执行。初始 Git（版本控制）提交为 `ecb9d85617947b83996dafd8914fab15e8c252e2`，main（主分支）已有其他任务修改。未提交、推送、重建工程、停止他人进程，未删除 `.uasset`／`.umap`、模型、纹理、材质、源美术或既有发布包。

用户授权清除确认不再使用的脚本。判断依据不是文件年龄、零字符串引用或没有上层调用，而是结合源码职责、明确替代路径、实际入口及恢复能力。构建、测试自动发现、手工资产作者、迁移/修复与部署入口保持保守。

本轮存在两个同任务审查会话：本会话直接清理5项；另一会话清理的2项经本会话独立读取Git原文、检查替代入口和引用、匹配初始文件摘要后合并为总数7，不重复删除、不擅自撤销他人的清理。另一会话记录见 `ScriptCleanupAudit_20261012.md`。

## 二、分步计划完成情况

- [x] P0：读取工程规则、总体规划及插件规范，确认Git基线、未提交修改、目录级规则和运行进程。
- [x] P1：建立完整一方脚本/命令入口清单及保守引用索引，区分代码配置、文档、手工入口、测试发现、资源再生与忽略区生成物。
- [x] P2：精读退役候选和正式替代链，检查本机可读取计划任务、服务、进程、启动项与Git钩子；补充UE二进制资产中的候选文件名检索。
- [x] P3：保存原始字节、SHA-256（文件摘要）、Git基准及引用证据；经版本快照保护删除5项，并独立核验同任务另外2项，合计7项均有精确备份。
- [x] P4：清理前后运行相关测试，解析剩余脚本，复核353项保留文件字节未变化；独立UE进程回读三种鸟类资源及两张审核地图，无资源写入。
- [x] P5：同步总体目录、旧计划退役注记、删除与保留清单、验证边界、备份和恢复说明。

## 三、盘点结果

初始360项包括225个Python脚本、100个PowerShell脚本、9个PowerShell模块以及26个CMakeLists构建入口；其中334项是脚本。范围为Git跟踪文件及未被忽略的新脚本，包含3个未跟踪Python脚本，保护8项已有未提交或并发改动脚本。

共扫描3257份仓库文本，按完整文件名和模块名查找引用；另外分析动态执行位置，避免把模块导入、测试目录发现和明确列出的作者链误作无引用。未发现字节完全相同的脚本副本。

补充只读检索1212份Git跟踪的UE资产，检查候选名称的UTF-8和UTF-16形式；未发现候选引用。此项只是原始字节辅助证据，不是完整反序列化、压缩载荷或动态编码调用证明。本机可读取的调用入口检查无候选匹配，不能扩大为其他机器的保证。

另外盘点1498项被忽略的脚本和命令文件，主要位于Saved（生成/验证证据）、构建产物及历史快照。本轮没有清空这些目录，也没有认定其中每个文件都不再使用。一方正式维护范围未发现独立 `.bat`／`.cmd`／`.sh`；此类文件主要存在于忽略区生成产物。

## 四、实际清理的7个脚本

### 1. 旧鸟群预设与蓝图作者

`Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockProfilesAndBlueprint.py`

早期ISM（实例化静态网格）方案使用旧Actor父类、独立Profile以及旧厘米模型引用，与已采用的世界特效定义和项目场景组件不符。同名正式蓝图已由 `AuthorBirdFlockSceneBlueprint.py` 创建，物种定义由 `AuthorBirdFlockDefinitions.py` 管理。清除此旧入口，避免重新引入过期架构。

### 2. 旧鸟群审核地图作者

`Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockReviewMap.py`

依赖旧ISM属性及Profile，且地图目标与现行审核场景重复。替代为 `AuthorBirdFlockNiagaraReviewMap.py` 和 `AuthorBirdFlockIntegrationReview.py`，分别承载真实Niagara资产审核和正式表现链路审核。

### 3. 旧鸟群图形捕获试验

`Tools/Unreal/VFX/BirdFlock/CaptureBirdFlockGPUScene.py`

原SceneCapture（场景捕获）试验生成的重复/空白画面路径已在P4—P7执行记录中被否决。现行 `RunBirdFlockGPUReview.py` 和 `RunBirdFlockIntegrationReview.py` 使用独立图形验证和实际粒子读回，旧工具不再作为验收入口。

### 4. 鸟群初期反射打印探针

`Tools/Unreal/VFX/BirdFlock/InspectNiagaraEditorAPI.py`

只打印最初调查的Editor Python能力，不创建或验证正式资产。当前已有明确的Monolith制作协议、Niagara作者与 `ValidateBirdFlockP4Assets.py` 实际资产验证链，初期打印探针退役。

### 5. 硬编码Python执行标记探针

`Tools/Unreal/Characters/TestExecutePython.py`

仅在个人硬编码路径写入“ok”，没有真实业务或测试断言。当前正式角色作者和独立UE资产验证已经覆盖脚本执行能力，无需继续保留该早期标记入口。

### 6. 破坏性旧前端地图生成器

`Tools/Unreal/FrontEnd/GenerateFrontEndMaps.py`

独立回读原始源码确认：既有前端或角色工作室地图存在时，直接调用delete_asset（删除资源），随后重建原型地图。此行为会覆盖当前已制作的场景，已被 `UpdateCharacterPreviewStudio.py` 与 `ImportZodiacSanctuary.py` 的受控更新/导入路径替代。由同任务并行会话删除，本会话复核后纳入统一清单。

### 7. PCG初期反射打印探针

`Tools/Unreal/PCG/InspectGoldReflection.py`

只枚举初期字段及类型，不产出资源或执行完整断言。正式 `ValidateGoldLevelAssets.py` 与 `RunGoldLevelAuthoring.ps1` 已承接定义、蓝图、地图检查和作者链。由同任务并行会话删除，本会话独立核验原文、替代链及备份后纳入统一清单。

以上7项原始脚本总计29292字节，均可按文件恢复。构建、发布、部署、源资源作者和全部测试文件未被本次删除。

## 五、保留与不确定项

保留353项：218个Python脚本、109个PowerShell脚本/模块、26个CMakeLists构建入口。初始保留文件的SHA-256在最终复核中全部一致，没有通过顺带重写文件扩大任务。

其中21项仍归“人工入口或用途待进一步确认”；其他大量没有直接代码调用的文件属于资产生成、回归测试、模块或文档指引入口。本轮全部保留，不把零直接引用当作闲置结论。

全量逐文件清单为 `Saved/Validation/ScriptCleanup/20261012/ScriptAuditInventory.md`；机器可读引用、分类及最终处置为同目录 `ScriptUsageAudit.json`。

## 六、实际验证结果

清理后218个Python脚本AST（语法树）解析成功，109个PowerShell脚本/模块解析成功，语法错误0。353项保留文件无意外缺失、无字节改动；7份原始备份摘要匹配，剩余代码/配置对7个清理脚本的可检索引用为0。历史文档中的名称保留为退役及恢复证据，不再作为有效执行指引。

鸟群发布策略测试18项通过；发布脚本门禁连接3项通过。两组测试均在清理前后执行，结果一致。

独立UE5.8编辑器进程执行保留的 `AuditBirdFlockDelivery.py`，三种鸟类正式资源、引用及两张审核地图回读通过，进程退出0，源资产前后摘要一致。旧7个鸟类模型仍保留，本次未保存或改写任何地图、Niagara、材质或纹理。该检查不等于GPU运动、完整构建、资源烘焙或人工美术验收。

以下两个静态校验在清理前已失败，清理后仍以同一原因失败，未删除或降低断言：

- `Tests/Assets/ValidateBirdFlockIntegrationSource.py`：要求当前头文件含 `SubmitPendingBirdFlocks`，断言不满足。
- `Tests/Architecture/VerifyPCGGoldAuthoring.py`：要求当前作者脚本含指定 `EnablePlugins=PythonScriptPlugin` 字面量，断言不满足。

这两个存量校验与现行实现的对应关系应单独审查；本次不能据此宣称全工程测试全部通过。未执行全量UE重新构建或新Cook（资源烘焙）。

## 七、证据和恢复

统一证据目录：`Saved/Validation/ScriptCleanup/20261012`。

`DeletionManifest.json`记录7项删除原因、正式替代、原始文件摘要、基准提交、备份路径以及5项直接删除/2项并行删除后复核的来源。`FinalVerification.json`记录最终范围校验。`BaselineTests`与`PostTests`保留前后成功及失败的原始日志；`UEAssetAudit`保存真实UE进程和资产报告。

原始字节保存于 `Backup/<原项目相对路径>.bak`。例如 `Backup/Tools/Unreal/FrontEnd/GenerateFrontEndMaps.py.bak`。PCG探针的Git规范化换行与原工作副本不同，已按初始摘要恢复原CRLF字节并验证一致，不把规范化字节误记为原始备份。

只读查阅某个历史文件可使用：

```text
git show ecb9d85617947b83996dafd8914fab15e8c252e2:<项目相对路径>
```

如需恢复，应先核对是否已有同名新版本或外部实际需求，再从备份逐文件恢复；不要对整个工作空间执行reset/clean（重置/清空）。旧前端生成器具有删除正式地图的行为，不应恢复后直接运行。

总体目录文档原有混合换行导致结构化编辑被拒绝时，采用已观测SHA校验、独占文件锁和精确字节插入，只新增清理记录，保留其他任务已有文档内容。
