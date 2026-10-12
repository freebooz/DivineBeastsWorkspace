# Task 6：完整 Editor 构建证据独立复核

## 审查结论与当前状态

**结论：发现的 P1 已在范围 v2 修订中关闭；最终独立 13 / 13 边界回归、38 / 38 合同回归均为 exit 0，未发现其他确定阻塞。** 真实 UE 构建和 Automation 不属于本次已执行结果，仍由主代理验证。

初始最终快照存在 1 个 P1：源码清单按任意深度的目录名称排除 `Content`、`Docs` 等目录，导致真实 UBT 会编译的项目/插件 C++ 未进入清单。新增此类源码后，旧 DLL 仍可通过完整 Editor 构建证据门禁；构建期间新增同类文件也未阻止签发证据。下文保留初始复现与最终修补关闭过程。

初始审查和修订关闭均在实现代理明确交付相应最终快照后进行。问题确认后已通知主代理及实现代理，由原实现代理限定修补；审查者随后只读核对差异并独立复跑，全部最终结果见关闭节。

审查者未修改生产源码；只在原工作区 Saved 下写入专属诊断夹具，并写本报告。未启动 UE、UBT 或 Cook，未触碰主代理专用 `.worktrees/v`，未停止其他进程，未提交或推送。真实完整 Editor 构建与 UE 自动化验证由主代理负责。

## 审查对象与最终快照

工作区：`E:/poject/feebooz/DivineBeastsWorkspace`。锁定引擎只读核对根：`F:/UnrealEngine-5.8.0-release`。

在 `/root/implement_server_policy` 明确通知“现在可以独立审查最终代码快照”后，读取四个实现文件及实施报告；其后代理说明原始文件已冻结于 Delivery，本次初始复现使用的四文件 SHA256 与该交付一致。

| 文件 | 初始最终 read_revision | 初始最终 SHA256 |
| --- | --- | --- |
| `Build/Game/FoundationBuildEvidence.psm1` | 7555604770030121 | `4829062594D1220174C7CA46CAE610D7F1A0584A51EBC590BD775CD771DE74C9` |
| `Build/Game/BuildFoundation.ps1` | 7555604770030116 | `FE982299F233420B783A9E7C89E7C25B49C2734840F54EB1749CAF70CE827F16` |
| `Build/Validation/TestPluginArchitectureUnreal.ps1` | 7555604770030124 | `8DB8CC2142E9E8CCB575520DA7DEAB71F6B6F4C02A0BB52A4E924EC2035E4C9D` |
| `Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1` | 7555604770030120 | `011C3A112DA2B20B62D8AD48A518180CF9A48D7C04CA367F94FB332B7C7CEB4B` |

实施说明为 `Docs/Implementation/PluginArchitectureOptimization/task-6-build-evidence-report.md`，初次读取 revision 7555604770030134；实现代理后续增加 Delivery 章节，说明四个实现文件未随报告修改。另只读核对 `FoundationTools.psm1` 中实际进程参数、退出信息、证据写入和锁行为，以理解接线；未扩大为该稳定模块的全面审计。

## P1：源码子树中的同名目录被误当成资产或生成目录

### 文件与触发条件

旧版 `Build/Game/FoundationBuildEvidence.psm1:12,89–97` 定义全局排除目录名，并在每一层递归中直接使用 `$item.Name -in $script:ExcludedDirectories` 跳过目录。该判断同样作用于 `Game/Source`、每个插件的 `Source`、`Game/Config` 和 `Game/Build` 子树。

三个最小触发均只调用原生产 helper、复用原合同测试中明确标识的合成夹具函数：

1. 在完整证据创建后新增 `Game/Source/DivineBeastsArena/Private/Content/ReviewAdded.cpp`；
2. 在完整证据创建后新增 `Game/Plugins/Fixture/Source/FixtureRuntime/Private/Docs/ReviewAdded.cpp`；
3. 在构建前快照和重新采集的构建后快照之间新增 `Game/Source/DivineBeastsArena/Private/Content/ReviewDuringBuild.cpp`。

这些目录位于正常模块 Source 内，并不是项目或插件根部的资产目录。真实 UE 5.8 源码表明：`UEBuildModuleCPP.cs:2958–2967` 从模块目录开始收集输入；`3042–3056` 除平台排除目录和 `.ubtignore` 外递归子目录；`3113–3116` 收集 `.cpp`。对应 `UEBuildPlatform.cs:420–423` 的排除集来自其他平台目录名，不包含一般名词 `Content` 或 `Docs`。以上位置位于 `F:/UnrealEngine-5.8.0-release/Engine/Source/Programs/UnrealBuildTool/Configuration/`，为本次实际只读核对的引擎源码。

### 实际观测与影响

原生产 helper 对三个变化都给出相同输入 SHA256：

