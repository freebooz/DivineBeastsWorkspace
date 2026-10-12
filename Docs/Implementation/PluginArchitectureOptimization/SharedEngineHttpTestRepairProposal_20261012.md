# 共享引擎 HTTP 测试修复审查提案（2026-10-12）

## 结论与本次可批准的范围

准备状态：**提案、精确补丁应用及两阶段恢复/回退的 Saved 镜像演练均已完成；original 注册材料和共享引擎修复未执行，原生重建和测试未执行。**

待授权的固定恢复流程分两步：先把 original 中既有完整补丁材料恢复为登记的 LF 字节，保持期望摘要和自动门禁不变；再仅修订锁定引擎中的一个文件：

`F:\UnrealEngine-5.8.0-release\Engine\Source\Runtime\Online\HTTP\Private\Tests\HttpRedirectPolicyTests.cpp`

该文件存在两类确定的编译错误；现有仓库登记补丁已经包含正确的测试修订，但共享引擎现场仍保留旧测试。本提案的 [三处修订补丁](SharedEngineHttpTestRepairProposal_20261012.patch) 只补齐这三个位置，不重放完整 HTTP 补丁。

| 字节对象 | SHA256 |
|---|---|
| 允许作为修复前置的现场测试 | `26E39B9061D0526B47DA802CB6763EF09434183AC5C06278E107C8310FA1C672` |
| 预期修复后的测试，与现有登记 AfterSHA256 相同 | `2861D419369AA45D62AC9FE8AC9823A9CCF706A7D9ED9B204E51F6C6AF26225B` |
| 本提案独立补丁文件（UTF-8、LF） | `5DB9C9B59279A1CBED1386DD39792E7B273ED9F02553A5DED0E124BCCF5FD469` |

修复前置应针对新鲜读取的完整文件 SHA；如现场已变化，必须重新审查，不能把本次快照当作覆盖其他修改的许可。

## 证据身份与保存位置

- 准备 RunId：`b34b20fc-6554-464e-b585-dbffec8e9123`。
- 准备工作区：`E:\poject\feebooz\DivineBeastsWorkspace`（original）。
- 实际失败构建工作区：`E:\poject\feebooz\DivineBeastsWorkspace\.worktrees\v`。
- 实际 Server 构建 RunId：`4f4fb9ee-acab-4a74-98dd-e4681068a478`。
- 实际 Server Job：`wc_job_2Yuz3OEh7aEmYk5c`。
- 本提案证据目录：`Saved/Validation/PluginArchitectureOptimization/20261012/SharedEngineHttpTestRepair/b34b20fc-6554-464e-b585-dbffec8e9123`。

所有证据均以该 GUID 目录新建；没有覆盖已有验证目录。原生 UBT 日志按实际字节复制到 `Server/UBT.snapshot.log`，五条诊断及其行号位于 `Server/diagnostics.json`，原字节上下文位于 `Server/error-context.log`。解析展示采用 UTF-8 并保留原生日志字节，不改写编译器结果。

### 原生编译失败与 Runner 取消分开记录

| 字段 | 本次事实与来源 |
|---|---|
| NativeCompilationOutcome | **FailedObserved**：真实 `Module.HTTP.cpp` 编译动作退出 2，UBT 明确记录 “The build will fail”。 |
| RunnerOutcome | **Cancelled**：主代理报告 07:48 的实际 `observe_jobs` 为 stopped、exit_code=-1、outcome=cancelled，stderr 为 `job stopped by request`。 |
| BuildResultProduced | **false**：主代理在 07:48:53.9949257Z 检查为不存在；本次捕获也确认原构建的 `Build/result.json` 不存在。 |
| 进程终态来源 | 主代理实际工具观察：PID 35428、36696 及直接子进程已不在。本准备任务没有自行停止进程。 |
| UBT 最终退出码 | **未取得**。Runner 的 -1 不能改写为 UBT 最终退出码；没有补写原构建的 result.json。 |

完整来源单独保存在 `Server/coordinator-stop-observation.json`。该文件明确归属主代理观察，不冒充本准备脚本重新执行的进程检查。同一来源记录另一个 Surface 任务曾在 v 使用 Editor PID 36308；本准备任务未操作该进程，后续独占情况必须重新确认。

## 已证实的两类错误

现场测试位于 `#if WITH_DEV_AUTOMATION_TESTS && WITH_CURL` 条件内。

