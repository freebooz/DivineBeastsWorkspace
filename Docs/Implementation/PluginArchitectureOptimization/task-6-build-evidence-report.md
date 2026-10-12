# Task 6 P1：完整 Editor 构建证据与 UE 测试入口

## 状态与责任边界

实现、独立审查发现的源码范围修补与纯脚本回归已完成。最终合同回归为 **38 / 38 通过，exit 0**，结果时间为 2026-10-12T05:04:15.0759550Z，job 为 `wc_job_3uZlNOYtYIjbvI1X`。该回归使用明确的合成源码、receipt、DLL 和进程记录；runner 副本保留真实前置门禁，在进程启动调用点加入始终失败的测试哨兵。结果包含 `FixturesOnly=true`、`ActualBuildExecuted=false`、`UEExecuted=false`，不能替代实际 Editor 构建或 UE 自动化成功证据。

本轮未启动 UE、UBT、Cook，未读取或修改主代理的 `.worktrees/v`，未停止其他进程，未提交或推送。真实完整 Editor 目标和随后 Data/UI/VFX/PCG 的 UE 验收由主代理在授权验证工作区执行。旧构建结果不能事后补成合格证据；必须使用本轮 builder 再成功执行完整 Editor 目标。

本报告只覆盖 P1 构建到测试的证据链。manifest、精确测试身份通用报告门禁与相关回归由其他实现代理负责；共享执行台账和目录规划由主代理维护。

## 修改文件

| 文件 | 作用 |
| --- | --- |
| `Build/Game/BuildFoundation.ps1` | 在完整 Editor 真实进程前后采集项目源码清单，成功后写入合格证据；保持原目标顺序、资源参数和外部非零码传播。 |
| `Build/Game/FoundationBuildEvidence.psm1` | 小型同步 helper：源码清单、receipt 产物与模块映射、进程身份、引擎工具指纹，以及测试前完整证据核验。 |
| `Build/Validation/TestPluginArchitectureUnreal.ps1` | 新增 `-BuildResultPath`；在锁和 UE 启动前核验完整构建，全部组结束后再次核验。 |
| `Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1` | 38 个隔离合同用例；runner 副本保留真实前置门禁并在 UE 调用处加测试哨兵，检查失败分类、未创建 Suite 目录和未创建 UE 进程；删除门禁的变异必须被识别。 |
| 本报告 | 设计、范围、真实脚本结果、验收接口和限制。 |

## 设计与判断

### 1. 真实完整目标是资格前提

沿用 Foundation 已有 RunId、独占证据目录、进程启动和 result.json 机制。builder 只在 `Editor` 且 `OnlyModules` 为空时采集资格快照；`OnlyModules` 仍可用于原有模块构建，但其结果明确为 `NotEligibleOnlyModules`。

完整 Editor 的实际 `Invoke-FoundationProcess` 返回 0 后，才读取构建后源码和实际 receipt。无法生成完整证据、源码在构建前后发生变化、receipt/DLL/模块清单缺失或不一致，均使本次 builder 失败，而不是给旧产物补证。

结果中保留真实目标进程的 ExitCode、PID、StartTimeTicks、TimedOut、Cleaned 和持续时间；核验本次 `Editor/process.json` 的宿主路径、实际 UBT DLL、完整目标/平台/配置、唯一工程路径和进程身份。局部 `-Module`、`-SingleFile`、`-SpecificFiles` 参数不能取得完整目标资格。

测试入口要求整个 Build 结果为 `Passed / 0`，且唯一 Editor 子目标成功。即使 Editor 成功，后续 Client 或 Server 失败的批次也不能用作入口凭据。它接受实际成功的完整增量构建；不声称所有 DLL 在该次都重新链接，也不以 DLL 修改时间代替成功目标执行。

`UsePrecompiled`、`NoUBA`、PCH 和并行动作参数仍由既有 builder 原样处理，并记录在原有结果中。没有通过跳过项目模块、伪造 UBT 结果或修改引擎工具加载逻辑取得资格。

### 2. 项目源码快照

范围标识为 `FoundationProjectCompileInputs-v2`；旧范围的构建证据显式拒绝，必须用新范围重新完成完整 Editor 目标。覆盖：