`B5E0BE167BDBA94785C9A220C31A32B4FAAA63B280E36EF69BD485321D0CB776`

每个新增源码的清单匹配数为 0。前两项的 `Assert-FoundationEditorBuildResult` 返回成功，第三项的 `New-FoundationEditorBuildEvidence` 返回新证据。普通 `Private/ReviewAdded.cpp` 对照正确拒绝，证明并非诊断缺少实际门禁调用。

因此，“当前源码与 DLL 所属构建相同”和“构建前后源码不变”的资格保证在合法源码路径下失效；原有 34 项 GREEN 不能覆盖此风险。这是常规项目目录布局导致的证据缺口，与恶意伪造证据或签名模型无关。

初始复现证据：

- 命令：`& 'E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe' -NoProfile -NonInteractive -File 'Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Initial/ReviewBuildEvidence.ps1'`
- Runner job：`wc_job_DpDyObxtGDQeWJ5x`，实际 exit 1；13 项中 3 项意外接受、10 项符合拒绝预期，stderr 为空。
- 时间：2026-10-12T04:44:07.0970181Z。
- 完整结果：`Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Initial/result.json`。
- 该目录保留独立诊断脚本、每项独立合成夹具、源码 SHA256 与 PowerShell 版本。结果明确为 `FixturesOnly=true`、`ActualBuildExecuted=false`、`UEExecuted=false`。

### 最小修复建议

按结构边界定义排除范围：`Game/Source` 和每个插件 `Source` 子树完整纳入；`Game/Config`、`Game/Build` 不按任意目录名剪除。只有在识别到真实 `.uplugin` 所在插件根时，才跳过其直接资产、生成或元数据目录；不跳过任意深度的同名源码目录。保持现有项目根输入范围，不扩展为 SDK/引擎源码或签名防伪框架。

回归应覆盖主工程和插件 Source 内原排除目录名的新增/变化，同时保留真实插件根 Content、Binaries、Intermediate 的排除基线。旧交付保留；修订版本重新以新 RunId 执行真实完整 Editor 构建，不事后给旧 receipt 补证。

## 其他检查与独立验证

### 原有 34 项合同回归

独立运行命令：

```powershell
& 'E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe' -NoProfile -NonInteractive -File Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1
```

实际 Runner job `wc_job_V-SN_vYRjTt_3VXG` 完成 exit 0，**34 / 34 通过**。结果为：

`Saved/Validation/FoundationM0/73244b9e-d44a-469b-9fbc-1c8e1658e614/BuildEvidenceContractTests/result.json`

这是审查者独立重跑的脚本合同结果，区别于实施代理的另两次 34 项结果。它不证明 UE 构建、加载或 Automation 已运行。

### 补充签发前负向边界

独立 13 项诊断中，除三个目录漏检外，下列完整名称全部按预期拒绝：

| 用例完整名称 | 实际验证边界 |
| --- | --- |
| `Control_CurrentNormalNewCppRejected` | 正常目录新增源码使当前清单与旧证据不一致。 |
| `IssuanceRejectsWrongReceiptTarget` | 签发前拒绝其他目标 receipt。 |
| `IssuanceRejectsWrongReceiptType` | 签发前拒绝非 Editor 类型。 |
| `IssuanceRejectsWrongReceiptConfiguration` | 签发前拒绝 Shipping 配置。 |
| `IssuanceRejectsWrongReceiptPlatform` | 签发前拒绝 Linux 平台。 |
| `IssuanceRejectsWrongReceiptProject` | 相对 receipt 的 Project 指向另一工程时拒绝。 |
| `IssuanceRejectsProductOutsideProjectAndEngine` | 产物不位于项目或引擎根时拒绝。 |
| `IssuanceRejectsDuplicateProjectDll` | receipt 重复列出同一项目 DLL 时拒绝。 |
| `IssuanceRejectsUnmappedProjectDll` | receipt 中有未由 .modules 映射的项目 DLL 时拒绝。 |
| `IssuanceRejectsModuleDllOutsideManifestDirectory` | .modules DLL 映射含父目录时拒绝。 |

这些 receipt 测试在调用证据签发函数前修改夹具收据，直接验证解析/身份/覆盖规则；不是只修改已签发收据然后依靠旧哈希不匹配而被拒绝。

### 已核对且未发现其他确定阻塞的生产行为

