# Task 5-PCG：平台Gold地图创作路径参数化实施报告

## 结论与验收边界

本子任务已完成平台Gold地图创作路径的最小参数解耦，以及不启动UE的静态、PowerShell语法/实参AST和并发保留验证。平台PCG源码不再持有`/DBAWorldPack_Village/`挂载点；项目工具继续显式提供原蓝图根。没有新增插件、模块、Settings UObject或公开API，也未改变Gold资源身份、11项中立领域/形状清单和既有不覆盖规则。

新增3个真实生产路径Editor Automation用例已放入既有编辑器模块的`Private/Tests`，本子任务没有编译或运行它们。本文的“通过”仅指所列静态/脚本检查，不能解释为UE编译、UObject加载、地图保存重开、PCG生成、G01～G16、G09双进程或Cook通过。UE验证由Root统一安排。

## 工作空间、规范与并发基线

- 远程项目：`agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615`。
- 实际源码根：`E:\poject\feebooz\DivineBeastsWorkspace`；分支`main`。
- 已读取根`AGENTS.md`、本任务brief、总体规划的分层/PCG/交付条款、插件开发规范的公开边界/端侧/内容/中文说明条款，以及现行整合交付说明。仓库规则文件搜索仅发现根`AGENTS.md`。
- 首次观察HEAD：`7d44194333cda24f8a14495695e1d478deef5a97`，当时已有其他PCG工作的未提交源码与脚本。
- 在本子任务首次源码写入之前，外部工作把HEAD推进至`f2cf1b6fcdcaa68ecb9c96e7cabef62fa4d0d740`，此前PCG增量已进入该提交。Root亦独立观察并确认。本子任务未执行commit、push、reset或回退该提交。
- 所有既有文件修改均采用canonical `edit_project_files`，使用fresh `read_files.read_revision`及精确锚点；没有整文件重写共享源码。

原始快照目录：

`Saved/Validation/PluginArchitectureOptimization/20261012/BeforePCG/Task5-20261012T033654Z/`

其中`files/`、`manifest.json`保存8个直接相关文件的原始字节、大小与SHA-256；`protected-files/`、`protected-manifest.json`另保存15个原有PCG变更文件（部分重合），合计19个不同既有文件。两次快照均检查读后内容未变化。`head.diff`、`worktree.diff`、`staged.diff`与`status.txt`记录快照时Git状态，因外部提交已完成而为空；没有把它们误称为首次观察时的dirty diff。`external-baseline-transition.diff`单独保存两个已观察提交之间的PCG历史增量。

实施后另一PCG任务在`Tools/Unreal/PCG/README.md`末尾追加了Village正式资源准入说明。本任务完成时的8817字节README完整保留为当前文件前缀；外部新增1565字节另存为`Task5PCG/readme-concurrent-append.bin`并绑定SHA-256。曾观察到该外部追加带EOF空行、使`git diff --check`退出2，随后外部作者去除该空行；本子任务未修改追加段落，当前完整范围检查退出0。相关负向观察与后续通过证据均保留。

## 修改和新增文件

|状态|文件（相对工作空间根）|本次职责|
|---|---|---|
|修改|`Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.cpp`|移除平台项目默认根；增加生产路径规范化；Create/Repair/Preflight显式传根，保留其他算法与阶段|
|修改|`Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.h`|私有模块入口声明与完整中文输入/错误/资产边界说明|
|修改|`Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldAssetsCommandlet.cpp`|仅Map/RepairMap解析并校验BlueprintRoot，在创作路由前返回参数错误；补齐Stage缺参提示|
|修改|`Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1`|由项目侧单个默认值向基础蓝图创建及Gold地图两个实际原生调用注入根目录|
|修改|`Tests/Architecture/VerifyPCGGoldAuthoring.py`|增补平台无项目默认值、参数链、规范化入口、阶段范围、项目默认注入与稳定输出身份的静态合同；保留原断言|
|修改|`Tools/Unreal/PCG/README.md`|新增参数、兼容行为和验证边界说明；原空间探针/预览/确定性说明以及并发新增准入说明均保留|
|新增|`Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Tests/PCGGoldAuthoringTests.cpp`|3个生产路径Editor Automation回归|
|新增|`Docs/Implementation/PluginArchitectureOptimization/task-5-pcg-report.md`|本报告|

没有删除、移动或重命名文件。新增受版本管理文件仅为上表两项，已交Root统一登记总体目录规划；本子任务未编辑共享总体目录规划、ExecutionProgress或PluginDeliveryManifest。`Saved/Validation/.../Task5PCG`中的验证脚本、快照、日志和JSON属于临时验证证据。

## 接口、行为和兼容性

### 生产输入链