- `Game/DivineBeastsArena.uproject`；
- `Game/Source` 下所有文件；
- `Game/Plugins` 下所有未排除文件，包括插件描述、模块规则和源码；
- `Game/Config`、`Game/Build` 下所有文件。

剪除只发生在 `Game/Plugins` 下直接含 `.uplugin` 的真实插件根，其直接子目录 `Binaries`、`Intermediate`、`Saved`、`DerivedDataCache`、`Content`、`SourceArt`、`.git`、`.vs`、`.idea` 被排除。`Docs` 保留。`Game/Source` 和实际插件根的 `Source` 子树一旦进入就完整递归，任何深度的上述目录名都不会导致遗漏，子树内的同名 `.uplugin` 测试输入也不会重新触发插件根剪除。`Game/Config`、`Game/Build` 同样不按目录名剪除。主工程资产和生成目录在这些输入根之外。

路径和重解析点检查防止清单或证据引用意外越出当前根目录。`AdditionalPluginDirectories` 或 `AdditionalRootDirectories` 非空时显式拒绝资格，避免静默漏掉外部编译输入。

每项记录规范化的工作区相对路径、字节数、SHA256 和 LastWriteTimeUtcTicks；聚合哈希按 Ordinal 路径顺序计算。构建前后必须内容清单相同，逐文件时间戳也相同；正常“改动后还原内容”的写入仍会因时间戳变化被拒绝。测试入口比较当前内容清单，不把单纯保持内容不变的文件时间戳变化当作新源码。

快照使用独占新文件 `Editor/CompileInputs.before.json` 与 `Editor/CompileInputs.after.json`，其文件哈希也绑定在 Build 结果中。输入清单保留完整条目，结果摘要无需复制数千条路径。

### 3. 按真实 receipt 取得产物

实施前只读检查了原工作区真实 `Game/Binaries/Win64/DivineBeastsArenaEditor.target` 和 `UnrealEditor.modules`。该 receipt 的 `Project` 为相对 receipt 所在目录的 `../../DivineBeastsArena.uproject`；`BuildProducts` 使用 `$(ProjectDir)`、`$(EngineDir)`，项目 `.modules` 的类型为 `RequiredResource`。helper 同时支持解析后位于项目或引擎内的绝对产物路径。

证据包含 receipt 本身及 receipt 列出的全部项目 `DynamicLibrary` DLL、全部项目 `.modules` 的 SHA256。每份 `.modules` 的 BuildId 必须匹配 receipt，模块映射必须指向 receipt 中的真实项目 DLL；全部 DLL 都必须被映射，模块名不能重复，必须包含主工程模块 `DivineBeastsArena`。不扫描目录中同名旧 DLL 来冒充 receipt 产物。

引擎身份另绑定 EngineRoot、版本文件、实际 UBT DLL/runtimeconfig、实际 dotnet 宿主和 `UnrealEditor-Cmd.exe` 的指纹。验证使用当前真实模块映射，按所选测试组要求存在：

| 组 | 必须由当前 receipt 覆盖的模块 |
| --- | --- |
| Data | `GamePlatformData`、`GamePlatformDataEditor` |
| UI | `DivineBeastsUIClient` |
| VFX | `GamePlatformVFXClient` |
| PCG | `GamePlatformPCGEditor` |

这不改变原有 16 / 11 / 3 / 3 个完整测试身份要求。模块存在是启动前提，真正成功仍要求本轮导出的每个必需身份唯一且为 Success。

### 4. 测试入口与状态

`-BuildResultPath` 可为工作区相对路径，也可为当前工作区内绝对路径；相对路径以 runner 自身的工作区根解析，不以调用者当前目录解析。结果必须位于：

`Saved/Validation/FoundationM0/<BuildRunId>/Build/result.json`

同一 RunId、工作区、工程、引擎、目标、平台和配置必须一致；源码快照文件、实际进程记录、当前源码、receipt、DLL、模块清单及引擎工具哈希必须匹配。只有这些检查通过才获取 Foundation 引擎锁并启动 UE。

| 条件 | 结果 |
| --- | --- |
| 未传 Execute、BuildResultPath 缺失、结果文件尚不存在 | NotExecuted，exit 2，未启动 UE |
| 已提供但损坏、旧格式、OnlyModules、错误根目录/引擎、陈旧源码或产物、缺少测试所属模块 | Failed，exit 1，未启动 UE |
| 实际 UE 进程非零退出 | Failed，保留原退出码 |
| 已运行 UE 后结束复核失败，包括构建结果被删除 | Failed，exit 1 |
| 本次所有必需身份唯一 Success，结束复核仍通过 | Passed，exit 0 |

