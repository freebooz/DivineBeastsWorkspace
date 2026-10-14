#requires -Version 7.0
<# SSLM演示运行器隔离回归：纯内存合成结果，只测试门禁，不写伪Automation/UE资产，不启动UE。
   对进程失败/超时、缺必需项、零测试、旧输入和失败结果明确拒绝；通过不代表任何原生测试通过。 #>
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$policy=Get-PluginDemoSuitePolicy Demo
$valid=@($policy.Tests | ForEach-Object { [pscustomobject]@{fullTestPath=$_;state='Success'} })
$before=[pscustomobject]@{BuildResultSHA256=('a'*64);InputSHA256=('b'*64);ArtifactSHA256=('c'*64)}
$result=[pscustomobject]@{ExitCode=0;TimedOut=$false;Cleaned=$true;ProcessId=123}
$passed=0
function Reject([object[]]$Cases,$Process=$result,$After=$before) {
    $caught=$false
    try { $null=Assert-PluginDemoRunResult -ProcessResult $Process -Cases $Cases -Before $before -After $After }
    catch { $caught=$true }
    if (-not $caught) { throw '不合格合成门禁被接受。' }
}
$count=Assert-PluginDemoRunResult -ProcessResult $result -Cases $valid -Before $before -After $before
if ($count -ne 8) { throw '必需测试数量不匹配。' }; $passed++
Reject @(); $passed++
Reject @($valid | Select-Object -Skip 1); $passed++
$bad=@($valid | ForEach-Object { [pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state} }); $bad[0].state='Fail'; Reject $bad; $passed++
Reject ($valid+$valid[0]); $passed++
Reject $valid ([pscustomobject]@{ExitCode=6;TimedOut=$false;Cleaned=$true;ProcessId=123}); $passed++
Reject $valid ([pscustomobject]@{ExitCode=0;TimedOut=$true;Cleaned=$true;ProcessId=123}); $passed++
Reject $valid ([pscustomobject]@{ExitCode=0;TimedOut=$false;Cleaned=$false;ProcessId=123}); $passed++
foreach ($field in @('InputSHA256','ArtifactSHA256','BuildResultSHA256')) {
    $after=[pscustomobject]@{BuildResultSHA256=$before.BuildResultSHA256;InputSHA256=$before.InputSHA256;ArtifactSHA256=$before.ArtifactSHA256}; $after.$field='d'*64
    Reject $valid $result $after; $passed++
}
# 源码结构断言是附加证据，不冒充进程回归；完整FBE拒绝旧DLL由既有TestFoundationBuildEvidence负责。
$source=Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../../Build/Game/TestPluginDemo.ps1') -Raw
if ($source.IndexOf('$before=Assert-FoundationEditorBuildResult') -ge $source.IndexOf('$result=Invoke-FoundationProcess') -or
    $source -match 'BuildFoundation|UnrealBuildTool|live_compile|trigger_build') { throw '启动门禁顺序/无构建合同被破坏。' }; $passed++
# 无人值守原生测试不承担Monolith资产制作，必须独立SessionId并排除其单端口/单SQLite编辑器服务。
# 此处只检查明确CLI合同，不把源码扫描当作真实进程隔离通过；真实结果仍由本次process/index证明。
foreach($runnerPath in @('Build/Game/TestPluginDemo.ps1','Build/Validation/TestPluginArchitectureUnreal.ps1')) {
    $runnerSource=Get-Content -LiteralPath (Join-Path $PSScriptRoot ('../../../'+$runnerPath)) -Raw
    if(!$runnerSource.Contains('-SessionId=$($context.RunId)') -or !$runnerSource.Contains("'-DisablePlugins=Monolith'")) {
        throw "无人值守测试缺少独立会话/Monolith工具服务隔离：$runnerPath"
    }; $passed++
}
Write-Output "PluginDemoRunnerContractCases=$passed Failed=0 UEExecuted=False BuildExecuted=False"