- builder 只在完整 Editor 且 OnlyModules 为空时采集资格，实际完整 UBT 进程成功后才签发；原生非零退出码保留。整批构建中任一目标失败，测试入口不能接收该批次作为完整 Editor 凭据。
- RunId、工作区、项目、引擎根、目标、平台和 Development 配置精确绑定；证据目录固定在本次 Build RunId 下；仅一个 Editor 子目标。
- 单文件/模块参数、失败/超时进程、进程 PID/启动时间/宿主/UBT 参数不一致均拒绝；不是“磁盘上存在旧 DLL 就认为已经构建”。
- 构建前后清单内容和逐文件时间戳均检查；当前清单、receipt、DLL、.modules、引擎关键工具身份重新读取。生产前置和测试成功后的复核均存在；结果文件在核验期间变化也拒绝。
- receipt 项目、目标、配置和相对路径解释与实际 UE 5.8 `TargetReceipt.cs` 一致：默认 Development receipt 路径（492–509），Project 相对 receipt 目录（799），产品变量路径写入（443–455、815–825）。
- 覆盖范围是 receipt 列出的全部项目 DynamicLibrary DLL 与项目 .modules；BuildId 必须匹配，每个项目 DLL 必须被模块映射，主工程模块必需，按所选测试组还要求对应模块。引擎产品明确不作为项目 DLL 覆盖。
- runner 缺参数/缺结果为 NotExecuted；已有但不合格的证据为 Failed。UE 已执行后的结束复核异常统一 Failed，不会因结果被删除误记 NotExecuted。

## 明确限制

本次为代码与纯脚本独立复核，不能替代真实 UE 完整构建、DLL 加载或 Data/UI/VFX/PCG Automation 验证。实施代理的原工作区真实 receipt 兼容性探测属于只读解析结果，不能补成成功完整构建资格，本审查不将该探测计作 UE 执行。

证据是未签名的本地记录；信任实际 builder/文件系统执行环境，不试图防御主动改写证据、进程替身或完整构建链被控制。同步快照检查构建前后与测试前后端点，不是对整个源码树的事务锁或持续审计。正常完整增量目标成功可合格，不要求每个 DLL 在该次重新链接。

证据范围明确不包含资产、生成输出、引擎源码、SDK 或任意外部根；外部 AdditionalPluginDirectories/AdditionalRootDirectories 显式拒绝。对于任意同名目录造成的合法 Source 漏检，本次要求在既定项目范围内修正，不能以资产排除限制解释为可接受。

## 修订版关闭验证

**P1 已关闭；最终独立 13 / 13 边界回归与 38 / 38 合同回归均为 exit 0。** 修订版没有其他确定阻塞。此结论只覆盖本次代码和纯脚本审查，不声明真实 UE 构建或 Automation 已完成。

### 最终冻结快照

实现代理先明确冻结 helper v2，随后完成最后的隔离 runner 夹具修正；最终四文件及其原字节保存在 `Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidence/Delivery-v2`。审查者已逐文件比较当前字节与该目录副本，四项全部一致。实施报告最终 revision 为 7555604770030191。

| 文件 | 最终 read_revision | 最终 SHA256 |
| --- | --- | --- |
| `Build/Game/FoundationBuildEvidence.psm1` | 7555604770030168 | `E386D1C78180FD154C0EB4788E0DD4CA560071A083D536F4B61544B3D28747F0` |
| `Build/Game/BuildFoundation.ps1` | 7555604770030116 | `FE982299F233420B783A9E7C89E7C25B49C2734840F54EB1749CAF70CE827F16` |
| `Build/Validation/TestPluginArchitectureUnreal.ps1` | 7555604770030124 | `8DB8CC2142E9E8CCB575520DA7DEAB71F6B6F4C02A0BB52A4E924EC2035E4C9D` |
| `Tests/Foundation/Scripts/TestFoundationBuildEvidence.ps1` | 7555604770030182 | `2EA9BDA6735E8363D81E4A604965181E8603C15B62220D6AB93B45AE905C1D91` |

### 生产修补核对

`FoundationBuildEvidence.psm1:11–12,88–109` 将清单范围升为 `FoundationProjectCompileInputs-v2`；旧范围证据直接被范围身份校验拒绝。待遍历节点携带 `PluginTree` 与 `SourceTree`：项目 Source 一开始标记为完整源码树，真实插件根进入 Source 时也标记；此标记永久向后继承。即使 Source 中存在测试用 `.uplugin` 文件，也不会再次触发目录剪除。

排除仅作用于含 `.uplugin` 的真实插件根的直接子目录，Docs 保留；Config/Build 不按目录名剪除。完整差异只改变输入范围/递归与范围描述，receipt、进程、产物、当前证据校验算法没有变化；builder 与生产 runner 的文件字节也没有变化。修补遵守本次限定范围。

### 原 13 项复现的最终独立 GREEN

命令：

```powershell
& 'E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe' -NoProfile -NonInteractive -File 'Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/FinalDelivery/ReviewBuildEvidence.ps1'
```

实际 job `wc_job_4U4xpz3Wqq85BZLx`，exit 0，时间 2026-10-12T05:05:18.4979298Z。结果：

`Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/FinalDelivery/result.json`