结果摘要记录关联 BuildRunId、BuildResultPath、BuildResultSHA256、InputSHA256、ArtifactSHA256、输入/DLL 数量、所需模块和结束复核状态。

## 真实验证记录

### TDD RED 与最终 GREEN

实际命令（工作目录为原工程根）：

```powershell
& 'E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe' -NoProfile -File Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1
```

| 阶段 | 实际结果与证据 |
| --- | --- |
| 首个 RED | exit 1，新 helper 尚不存在；`Saved/Validation/FoundationM0/b50f423c-a309-4598-90dc-fac70c58bced/BuildEvidenceContractTests/result.json`。 |
| 首次实现验证 | exit 1，进程时间身份不匹配；`8aa94483-d6a3-4000-8850-448baaaad5a8`。定位为 PS7.6 JSON 自动日期解析后再字符串 Parse 丢失精度，改为直接 DateTime 转换并比较 ticks。 |
| 首轮 GREEN | 34 / 34，exit 0；`4f264f36-f6a5-41bf-b439-2bc946921211`。 |
| v1 交付 GREEN | 34 / 34，exit 0；`Saved/Validation/FoundationM0/eb391e13-81f7-46f4-b503-ae2f447728c3/BuildEvidenceContractTests/result.json`；2026-10-12T04:34:59.9491737Z。此版本后来发现输入范围缺口，不作为最终交付。 |
| 源码范围 RED | exit 1；`cef5887a-e5bb-4c49-97a6-f9e0e1ab1314`；旧 helper 意外接受 `NestedSourceCppAddedDuringBuildRejected`。 |
| runner 变异回归 RED | `83043f86-9406-46ef-88cd-229efc514219`、`52e617dd-d661-47f0-a3ae-3bbe6888e3be` 均为 37 项通过后末项失败，exit 1；独立运行 `b7c8bf92-f352-4354-ae08-86d6a4a8528e` 也为 37 项通过后末项失败。未将这些运行记为 GREEN。 |
| 最终 v2 GREEN | **38 / 38，exit 0**；`Saved/Validation/FoundationM0/ee396d8a-44d5-437b-8c72-0229c9ab221d/BuildEvidenceContractTests/result.json`；2026-10-12T05:04:15.0759550Z；PowerShell 7.6.6 / .NET 10.0.12。 |

用例覆盖合格结构与相对路径、完整源码范围、排除生成/资产目录、OnlyModules 两处拒绝、实际参数中的 Module、进程非零和超时、期间内容/时间戳变化、当前同名测试源码变化、新源码、插件描述变更、同名 DLL 内容变化、缺失 DLL 或模块清单、未在 receipt 中的映射、receipt 配置变化、缺少测试模块、跨工作区/引擎、整体失败批次、旧格式和损坏结果、引擎程序变化、外部插件输入拒绝。

其中五项对子进程中的 runner 隔离副本验证：未 Execute、缺 BuildResultPath、缺结果文件分别 NotExecuted/2；源码陈旧和 OnlyModules 分别 Failed/1。副本的前置门禁、helper 和 Suite 创建路径保留真实实现，只在唯一 `Invoke-FoundationProcess` 调用点替换为 `throw 'FoundationFixtureUEBoundaryReached'`。因此这属于有明确启动边界替身的合同测试，不是逐字未修改 runner，也不是实际 UE 运行。

这些用例同时要求 Suites 为空、没有 Suite 子目录且没有 UE 的 process.json。`RunnerHarnessDetectsRemovedBuildGate` 只在临时副本删除前置门禁，要求 runner 的 Message 精确为上述哨兵，并要求夹具捕获精确的“门禁前不应创建Suite目录”断言；超时或其他异常仍失败。这样既保留对接线的实际检查，也不依赖系统加载无效 PE 文件的失败时机。

### 独立审查修补与复核

独立报告为 `Docs/Implementation/PluginArchitectureOptimization/task-6-build-evidence-review.md`。初次 13 项诊断中有 3 项意外接受：当前主工程 `Source/Private/Content/*.cpp`、插件 `Source/Private/Docs/*.cpp`，以及构建期间新增前一类源码。初始证据为 `Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Initial/result.json`。实际 UE5.8 的 Source 遍历会收集这些 C++ 输入，原来的任意深度目录名排除因此不成立。