私有入口现为`Create(const FString& BlueprintRoot, FString& Error)`与`Repair(const FString& BlueprintRoot, FString& Error)`；两者向Preflight显式传入根。Preflight先调用`NormalizeBlueprintRoot`，成功后使用规范结果组装`BP名称.BP名称_C`类路径，继续逐项检查真实蓝图、正确父类、批准领域、Graph以及实例化图标记。没有用合法路径冒充资源已经存在。

`NormalizeBlueprintRoot`属于同一Editor模块私有头，不扩展平台公开API。它在复制输入后清空旧输出与错误，先修剪首尾空白，再使用UE5.8 `FPaths::NormalizeDirectoryName`处理目录分隔符及可选单个尾斜杠；随后用`FPackageName::IsValidLongPackageName(Normalized + TEXT("/BP_PCG_Path"), false, &Reason)`校验目录内包名的实际形状和已注册挂载点。探测包名沿用既有Foundation蓝图创建入口的命名。空值、相对/磁盘路径、对象路径、非法字符、重复斜杠和未挂载根不会被宽松改写成可接受路径；成功只输出一个尾斜杠，失败返回中文原因并清空旧根。该函数不访问或保存资产。

Commandlet只对大小写不敏感的Map/RepairMap阶段解析`-BlueprintRoot=`，缺参等同空输入，非法输入以明确参数错误和退出码1返回，发生在任何创作函数调用之前。Preflight另有同一校验，以保护直接调用私有创作入口的路径。

|阶段|是否要求BlueprintRoot|兼容行为|
|---|---|---|
|Definitions / Realized|否|固定开发定义与图创作合同保持原状|
|Map|是|直接调用者必须补充根；项目统一脚本自动提供原默认值；已有地图仍拒绝覆盖|
|RepairMap|是|直接调用者必须补充根；仍只修复已有唯一Gold地图，完整校验后备份并保存|
|Verify / SpatialProbe / GeneratePreview|否|函数签名和原命令路由保持原状，不引入无用输入|

项目`RunGoldLevelAuthoring.ps1`仍负责Village固定资产盘点和插件装配，因此只把原项目默认根提升为一个项目侧`$blueprintRoot`常量，供基础蓝图与原生Map阶段共用；未开放一个与Village文件盘点不一致的伪通用覆盖参数。跨项目调用方可直接向平台Commandlet提供自己的已注册目录。已有显式Python兼容创作链未改动。

中立`GoldRoot=/Game/Development/Foundation/PCG/`、`MapPackage=/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1`、GetPlacements内11项清单、空间快照、权威/碰撞边界、预览清理、稳定ISM指纹及不覆盖检查均保持原实现。这里没有迁移任何uasset/umap或反射身份。

### 引擎合同核对

已直接读取锁定`F:\UnrealEngine-5.8.0-release`的`Runtime/CoreUObject/Public/Misc/PackageName.h`声明、`PackageName.cpp`合法包名实现及`Runtime/Core/Private/Misc/Paths.cpp`目录规范化实现。证据为`Task5PCG/engine-path-api.txt`。这是源码合同核对，不是引擎执行证明。

## 实际执行的检查

命令均从工作空间根执行。Python实际解释器为`C:\Users\Freebooz\AppData\Local\Programs\Python\Python314\python.exe`，短命令中的`python`表示该解释器；PowerShell使用已准备的7.6.6便携版本。

|检查|真实命令|结果与证明范围|
|---|---|---|
|修前负向静态回归|`python Tests/Architecture/VerifyPCGGoldAuthoring.py`|退出1；新增断言准确定位GoldMapAuthoring.cpp中的旧平台项目挂载点，证明回归可捕捉目标问题|
|修后Gold创作静态合同|`python Tests/Architecture/VerifyPCGGoldAuthoring.py`|退出0；输出`PCG_GOLD_BLUEPRINT_ROOT_STATIC_CONTRACTS_PASS`与原`PCG_GOLD_AUTHORING_STATIC_CONTRACTS_PASS`；原12模板/7子图/11蓝图/17定义/3实际图/3配置合同仍通过|
|已有空间探针源码门禁|`python Tests/Architecture/VerifyPCGGoldSpatialProbe.py`|退出0；输出`PCG_GOLD_SPATIAL_PROBE_SOURCE_PREFLIGHT_PASS`，原10个固定取样案例与11类源码约束仍通过；未运行真实空间探针|
|PowerShell语法及实际调用参数AST|`Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoLogo -NoProfile -NonInteractive -File Saved/Validation/PluginArchitectureOptimization/20261012/Task5PCG/ValidateTask5PowerShell.ps1`|退出0；解析3个PCG脚本，验证2个原生调用实际连接同一项目根；没有执行创作入口或启动UE|
|原字节、精确编辑及并发追加保留|`python Saved/Validation/PluginArchitectureOptimization/20261012/Task5PCG/ValidateTask5Preservation.py`|最终退出0；5个既有文件精确等于原快照加记录编辑，README精确等于同样结果加固定的已观察外部追加，新测试内容一致；13个未修改保护文件SHA-256全部相同|
|空白差异门禁|由上述保留验证脚本执行`git diff --check --`，后接`guarded-edit-plan.json`中7个变更文件的完整路径数组|最终退出0；参数列表及输出均保留；Git另提示现有autocrlf策略今后可能把LF转换为CRLF，本次校验确认原文件编码/BOM/换行未被额外改写|