| 测试源行 | 真实诊断 | 确定原因 |
|---:|---|---|
| 18 | C2248 | 派生夹具的 `CleanupForTest()` 直接调用 Curl 的 private `CleanupRequest()`。 |
| 21 | C2838、C2065 | 使用不存在的 `EAutomationTestFlags::ApplicationContextMask`。 |
| 154 | C2838、C2065 | 第二处测试注册重复使用同一个不存在的枚举成员。 |

总计五条原生诊断，不能只记录日志最后一条 C2065。

实际消费者声明已保存在 `EngineBefore/`：

- `AutomationTest.h:89–149` 的枚举没有 ApplicationContextMask 成员；第 144 行定义全局常量 `EAutomationTestFlags_ApplicationContextMask`。
- `AutomationTest.h:4121–4148` 的测试宏使用该全局常量并展开测试标志表达式。这不是只在 Server 下缺失的声明。
- `CurlHttp.h:121–129,268` 表明 SetupRequest 是 protected，CleanupRequest 覆盖是 private。
- `HttpRequestCommon.h:74,103` 的基类 CleanupRequest 是 protected 虚函数。

### 三处修订的含义

1. 在测试派生类中加入 `using FHttpRequestCommon::CleanupRequest;`，使夹具通过基类的受保护虚函数入口进行调用，仍然虚分派到 Curl 的覆盖。没有把 Curl 私有实现移到公共接口，也没有以基类限定调用绕过真实 Curl 清理。
2. 第一处测试注册改用实际存在的 `EAutomationTestFlags_ApplicationContextMask`。
3. 第二处测试注册作相同修订。

这三处与仓库现有登记补丁逐字对应；候选文件不是另造的一版修复。两个测试的身份、现有断言、HTTP 行为及测试编译条件保留。

## 既有补丁与现场同步状态

依据 `Build/Engine/Patches/UE5.8-HttpRedirectPolicy.md:5–7`，这是既有的请求级重定向定制补丁，现场引擎不是 Git 仓库。该说明不保证现场基线等于官方未修改源码。更早的 `Docs/Implementation/GamePlatformDesignRemediation/ExecutionProgress.md:92–94` 已记录相同两类测试编译问题。

本次实际 SHA 比较结果：

| 共享引擎文件 | 实际 SHA256，均匹配现有登记 After |
|---|---|
| `Public/Interfaces/IHttpRequest.h` | `45A63D22608E23C83B32EA2E7CD6AAD3429E906643A038BE83E49614ED9B07ED` |
| `Private/Curl/CurlHttp.h` | `BD5836507C72B4A014E5E1CB3965F7C7D31EF98DA41588E6AC88239F8C25A019` |
| `Private/Curl/CurlHttp.cpp` | `AE06C3EF230E354DA37CCF58584BF18BC600860E3478A5BC93EAF9B21BC4AECC` |

上述三份生产文件不在本修复补丁范围。仅测试文件未匹配登记 After；其实际旧 SHA 与候选新 SHA 已列于本文首表。完整文件快照、登记清单及比较记录见 `EngineBefore/`、`RegisteredPatch/`、`EngineFileHashes.json`。

### 现有自动应用门禁：原始预测与 Saved 镜像实测

本次没有对 original 注册材料或共享引擎调用应用脚本。最初根据实际脚本和字节作出下表预测；随后只在 Saved 镜像中运行**字节完全相同的 applier 副本**的 CheckOnly，实际确认了两道拒绝。

| 项目 | 实际观察或预计拒绝 |
|---|---|
| 当前仓库完整 .patch 原字节 SHA | `A49EAD79D734E793714F47649B194F4B79F2F53B5779B6DDE5C07EF7F7A8F861` |
| 清单登记的完整 .patch SHA | `8F78CDD1FB4E8E02720F90519F7154C8C546CB3C00BC345B53E2B34E885C7F35` |
| 保持不变的 hashes.json 原字节 SHA | `73904A8F58011D2134EF5FA0FA8BD42D14BC2AA7BBD65E69ABAD5EF357C5C751` |
| 保持不变的 applier 原字节 SHA | `4E30495178E5F96AC68C8B6AF3ED8BDF31CB57B52797FAB52042F3B3A9170F86` |
| 行尾比较 | 原文件有 347 个 CRLF；内存和 Saved 镜像 LF 规范化后的摘要均等于登记 SHA，未改写 original 材料。 |
| 脚本第 20 行 | 使用 Get-FileHash 检查原字节，当前材料预计先被补丁 SHA 门禁拒绝。 |
| 假设补丁材料字节正确后 | 第 45–51 行仍会拒绝旧测试 SHA：它既非 Before（登记为 null）也非 After；其他三文件已是 After。不能自动猜测或重放全补丁。 |

