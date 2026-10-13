#requires -Version 7.0
<#
用途：恢复SSLM唯一正式UE5.8编辑器的最小必要构建，不执行客户端/服务器/Cook/UE测试。
先构建且核对BuildFoundation退出码，再尝试启动原主工程。
已有Editor进程、错误引擎/工程路径、磁盘不一致时拒绝继续。
不清理Engine/Intermediate、不删除历史DLL、不改写.modules或BuildId，不终止其它会话进程。
本脚本仅供用户明确授权的启动修复；完整三目标构建仍属于最终统一验收。
#>
param(
    [string]$EngineRoot = 'F:\UnrealEngine-5.8.0-release',
    [ValidateRange(1,8)][int]$MaxParallelActions = 2,
    [ValidateRange(300,86400)][int]$BuildTimeoutSeconds = 7200,
    # 可选：仅构建需要修复的Editor模块；不代表完整Editor构建验收。
    [ValidatePattern('^[A-Za-z][A-Za-z0-9_]*$')][string[]]$OnlyModules = @(),
    [switch]$SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project = Join-Path $workspace 'Game/DivineBeastsArena.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$build = Join-Path $PSScriptRoot 'BuildFoundation.ps1'
if ($SkipBuild -and $OnlyModules.Count) {
    throw 'SkipBuild（跳过编译）与OnlyModules（指定编译模块）不可同时使用'
}
$expectedRootName = [IO.Path]::GetFileName($workspace.TrimEnd('\','/'))
if ($expectedRootName -ne 'DivineBeastsWorkspace') {
    throw '正式工作空间名称不符；拒绝启动其他工程'
}
foreach ($path in @($project, $editor, $build, (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw ('编辑器启动前置不存在：' + $path)
    }
}
$projectText = Get-Content -Raw -LiteralPath $project -Encoding utf8
$projectJson = $projectText | ConvertFrom-Json
if (@($projectJson.Modules | Where-Object { $_.Name -eq 'DivineBeastsArena' }).Count -ne 1) {
    throw '正式主工程模块身份不符'
}
# 不关闭其它代理正在使用的Editor或UE构建；只做进程冲突检测。
$existing = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue)
if ($existing.Count) {
    $details = $existing | ForEach-Object { "$($_.ProcessId):$($_.CommandLine)" }
    throw ('检测到UE Editor已运行；不重复编译/启动。' + ($details -join '; '))
}
$validationRoot = Join-Path $workspace 'Saved/Validation/MaterialLibrary/EditorStartupRecovery'
$runId = [Guid]::NewGuid().ToString('N')
$reportDir = Join-Path $validationRoot $runId
$null = New-Item -ItemType Directory -Path $reportDir -Force
$report = Join-Path $reportDir 'Result.json'
$state = [ordered]@{
    RunId = $runId
    DateUtc = [DateTime]::UtcNow.ToString('o')
    Workspace = $workspace
    EngineRoot = $EngineRoot
    Project = $project
    EditorExe = $editor
    MinimalEditorBuild = (-not $SkipBuild)
    Scope = 'Only DivineBeastsArenaEditor Development Win64; never Client/Server/Cook'
    BuildMode = $(if ($SkipBuild) { 'SkipBuild' } elseif ($OnlyModules.Count) { 'EditorOnlyModules' } else { 'EditorTarget' })
    OnlyModules = @($OnlyModules)
    State = 'Started'
    BuildExitCode = $null
    EditorPid = $null
    ReadinessConfirmed = $false
    Error = $null
}
try {
    if (-not $SkipBuild) {
        $state.State = 'BuildingEditor'
        $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $report -Encoding utf8
        Write-Host ('[SSLM] 构建Editor目标，记录目录：' + $reportDir)
        # 复用已审查的Foundation锁、UBT互斥、只拥有本次PID、完整日志和退出码记录。
        # 仅装配Editor目标；可显式限定缺失模块以缩小启动修复的构建范围。
        $buildOptions = @{
            EngineRoot = $EngineRoot
            Editor = $true
            MaxParallelActions = $MaxParallelActions
            TimeoutSeconds = $BuildTimeoutSeconds
            NoUBA = $true
        }
        if ($OnlyModules.Count) { $buildOptions.OnlyModules = @($OnlyModules) }
        & $build @buildOptions
        $state.BuildExitCode = $LASTEXITCODE
        if ($LASTEXITCODE -ne 0) {
            throw ('真实Editor最小必要构建失败，退出码：' + $LASTEXITCODE)
        }
    } else {
        Write-Host '[SSLM] 仅检查现有Editor模块是否已由其它流程恢复；不编译'
    }
    $existing = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue)
    if ($existing.Count) { throw '构建期间已有其他Editor实例启动；拒绝重复启动' }
    $state.State = 'LaunchingEditor'
    $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $report -Encoding utf8
    $p = Start-Process -FilePath $editor -ArgumentList @((('"' + $project + '"')), '-log', '-NoSplash', '-NoCompile', '-NoLiveCoding') -PassThru
    $state.EditorPid = $p.Id
    Write-Host ('[SSLM] 启动Editor，进程PID：' + $p.Id)
    Start-Sleep -Seconds 18
    $p.Refresh()
    if ($p.HasExited) { throw ('编辑器启动后早期退出，退出码：' + $p.ExitCode) }
    $state.State = 'EditorProcessRunning'
    # PID持续存活只证明进程，Monolith正式工程会话与材质图仍须在后续步骤独立确认。
    $state.ReadinessConfirmed = $false
    Write-Host '[SSLM] 编辑器进程已启动；待Monolith确认正式项目就绪'
} catch {
    $state.State = 'Failed'
    $state.Error = $_.Exception.Message
    Write-Error $_.Exception.Message
} finally {
    $state.FinishedUtc = [DateTime]::UtcNow.ToString('o')
    $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $report -Encoding utf8
    Write-Host ('[SSLM] 执行记录：' + $report)
}
if ($state.State -eq 'Failed') { exit 1 }
exit 0