验证目录：`Saved/Validation/PluginArchitectureOptimization/20261012/Task5PCG/`。主要证据：

- `static-before-implementation-utf8.log`：目标问题的真实修前失败。
- `VerifyPCGGoldAuthoring.log`、`VerifyPCGGoldSpatialProbe.log`、`static-results.json`：修后静态结果和完整解释器/命令参数。
- `ValidateTask5PowerShell.ps1`、`powershell-validation.log`、`powershell-result.json`：可重复执行的PowerShell只读检查及结果。
- `guarded-edit-plan.json`、`ValidateTask5Preservation.py`、`preservation-results.json`、`preservation-validation.log`：原快照加精确修改的逐字节重建与实际SHA-256。
- `readme-concurrent-append.json/.bin`、`readme-external-after-task5.md`：外部README追加的独立快照与固定哈希；完整当前README SHA-256为`3cab24f76dca87caaa00a178a9a072ad2675fdd287c1fc6cac294a2d983350d2`。
- `preservation-results-concurrent-readme-detected.json`、`preservation-validation-concurrent-readme-detected.log`与`git-diff-check-after-concurrent-readme.log`：并发追加被发现、原内容保留及后续恢复通过的过程，未删除中间失败。
- `task5-tracked-increment.diff`、`git-diff-check.log`、`callsite-audit.log`、`engine-path-api.txt`：变更、空白门禁、调用方和实际引擎接口证据。tracked diff包含已观察README外部追加；本任务自己的精确边界以guarded-edit-plan为准。

两次最初的验证封装遇到Windows Python默认GBK编码：一次在输出修前异常时失败，一次在把UTF-8 stdin写入临时PowerShell验证脚本前失败。后续显式使用UTF-8 stdin/stdout后执行了上表真实命令；这两次封装错误未记为业务测试通过，也未修改项目源码或资产。

## 新增但本子任务未执行的UE回归

|完整Automation名称|覆盖内容|状态|
|---|---|---|
|`GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.Normalization`|5种正常输入：标准目录、无尾斜杠、首尾空白、Windows反斜杠、另一个中立目录；成功清理旧错误、规范结果幂等|源码已新增；未编译/运行|
|`GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.InvalidInputs`|11种空值、非法路径、未挂载根与只读Script根；失败清空旧输出且提供错误|源码已新增；未编译/运行|
|`GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.RequiredStages`|通过真实Commandlet Main覆盖Map/RepairMap及大小写变体，共16组缺失、空值、相对路径和对象路径输入；预期参数错误与退出码1|源码已新增；未编译/运行|

仍待Root在隔离且符合项目进程约束的UE5.8环境完成：受影响Editor模块实际编译链接、三项新增Automation的发现与运行、合法已挂载蓝图根的真实资源预检、项目默认Map链及受控RepairMap的适用资产验证。地图创建、修复或其他资产写入必须继续遵守既有占用/备份/不覆盖门禁；本次没有为了测试启动这些动作。现有Verify、SpatialProbe、GeneratePreview/G09和Cook不因本次静态检查自动取得新运行证据。

## 中文说明审核与剩余风险

已同步审核本次新增私有入口、Preflight输入校验、Commandlet路由、项目根默认值、测试目标/前置条件/失败意义及README兼容说明。修正了私有头原先把整个入口笼统表述为“不执行Generate”的说明，明确区分创作/修复与既有受控生成预览的临时资源所有权。未进行全插件历史私有实现的全量中文审计。

剩余风险主要是未执行的UE编译与运行验证，以及仍在继续的其他PCG工作可能产生新版本。最终保留验证绑定当前源字节和已观察的外部README追加，不会自动接受将来的任意变化；后续同步或构建应重新核对相同SHA-256。直接调用旧Map/RepairMap命令而不提供BlueprintRoot现在会有意失败，外部调用方需按说明迁移；项目统一入口已兼容原默认资源。

本子任务没有关闭Editor PID 28168，没有启动UE、UBT、Cook或其他资产处理进程，没有提交、推送或操作其他任务的工作树。