初始预测保存在 `ExistingGatePrediction.json`，其中 `ExistingApplyScriptExecuted=false`、`PredictionOnly=true` 记录的是初始捕获阶段，保持历史原样。后续实际镜像证据另存 `GateRehearsal/result.json`：原始 CRLF 材料返回 exit 1（补丁 SHA256 不匹配）；仅恢复 LF 材料、仍保留旧测试时返回 exit 1（目标有未知修改或缺失）。

**只修正三处引擎测试仍不能让原自动应用器通过：CRLF 材料会先触发原始摘要门禁。** 必须分开完成“材料恢复到原登记 SHA”和“已知旧测试同步到原登记 After SHA”。本准备过程没有改变 original 注册补丁、hash 清单、自动脚本或其拒绝逻辑。

## Server 与 Client 的范围判断

实际 v Server 和 original Client 的生成输入均显示：

- `Development/Core/SharedDefinitions.Core.Cpp20.h:12`：WITH_DEV_AUTOMATION_TESTS=1。
- `Development/HTTP/Definitions.HTTP.h:36`：WITH_CURL=1。
- `Development/HTTP/Module.HTTP.cpp:23`：包含同一份 HttpRedirectPolicyTests.cpp。
- 两者为 Development，WITH_EDITOR=0；错误不依赖 Server 特有符号。

因此，正常 Client 重编 HTTP 时也会遇到相同源码错误。**这里是对实际 Client 生成输入的编译推断，不是新 Client 构建结果。** 本准备任务没有启动新的 Client，也没有给它分配构建 RunId。v 尚未生成对应正常 Client HTTP 目录；不能把 original 输入快照改称 v 的 Client 实跑。

相关输入按工作区分别保存在 `ServerInputs/`、`ClientInputs/`，解释位于 `ClientInference.json`。

## 本次实际完成的隔离验证

验证目录为证据根下 `OfflinePatchFixture/`，只放入现场测试的字节副本及预期的 Engine/Source 相对目录。没有向共享引擎应用补丁。

为防止 Git 向上识别 original 工作区，三个命令使用临时子进程环境 `GIT_CEILING_DIRECTORIES=<本证据根>`，清除继承的 GIT_DIR/GIT_WORK_TREE/GIT_INDEX_FILE 覆盖；不修改宿主环境或 Git 配置。

| 实际命令，cwd 均为 OfflinePatchFixture | 退出码 | 判断 |
|---|---:|---|
| `git rev-parse --show-toplevel` | 128 | 预期结果，确认该隔离目录未被识别为原仓库工作树。 |
| `git -c core.autocrlf=false apply --check -- <本提案 .patch 绝对路径>` | 0 | 当前现场测试副本可接受该精确补丁。 |
| `git -c core.autocrlf=false apply -- <本提案 .patch 绝对路径>` | 0 | 仅副本完成应用；结果字节等于登记的新 SHA。 |

原副本 SHA 为 26E39B90…，副本应用后为 2861D419…，与本文完整摘要一致。补丁逻辑差异检查确认只有三处测试修改。

完整 argv、cwd、环境差异、时间、stdout/stderr 和退出码位于 `OfflineValidation/Isolation/`、`OfflineValidation/Check/`、`OfflineValidation/ApplyFixture/`。结论见 `OfflineValidation/result.json`。实际捕获与验证脚本分别保存在 `capture-evidence.py`、`validate-offline-patch.py`。

所有捕获输入在读取前后、离线补丁验证和两阶段镜像演练结束时重复检查，摘要一致；证据见 `CapturedInputs.json`、`InputsUnchanged.json`、`InputsUnchangedAfterOfflineValidation.json`、`InputsUnchangedAfterGateRehearsal.json`、`InputsUnchangedAfterFormalGateRehearsal.json`。这证明所捕获的共享引擎与注册材料在本准备过程保持原字节，不宣称检测过所有无关工作区文件。

**该验证仅证明补丁能在准确旧字节上得到准确候选字节。没有执行 C++ 编译、UE Automation、回环 HTTP、Client、Server 或 Cook；不能称为原生 RED/GREEN。**

## 已完成的受保护恢复与回退演练

先在 `GateRehearsal/` 完成准备入口演练，再用正式版本化脚本在新建的 `FormalGateRehearsal/Workspace/` 和 `FormalGateRehearsal/Engine/` 中复用同一五步演练。使用原字节 applier/hash 清单及四份引擎 HTTP 文件镜像；引擎版本文件也取自实际锁定引擎。所有执行目标均位于本次 Saved 证据目录，旧脚本与旧演练保留不覆盖。