该独立诊断脚本与 Initial 版本逐字相同，复用最终合同测试中的明确合成夹具函数；结果记录最终四文件 SHA256，全部匹配上表。13 项全部符合预期、意外接受数为 0。三个旧漏洞的完整用例名称和关闭结果如下：

| 完整用例名称 | 关闭观测 |
| --- | --- |
| `CurrentGameSourcePrivateContentCppMustReject` | 新增源码在当前清单中出现一次，输入 SHA 改变，当前完整构建门禁拒绝。 |
| `CurrentPluginSourcePrivateDocsCppMustReject` | 插件同名目录内源码出现一次，输入 SHA 改变，当前完整构建门禁拒绝。 |
| `DuringBuildGameSourcePrivateContentCppMustReject` | 新增源码在构建后清单中出现一次，前后 SHA 不同，证据签发拒绝。 |

其余 10 项仍按前文描述拒绝。`Final/result.json` 另保留了 helper v2 最早一次 13 项独立 GREEN；本节 FinalDelivery 记录对应最后交付的完整四文件哈希。

### 最终 38 项合同回归与测试有效性

独立运行与前述原 34 项相同的 PowerShell 命令。实际 job `wc_job_HgqLFDyQHRM7AS8p` 完成 exit 0，时间 2026-10-12T05:06:10.1607520Z；读取结果 JSON 后确认 CaseCount 为 38，所有 Case.Status 均为 Passed：

`Saved/Validation/FoundationM0/60794450-f78c-4cbd-a46a-b9bea1dba41e/BuildEvidenceContractTests/result.json`

新增四项完整名称：

- `NestedSourceCppAddedDuringBuildRejected`：在实际签发入口前改变源码输入，旧目录剪除会漏检，新逻辑拒绝。
- `AllSourceDirectoryNamesIncludeNewAndModifiedFiles`：对主工程和插件 Source 的 2 × 10 种原排除目录名逐项检查新增条目、实际前置拒绝和修改哈希变化；Source 中另放 `.uplugin` 测试数据，验证 SourceTree 不能被重置。
- `PluginRootGeneratedAndAssetDirectoriesRemainExcluded`：真实插件根的九类资产、生成和元数据目录保持排除。
- `RunnerHarnessDetectsRemovedBuildGate`：只删除临时 runner 副本中的前置门禁，要求到达精确失败哨兵，然后由 Suite 目录断言识别越过门禁；异常类型/消息不符或超时都不能冒充通过。

实施代理自查补强了旧 runner 夹具：原先仅检查结果中的 Suites 为空和没有 process.json；如果门禁被删除而无效 exe 在 Process.Start 阶段失败，这两项检查仍可能同时成立。新版 `TestFoundationBuildEvidence.ps1:90–125` 检查未创建 Suite 目录，且只在隔离副本的唯一真实 UE 调用点用 `throw 'FoundationFixtureUEBoundaryReached'` 替换进程调用。生产门禁、其先后顺序和 Suite 创建路径继续来自生产源码。变异额外要求 result.Message 精确为哨兵，再要求捕获的异常精确为目录断言。该哨兵总是失败，不产生 UE 成功、构建成功或 Automation 成功记录。

### 已保留的真实失败和定位证据

修补过程中没有覆盖或删去失败记录：

- 原最终 helper 的 13 项 RED：前文 Initial/result.json，3 个意外接受，实际 exit 1。
- 第一次 v2 合同独立重跑：`Saved/Validation/FoundationM0/b7c8bf92-f352-4354-ae08-86d6a4a8528e/BuildEvidenceContractTests/result.json`，job `wc_job_dyjuT1GE2qExpOVC`，实际 exit 1；37 项通过，末项变异夹具未命中精确断言。
- 单夹具定位：`Saved/Validation/PluginArchitectureOptimization/20261012/Task6BuildEvidenceReview/Mutation/InspectRunnerMutation.ps1` 与同目录 result.json。命令为同一 PS7.6.6 的 `-NoProfile -NonInteractive -File` 指向该脚本；job `wc_job_JSWvus9GO6ZRp-M2` exit 0 仅表示诊断完成。实际捕获“隔离runner前置回归超时；只终止本次PowerShell。”，保留目录已有 Data Suite 和空 stdout/stderr，但没有该 runner 的 process.json/result.json。该结果明确包含 UEExecuted=false、ActualBuildExecuted=false。

此定位与实施代理的快速无效 exe 报错单例共同说明，文本假 exe 的宿主拒绝时序不适合作为夹具断言前提。因此只将临时副本的启动边界改为固定失败哨兵；没有放宽目录断言、接受任意异常、增加全局超时或修改生产逻辑。

最终 38 项结果与最终 13 项结果都明确为 `FixturesOnly=true`、`ActualBuildExecuted=false`、`UEExecuted=false`。本次审查到此结束；真实完整 Editor 构建和随后 UE 自动化仍由主代理以新的 RunId 执行。
