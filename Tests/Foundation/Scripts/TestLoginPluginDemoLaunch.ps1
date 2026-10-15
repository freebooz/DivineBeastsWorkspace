#requires -Version 7.0
<# SSLM登录开发入口的无UE回归：测试宿主资格与脚本接线，不把静态检查当作点击或资产验收。 #>
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$passed=0
$runner=Get-Content -LiteralPath (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -Raw
foreach($required in @('RunPluginDemo.ps1','-Start','-EngineRoot','-BuildResultPath $BuildResultPath','WaitForExit','status.txt','Failed')) {
    if(!$runner.Contains($required)){throw "启动器缺少合同：$required"}; $passed++
}
if($runner -match 'BuildFoundation|UnrealBuildTool|FCommandLine::Append'){throw '按钮不得自动构建或修改父进程模式'}
# 真正执行隔离脚本的失败路径：不存在引擎不得启动UE，须留下可读失败结果并传播非零。
$runId=[guid]::NewGuid()
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -RunId $runId -EngineRoot (Join-Path $root 'Saved/DoesNotExist-LoginLauncherTest')
if($LASTEXITCODE -eq 0){throw '缺失引擎错误被吞掉'}; $passed++
$status=Join-Path $root "Saved/Validation/PluginDemos/LoginLaunch/$runId/status.txt"
if((Get-Content $status -Raw).Trim() -ne 'Failed'){throw '缺失失败状态'}; $passed++
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -RunId $runId -EngineRoot (Join-Path $root 'Saved/DoesNotExist-LoginLauncherTest')
if($LASTEXITCODE -eq 0){throw '重复RunId未拒绝'}; $passed++
Write-Output "LoginPluginDemoContractCases=$passed Failed=0 UEExecuted=False"