| 实际步骤 | 退出码 | 真实结果 |
|---|---:|---|
| RawMaterialGate | 1 | 原始 CRLF 完整补丁被现有原始摘要门禁拒绝。 |
| CanonicalMaterialOldTestGate | 1 | LF 材料已匹配登记摘要，旧测试仍被未知文件状态门禁拒绝。 |
| RecoveryPlanOnly | 0 | 完成前置检查并返回计划，没有写入目标文件。 |
| RecoveryExecuteMirror | 0 | 两阶段事务完成；测试为 2861D419…，完整补丁为 8F78CDD1…；未改动的 applier CheckOnly 返回 AlreadyApplied。 |
| RecoveryRollbackMirror | 0 | 恢复当次备份，测试回到 26E39B90…，完整补丁回到 A49EAD79…，两者逐字节等于初始镜像。 |

表中的两个 exit 1 是实际执行得到的**预期拒绝**，不是测试脚本跳过的结果。正式入口镜像演练总体为 Passed；真实 argv、cwd、时间、stdout/stderr、退出码及前后摘要见 `FormalGateRehearsal/result.json` 和各步骤同名子目录。对应演练脚本为 `rehearse-formal-guarded-recovery.py`；初版记录仍在 `GateRehearsal/result.json` 和 `rehearse-guarded-recovery.py`。最终镜像保持已回退状态，成功前向记录另行保留。

## 授权后可直接执行的恢复步骤

已准备并在上述镜像中实际执行的脚本：

`Docs/Implementation/PluginArchitectureOptimization/SharedEngineHttpTestRepair_20261012.ps1`

脚本 SHA256：`81766F8919F4370B63981748321C4E674AF9455E5A48DEC00759F110A185418B`。

该脚本默认 PlanOnly，只有明确传入 `-Execute` 才会写入。`WorkspaceRoot`、`EngineRoot` 和 `EvidenceRoot` 必填；`-RestoreRun` 用于回退本脚本实际生成的执行目录。脚本不启动构建、UE 或 Cook。以下写入与回退操作尚未针对 original 和共享引擎执行。主代理已另外对真实目标运行默认 PlanOnly，返回 `TargetsWritten=false`、`ExistingGateChanged=false` 和预期摘要；该只读观察保存在 `Saved/Validation/PluginArchitectureOptimization/20261012/FinalDeliveryChecks/77fd4f80-066c-48a0-b28c-4998663bf74b/SharedEnginePlanOnly/tool-observation.json`。待授权后的固定执行方案如下：

```powershell
$repairWorkspace = 'E:\poject\feebooz\DivineBeastsWorkspace'
$repairPwsh = Join-Path $repairWorkspace 'Saved/Tools/PowerShell-7.6.6/pwsh.exe'
$repairScript = Join-Path $repairWorkspace 'Docs/Implementation/PluginArchitectureOptimization/SharedEngineHttpTestRepair_20261012.ps1'
$repairEvidence = Join-Path $repairWorkspace 'Saved/Validation/PluginArchitectureOptimization/20261012/SharedEngineHttpTestRepairExecution'
if ((Get-FileHash -LiteralPath $repairScript -Algorithm SHA256).Hash -cne '81766F8919F4370B63981748321C4E674AF9455E5A48DEC00759F110A185418B') {
    throw 'Reviewed recovery script SHA changed.'
}
$repairArguments = @(
    '-WorkspaceRoot', $repairWorkspace,
    '-EngineRoot', 'F:\UnrealEngine-5.8.0-release',
    '-EvidenceRoot', $repairEvidence
)
& $repairPwsh -NoProfile -NonInteractive -File $repairScript @repairArguments
if ($LASTEXITCODE -ne 0) { throw 'Recovery preflight failed.' }
& $repairPwsh -NoProfile -NonInteractive -File $repairScript @repairArguments -Execute
if ($LASTEXITCODE -ne 0) { throw 'Recovery did not succeed; inspect its execution evidence before proceeding.' }
```

成功输出必须为 `AppliedAndGateVerified`，并返回实际 `RunDirectory`。该新 GUID 目录保存 `before.json`、完整的 `test.before.cpp` 和 `patch.before.patch`、原 applier 的 CheckOnly 输出和结果。若后续决定撤回，只对这次实际返回的目录执行以下命令；路径来自真实执行输出，不得拿本次 Saved 镜像目录代替共享引擎执行记录：

```powershell
$repairRestoreRun = '<上一步真实输出的 RunDirectory>'
& $repairPwsh -NoProfile -NonInteractive -File $repairScript @repairArguments -RestoreRun $repairRestoreRun -Execute
if ($LASTEXITCODE -ne 0) { throw 'Guarded rollback requires manual review; do not overwrite unknown concurrent bytes.' }
```

