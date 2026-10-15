#requires -Version 7.0
<# 中文审核入口：运行真实UE原生施法测试，使用已完成的本工程Editor构建。
不登录、不改资产/配置、不停止人工客户端；每次独占RunId。缺报告、零测试、数量错或任何失败均非零退出。
开发开关仅该测试进程可用；Editor/单机断言不是联机复制、Cook或人工视觉验收。
回退仅保留本轮Saved证据，不删除源码/资源。 #>
param(
    [string]$EngineRoot,
    [Parameter(Mandatory)][string]$BuildResultPath,
    [string[]]$Prefixes=@('GamePlatform.AbilitySystem.Cast','DivineBeasts.Presentation.DevelopmentCasting'),
    [ValidateRange(1,500)][int]$ExpectedCount=12,
    [guid]$RunId=[guid]::NewGuid()
)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1') -Force
$engine=Get-FoundationEngine $EngineRoot
foreach($prefix in $Prefixes){if($prefix -notmatch '^[A-Za-z0-9.]+$'){throw '测试前缀只允许稳定英文标识，不能执行额外控制台命令。'}}
# 验证完整Editor证据，而非凭旧DLL时间或编辑器能启动就宣称本轮源码已编译。
$requiredModules=@('GamePlatformAbilitySystem','DivineBeastsCharactersRuntime')
if(@($Prefixes|Where-Object {$_.StartsWith('DivineBeasts.Presentation.')}).Count -gt 0){$requiredModules+='DivineBeastsPresentationClient'}
# 远古石灵开发怪物已退役；施法回归只装配实际需要的平台与项目客户端模块。
$proof=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $requiredModules
$directory=Join-Path $workspace "Saved/Validation/CastingMonster20261013/Native-$RunId"
$null=New-Item -ItemType Directory -Path $directory
$report=Join-Path $directory 'Report'
# UE5.8 AutomationCommandline源码明确支持+合并StartsWith筛选，确保只运行本次指定用例。
$selector=($Prefixes | ForEach-Object {'StartsWith:'+$_}) -join '+'
$arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),'/DBAFrontEndPack/Maps/L_DBA_FrontEnd',
    '-unattended','-RenderOffscreen','-nosound','-nosplash','-NoLiveCoding','-stdout','-FullStdOutLogOutput','-DisablePlugins=Monolith',
    '-CustomConfig=VillageDevelopmentClient','-DBADevelopmentSkills','-textconfig',
    '-ini:EditorSettings:/Script/UnrealEd.EditorPerformanceSettings:bThrottleCPUWhenNotForeground=False',
    "-abslog=$(Join-Path $directory 'Unreal.log')","-ExecCmds=Automation RunTests $selector",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report")

$process=Invoke-FoundationProcess -FilePath (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -Arguments $arguments -WorkingDirectory $workspace -OutputDirectory $directory -TimeoutSeconds 900
if($process.ExitCode -ne 0){throw "真实原生进程失败：$($process.ExitCode)；证据=$directory"}
$index=Get-Content -LiteralPath (Join-Path $report 'index.json') -Raw | ConvertFrom-Json
$cases=@($index.tests | Where-Object {$testPath=$_.fullTestPath; @($Prefixes | Where-Object {$testPath.StartsWith($_+'.')}).Count -gt 0})
$failed=@($cases | Where-Object {$_.state -ne 'Success'})
# 成功与失败均落盘原生结果；不把预期RED改写成绿色，也不靠退出码0掩盖失败断言。
$result=[ordered]@{Passed=($cases.Count -eq $ExpectedCount -and $failed.Count -eq 0);Count=$cases.Count;Failed=$failed.Count;
    Build=$proof;Tests=@($cases|Select-Object fullTestPath,state);Report=(Join-Path $report 'index.json');NetworkAcceptance=$false;ManualVisualAccepted=$false}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $directory 'Result.json') -Encoding utf8
$result | ConvertTo-Json -Depth 4
if(-not $result.Passed){throw "要求恰好${ExpectedCount}项全部成功；实际$($cases.Count)项、失败$($failed.Count)；证据=$directory"}
