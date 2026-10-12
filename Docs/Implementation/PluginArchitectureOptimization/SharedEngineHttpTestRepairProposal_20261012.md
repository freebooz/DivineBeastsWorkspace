# 共享引擎 HTTP 测试修复审查提案（2026-10-12）

## 结论与本次可批准的范围

准备状态：**提案及隔离副本验证完成；共享引擎修复未执行，原生重建和测试未执行。**

建议仅修订锁定引擎中的一个文件：

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

### 现有自动应用门禁的预计结果——未执行

本次没有运行 `Apply-UE58HttpRedirectPolicy.ps1`，包括其 CheckOnly 模式。以下是根据实际脚本和字节作出的预测：

| 项目 | 实际观察或预计拒绝 |
|---|---|
| 当前仓库完整 .patch 原字节 SHA | `A49EAD79D734E793714F47649B194F4B79F2F53B5779B6DDE5C07EF7F7A8F861` |
| 清单登记的完整 .patch SHA | `8F78CDD1FB4E8E02720F90519F7154C8C546CB3C00BC345B53E2B34E885C7F35` |
| 行尾比较 | 原文件有 347 个 CRLF；仅作内存 LF 规范化后的摘要等于登记 SHA，未改写原文件。 |
| 脚本第 20 行 | 使用 Get-FileHash 检查原字节，当前材料预计先被补丁 SHA 门禁拒绝。 |
| 假设补丁材料字节正确后 | 第 45–51 行仍会拒绝旧测试 SHA：它既非 Before（登记为 null）也非 After；其他三文件已是 After。不能自动猜测或重放全补丁。 |

该预测保存于 `ExistingGatePrediction.json`，明确 `ExistingApplyScriptExecuted=false`、`PredictionOnly=true`。本提案没有改变注册补丁、hash 清单、自动脚本或其拒绝逻辑。

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

所有捕获输入在读取前后及离线验证结束时重复检查，摘要一致；证据见 `CapturedInputs.json`、`InputsUnchanged.json`、`InputsUnchangedAfterOfflineValidation.json`。这证明本准备过程未改动共享引擎或注册补丁材料，不宣称检测过所有无关工作区文件。

**该验证仅证明补丁能在准确旧字节上得到准确候选字节。没有执行 C++ 编译、UE Automation、回环 HTTP、Client、Server 或 Cook；不能称为原生 RED/GREEN。**

## 授权后需要的重建与验收

以下是后续执行要求，本次均未执行：

1. 共享引擎维护方确认独占窗口与当前执行进程，重新读取测试旧 SHA、三份生产文件 After SHA 及引擎版本。仅当本提案前置仍满足时，授权具有共享引擎写权限的执行方进行这个单文件修复事务；保存当次完整备份和操作记录。
2. 只应用本提案的三处测试修订，验证新测试 SHA 为 2861D419…且三份生产文件保持登记 After。若前置已变，停止并重新审查。不得关闭开发测试、改 Target 或绕过现有自动脚本的未知状态/摘要门禁。
3. 以新 RunId 进行真实 HTTP 编译并完成所需正常完整 Server、Client 目标，保留真实退出码、收据及产品。原取消的 RunId 不能补写成成功，也不能用 OnlyModules、旧引擎 Server.exe 或历史对象数量替代完整目标。
4. 需要运行上述 HTTP 自动化时，应先使相应 Editor/HTTP 二进制及调用者与实际接口一致。既有补丁增加了虚函数，原说明第 36 行已指出 ABI 与重建要求；此前项目 Editor 证据不能单独证明这份引擎 HTTP 补丁已原生生效。
5. 运行两个完整身份 `System.HTTP.RedirectPolicy.Contract` 与 `System.HTTP.RedirectPolicy.LoopbackNetwork`。后者使用既有双端口回环夹具和明确 Origin，按原补丁说明分别检查 legacy/deferred 启动配置；保存实际测试结果和两个目标未收到重定向请求的证据。补丁应用或编译成功不能替代真实网络负例。
6. Server 完整构建与对应 Editor 前置成立后，才能继续正常 Cook/Stage 及资源容器验收；本提案没有产生 Cook 证据。

## 为什么需要独立的共享引擎授权

当前任务明确授权的是 original 内的诊断证据与审查材料，并明确禁止修改共享引擎。目标文件位于所有工作区共用的 F: 引擎根，现场还有其他任务使用该引擎的记录；它超出单个项目检出的源码范围。现有完整补丁脚本也会对当前现场状态拒绝自动覆盖。

因此，需要批准的是**具有明确旧/新 SHA 的单文件、三处测试修复及其协调后的原生重建/验收**。本提案不构成已经取得该授权，不自动执行应用，不修改现有自动门禁。中央计划、台账、Manifest 和目录登记由主代理另行整合。