### 固定前置、写入与回退规则

- 引擎必须仍为 UE 5.8.0；applier、hash 清单和三份生产文件必须保持本文审查的固定 SHA。路径检查拒绝被检查路径中的重解析点；执行证据必须位于本工作区的 `Saved/Validation` 下。
- 完整补丁材料只接受已捕获的 A49EAD79…原字节或已登记的 8F78CDD1…字节；LF 转换后必须精确等于**原登记摘要**。不修改 hash 清单中的期望值。
- 引擎测试只接受 26E39B90…已知旧版本或 2861D419…已登记版本；正式脚本从已核准旧字节执行精确三处变换并强制验证完整新摘要，不依赖 Saved 候选文件。三处提案补丁也有固定完整摘要检查。未知状态一律拒绝。
- 先备份两个实际可写对象，再恢复材料 LF，随后同步测试。每个写入都在独占文件流内再次读取当前 SHA，通过后才写；写后刷新并验证准确的新 SHA。不会重放三份生产源码补丁。
- 两阶段完成后重新检查不可变输入，调用现有未改动 applier 的 CheckOnly；必须返回 AlreadyApplied，且两份写入文件最终摘要正确。
- 写入期间异常会尝试恢复自己的完整备份；恢复也要求当前仍为本次批准的前后状态。若发现未知并发字节，保留它们并记录 ManualReviewRequired，不覆盖其他任务修改。显式回退还核对执行记录的工作区、引擎身份和备份摘要。本轮真实镜像演练已验证成功前向及精确回退；没有模拟所有硬件或进程中断情形。

## 授权后需要的重建与验收

以下是后续执行要求，本次均未执行：

1. 共享引擎维护方确认独占窗口与当前执行进程，重新读取测试旧 SHA、三份生产文件 After SHA 及引擎版本。只有获得此项共享引擎恢复授权且前置仍满足时，才执行上一节的固定恢复命令；保存本次真实执行目录。
2. 先恢复原登记 LF 材料，再仅同步三处测试修订，要求新测试为 2861D419…、三份生产文件保持登记 After，原 applier CheckOnly 实际返回 AlreadyApplied。若前置已变，停止并重新审查。不得关闭开发测试、改 Target 或绕过现有自动脚本的未知状态/摘要门禁。
3. 以新 RunId 进行真实 HTTP 编译并完成所需正常完整 Server、Client 目标，保留真实退出码、收据及产品。原取消的 RunId 不能补写成成功，也不能用 OnlyModules、旧引擎 Server.exe 或历史对象数量替代完整目标。
4. 需要运行上述 HTTP 自动化时，应先使相应 Editor/HTTP 二进制及调用者与实际接口一致。既有补丁增加了虚函数，原说明第 36 行已指出 ABI 与重建要求；此前项目 Editor 证据不能单独证明这份引擎 HTTP 补丁已原生生效。
5. 运行两个完整身份 `System.HTTP.RedirectPolicy.Contract` 与 `System.HTTP.RedirectPolicy.LoopbackNetwork`。后者使用既有双端口回环夹具和明确 Origin，按原补丁说明分别检查 legacy/deferred 启动配置；保存实际测试结果和两个目标未收到重定向请求的证据。补丁应用或编译成功不能替代真实网络负例。
6. Server 完整构建与对应 Editor 前置成立后，才能继续正常 Cook/Stage 及资源容器验收；本提案没有产生 Cook 证据。

## 为什么需要独立的共享引擎授权

本轮已授权并执行项目内的插件架构优化。这里提出的恢复还会修改所有工作区共用的 F: 引擎根，现场存在其他任务使用该引擎的记录，影响范围超出单个项目检出。仓库 `AGENTS.md` 要求高影响操作具备明确具体授权和回退；现有完整补丁脚本也会拒绝当前未同步状态。因此，准备阶段已完成诊断、精确修复材料、镜像演练和真实目标只读预检，将共享引擎写入留待这组具体恢复范围确认。

因此，需要一次性批准的具体范围是：**恢复现有完整补丁为原登记 LF 字节；在固定前后 SHA 和可回退事务保护下同步这一份引擎测试；随后协调并执行相关正常完整目标、两个 HTTP 回归及 Cook/Stage 验收。** 上文已给出可执行恢复流程，无需再选择覆盖机制或期望摘要。本提案不构成已经取得该授权，不自动执行应用，不修改现有自动门禁。中央计划、台账、Manifest 和目录登记由主代理另行整合。
