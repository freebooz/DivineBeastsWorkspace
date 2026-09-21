# Foundation M0 源码与脚本交付（UE目标、资产及运行未验证）

日期：2026-09-21。当前**不是M0可运行交付，也不是完整《神兽联盟》游戏**。唯一工作空间`E:/poject/feebooz/DivineBeastsWorkspace`；唯一工程`Game/DivineBeastsArena.uproject`。

## 修改范围与能力

- 薄主模块：GameInstance仅协调生命周期；开发协调器通过工厂注入项目节点；HUD显示当前实例/地图/运行/节点/租约/错误；模块启动不运行主流程。专用服务器不创建玩家HUD/主流程。
- Core：稳定逻辑身份、严格解析与规范化、回环/哈希、默认未执行的五态结果、独立数值版本。Data与主工程真实调用，不存在虚构ToName。
- Data：反射定义及稳定主资产映射、按类可读结构版本、进程需求合并、实例/世界租约、真实AssetManager加载、必需依赖遍历、源资产查重及编辑器验证。已知审查项和执行状态见验证记录。
- Flow：保留旧Configure/Start/Cancel/Execute/Finish接口，增量资产/工厂/执行令牌；同一执行器处理事件、取消、超时及有界即时循环，不创建第二套主执行器。
- 项目流程资产的生成默认路径：Boot→ValidateConfiguration→LoadProbeDefinition→EnterSandbox→Ready。节点从资产图装配，地图操作留主工程；探针租约由协调器保持到取消/重试/关闭，HUD不借用已释放定义指针。
- Tools/Build/Tests/Docs：分阶段引擎资产脚本、三目标构建、开发Cook/Stage、受控启动、验收矩阵、真实UE测试入口与离线回归。文件逐项归属见`../Architecture/解决方案总体目录规划说明_V1.3.0.md`及插件细化说明。

没有修改Backend/Shared/Deploy，没有新增正式登录、在线传输、竞技、支付或VFX功能，没有迁移旧公开身份、提交/推送或建立新宿主。

## 当前阻断与续接顺序

用户明确要求原位保留：`MobaCommon/GamePlatformArena/GamePlatformArena.uplugin`、`MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin`、`DivineBeasts/Presentation/DivineBeastsPresentation.uplugin`。三者在`Game/Plugins/`下为空白，UE即使未启用也会扫描并失败。不得执行下列UE命令后期待其绕过这些文件，不得为通过验收暗中移动/禁用。

下一断点先复核本批源码修复，再由用户明确决定历史空描述的正式修复；之后补齐真实UE开发库并依次进行Editor/UHT→三目标→四资产→真实测试→Cook/Stage→运行→图形/多PIE。无需重新创建已经写入的插件。当前没有后台自动续跑任务。

## 现有命令与执行边界

以下均从真实工作空间根执行。脚本已经存在；参数按本机UE5.8源码核对。**仅离线/原生及脚本检查有实际通过记录，下面UE阶段尚未执行成功**，必须以退出码及新证据目录判断，不继续串行掩盖前一步失败。

### 可在当前阻断下运行的检查

```powershell
Set-Location 'E:/poject/feebooz/DivineBeastsWorkspace'
# 仅当前PowerShell进程指定已核实引擎；脚本不硬编码个人盘符。
$env:UE_ROOT = 'F:/UnrealEngine-5.8.0-release'
pwsh -NoProfile -File Build/Validation/VerifyFoundation.ps1 -NativeTests
python -X utf8 -B -m unittest discover -s Tests/Foundation/Assets -p 'test_*.py' -v
pwsh -NoProfile -File Tests/Foundation/Scripts/TestFoundationScripts.ps1
pwsh -NoProfile -File Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1
```

第一条验证矩阵在原位空描述未修复时预期返回1，虽然其中原生测试可以通过；不能将总结果改为0。最后一条使用真实UBT配置解析器，不触发UBT目标构建。

### 正式目标构建（阻断解除后）

```powershell
pwsh -NoProfile -File Build/Game/BuildFoundation.ps1 -Editor -Client -Server -MaxParallelActions 2
if ($LASTEXITCODE -ne 0) { throw "正式目标未通过：$LASTEXITCODE" }
```