先加入可复现 RED，再将剪除限制在真实插件根；新增回归覆盖主工程和插件 Source 下全部原排除目录名的新增/修改、Source 内 `.uplugin` 不改变递归策略，以及实际插件根资产/生成目录仍被排除。生产 helper v2 已通过原 13 项独立复跑，job `wc_job_1hQFfKGWhb6Dxp6W` exit 0；证据为 `Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Final/result.json`，13 项符合预期、0 项意外接受。

变异用例曾在无效 exe 的系统启动边界停留到 30 秒，导致夹具超时；独立诊断保留了 Data 目录及空 stdout/stderr、但无 process.json/result.json，见 `Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Mutation/result.json`。另一次单例则快速返回无效应用异常并命中目录断言。因此最终仅调整测试副本的启动边界哨兵，生产 builder/helper/runner 保持冻结，随后取得上述 38 项 GREEN。

最终四文件冻结后，独立复跑 **38 / 38，exit 0**，job `wc_job_HgqLFDyQHRM7AS8p`；证据为 `Saved/Validation/FoundationM0/60794450-f78c-4cbd-a46a-b9bea1dba41e/BuildEvidenceContractTests/result.json`，时间 2026-10-12T05:06:10.1607520Z。相同最终 SHA 下再次运行原 13 项诊断，job `wc_job_4U4xpz3Wqq85BZLx` exit 0；`Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/FinalDelivery/result.json` 记录 `CaseCount=13`、`UnexpectedAcceptances=0` 及与下表完全一致的四文件 SHA。独立复核已关闭本范围发现，未继续扩充测试。

### 原工作区实际 receipt 只读兼容探测

实际通过 Cwebcodex 的 PowerShell typed script 进入上述 PS7.6 宿主，导入 FoundationTools 与 FoundationBuildEvidence，调用 `Get-FoundationCompileInputInventory` 和 helper 内部只读 `Get-FBEEditorProducts`。项目根为原 checkout，引擎为 `F:/UnrealEngine-5.8.0-release`。

v1 阶段的只读兼容 job `wc_job_1E8McBt_o6xQF-yD` exit 0，输出：

- 输入文件 1706 个，聚合 SHA256 `E2C8AB71C17FED674FF23687DFE2EA1BAAC0093F94E1E21E1E3E5FD8C87C807B`；
- 原有 receipt BuildId `cf41249f-44dc-4956-bd09-4a42f079e86b`；
- 60 个项目 DLL、33 份 .modules、60 个模块，所选组的 5 个模块全部存在；
- 探测耗时约 26.05 秒；
- 明确输出 `ExistingReceiptReadOnly=true`、`ActualBuildExecuted=false`、`BuildQualified=false`。

此项只验证 helper 能读取实际格式和真实清单，**不会**把该旧 receipt 认定为当前源码的构建成功证据。第一次临时诊断脚本漏导入 FoundationTools 而 exit 1，补全临时脚本依赖后取得上述结果，未据此修改工程实现。

### v2 输入范围只读探测

范围修复后，以最终 helper 对原工作区重新调用 `Get-FoundationCompileInputInventory`，job `wc_job_7MUbOIOTmoxpgmUx` exit 0：范围为 `FoundationProjectCompileInputs-v2`，输入 **2533** 个，聚合 SHA256 为 `F2E058E84C0C852358691AAEE2FCD6B34F0636475036D27F44C58F8E439C34E3`，约 15.44 秒。输出明确为 `ReadOnly=true`、`ActualBuildExecuted=false`、`BuildQualified=false`。这一数量与指纹是当时原 checkout 的只读观察；验证工作区仍需在实际完整构建前后生成自己的快照。

### 语法与差异检查

在 PS7.6 中对四个脚本/模块执行 PowerShell Parser.ParseFile，全部 ParseErrors=0；限定四文件运行 `git diff --check -- <四个路径>`，exit 0，仅有原有 Git 换行转换提示。初次临时诊断使用默认 PS5.1 ParseFile，未作为本项目 PS7 语法门禁；改用显式 PS7.6 后通过。没有降低 `#requires` 或更改 UBT 的加载要求。