使用Development/Win64，Editor首先执行；路径与引擎版本、退出码、超时均由脚本核对。没有普通Game目标替代Client目标。

### 真实资产生成（Editor反射编译成功后）

```powershell
$project = (Resolve-Path Game/DivineBeastsArena.uproject).Path
$assetScript = (Resolve-Path Tools/AssetTools/CreateFoundationAssets.py).Path
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($phase in @('Maps','Probe','Flow')) {
    & $editor $project -unattended -nop4 -nosplash -ScriptErrorsAreFatal `
        '-EnablePlugins=PythonScriptPlugin' '-CustomConfig=FoundationStandalone' `
        "-ExecutePythonScript=$assetScript --phase $phase"
    if ($LASTEXITCODE -ne 0) { throw "资产阶段$phase失败：$LASTEXITCODE" }
}
```

PythonScriptPlugin是本次编辑器命令显式启用的引擎工具，不是新增产品启动插件。不加`-run=PythonScript`，该方式需要完整编辑器初始化，不能和Commandlet混用。文件路径作为一个命令参数传递；本地Python执行器源码支持带空格脚本路径。

生成目标固定为`L_FoundationBootstrap.umap`、`L_FoundationSandbox.umap`、`DA_FoundationProbe.uasset`、`DA_FoundationFlow.uasset`，仅在本批Development/Foundation目录。第二次运行只校验，不覆盖人工编辑；已有探针整数/合法流程图变化报告差异。资产本轮均未生成。真实报告需保存`FOUNDATION_ASSETS_REPORT`及本次UE日志；脚本离线测试不代替生成验收。

### 真实数据与三插件自动化（四资产生成后）

```powershell
pwsh -NoProfile -File Build/Validation/TestFoundationUnreal.ps1 -Execute
if ($LASTEXITCODE -ne 0) { throw "UE自动化未通过：$LASTEXITCODE" }
& $editor $project '-run=DataValidation' -unattended -nop4 -nosplash -NullRHI
if ($LASTEXITCODE -ne 0) { throw "引擎数据验证未通过：$LASTEXITCODE" }
```

测试脚本显式传实际规划身份`GamePlatformDefinition:foundation.probe@1`和`GamePlatformDefinition:foundation.flow@1`，缺资产须失败。命令行数据验证会验证项目可发现资产；非本批资产问题应单独归因，不删除正式资产制造负例。

### 开发Cook、暂存与启动（构建/资产/测试均通过后）

```powershell
pwsh -NoProfile -File Build/Game/CookFoundation.ps1 -Cook -Target Client
if ($LASTEXITCODE -ne 0) { throw "客户端Cook/Stage失败：$LASTEXITCODE" }
pwsh -NoProfile -File Build/Game/CookFoundation.ps1 -Cook -Target Server
if ($LASTEXITCODE -ne 0) { throw "服务器Cook/Stage失败：$LASTEXITCODE" }
# 从各自新result.json取得实际StageDirectory，不能引用旧成功产物。
pwsh -NoProfile -File Build/Game/RunFoundation.ps1 -Start -FoundationStandalone -Target Editor -Phase FullM0
```

运行已暂存程序时使用`-Target Client -StageDirectory <本次客户端StageDirectory>`或`-Target Server -StageDirectory <本次服务器StageDirectory>`。这些占位值必须取实际Cook报告，当前无真实Stage可填写。Server只绑定本机回环。默认FullM0要求本次RunId对应`FoundationReady`；HostOnly仅证明薄宿主，不能作为完整M0。

基础编辑器/客户端启动显式带FoundationStandalone和独占RunId；未启用、发行版、Commandlet不会自动跑玩家开发流程。不设置正式默认地图，不静默回退到测试场景。开发诊断为只读；显式控制台`FoundationCancel`取消，`FoundationRetry`创建新运行并重新申请资产，实际恢复行为待UE验证。

## 剩余风险

UE目标/UHT、真实资源持有、Cook/Stage、三维和多PIE都未验收；即使原生测试通过仍可能存在反射/API/配置集成问题。Data审查修复和测试覆盖明细以`FoundationM0Verification.md`为准。已释放租约签发历史暂保留至GI结束，常驻实例长期内存增长需后续量测与不破坏幂等的保留策略。生产密钥、联网准入、三服务器角色业务不在本批范围。