## 主代理后续 UE 验收接口

在最终源码和本轮四个脚本已同步的 **同一授权工作区**，先执行独立完整 Editor 目标：

```powershell
& '<PS7.6.6路径>/pwsh.exe' -NoProfile -File Build/Game/BuildFoundation.ps1 -Editor -EngineRoot 'F:/UnrealEngine-5.8.0-release' -RunId '<BuildRunId>'
```

按主代理已核实的构建环境可显式补充 `-UsePrecompiled`、`-NoUBA` 等原有资源参数，但不能传 `-OnlyModules`。Data 插件在现有 uproject 已显式启用，无需为了 Data 增加 AdditionalPlugins。

只有该独立 Build 的整体 result 为 Passed/0 且含合格 EditorBuildEvidence 后，才能运行：

```powershell
& '<PS7.6.6路径>/pwsh.exe' -NoProfile -File Build/Validation/TestPluginArchitectureUnreal.ps1 -Execute -Suites Data -EngineRoot 'F:/UnrealEngine-5.8.0-release' -BuildResultPath 'Saved/Validation/FoundationM0/<BuildRunId>/Build/result.json'
```

省略 `-Suites` 即按 Data、UI、VFX、PCG 顺序验证全部 33 个既有必需身份。不得用原 checkout 的结果替代验证工作区的完整构建，或把 Editor 成功但 Client 失败的混合批次改写为合格结果。根台账应分别记录新的完整 Editor 构建和随后 UE runner 的实际结果。

## 范围限制与疑虑

证据是本机可审计文件，未做数字签名或对恶意本地写入者的远程证明。它证明限定项目源码端点稳定、真实完整 UBT 目标成功、当前产物与该次 receipt/指纹相符，不声称能检测恢复全部内容及时间戳的任意瞬时写入。

源码范围明确不包含引擎源码、SDK、所有引擎运行 DLL、外部输入根，以及被上述结构规则排除的真实资产、生成目录和预编译依赖目录；位于已进入 Source 子树的同名目录仍完整纳入。引擎记录的是版本/关键工具身份；`UsePrecompiled` 的既有引擎依赖限制仍成立。若以后工程引入外部插件或其他未覆盖的编译输入，应先扩展范围版本再取得新证据。

实际完整目标使用 UBT 的正常增量判定，完整成功不要求所有 DLL 本轮重新链接。生产验收应保持授权工作区输入冻结，并在同步后运行完整目标。资产、视觉、PIE、Cook 和 UCAS 载荷完整性均不由这组纯脚本回归证明。

## 交付快照

2026-10-12T04:39:30.6456613Z 创建的 `Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidence/Delivery` 保留为 v1 历史快照，关联当时 34 项结果，未被覆盖。

最终交付为 **`Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidence/Delivery-v2`**，创建于 2026-10-12T05:05:19.7795380Z。目录内保存四文件的原始字节及相对路径、`manifest.json` 和 `source-sha256.json`。复制前后分别验证源文件与副本 SHA256，均与下表相同。manifest 绑定 v2 范围、实际 38 项结果路径/时间/文件哈希、报告及执行边界；该结果文件 SHA256 为 `BC7A627738318D2CC7A10311B588F8A33AC73857AD7BF816B4FE84D1B56ABD8B`。交付快照不是 Editor 构建凭据，主代理必须同步此版本后使用新 RunId 执行完整目标。

## 本轮同步用 SHA256

以下为最终脚本回归和 PS7 语法检查时的精确文件字节哈希；独立审查若提出修补，应使用修补后的更新值。

| 文件 | SHA256 |
| --- | --- |
| `Build/Game/BuildFoundation.ps1` | `FE982299F233420B783A9E7C89E7C25B49C2734840F54EB1749CAF70CE827F16` |
| `Build/Game/FoundationBuildEvidence.psm1` | `E386D1C78180FD154C0EB4788E0DD4CA560071A083D536F4B61544B3D28747F0` |
| `Build/Validation/TestPluginArchitectureUnreal.ps1` | `8DB8CC2142E9E8CCB575520DA7DEAB71F6B6F4C02A0BB52A4E924EC2035E4C9D` |
| `Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1` | `2EA9BDA6735E8363D81E4A604965181E8603C15B62220D6AB93B45AE905C1D91` |
