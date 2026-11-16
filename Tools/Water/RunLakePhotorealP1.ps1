[CmdletBinding()]
param(
    [ValidateSet('inspect','materials','material_verify','map_copy','map_bind','map_verify','capture')]
    [string]$Mode = 'inspect',
    [string]$RunId = 'Manual',
    [string]$EngineRoot = 'D:\UnrealEngine-5.8.0-release'
)
$ErrorActionPreference = 'Stop'
# 工具只驱动神兽联盟独立水体审核资产，不结束任何已有编辑器／构建进程。
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $workspace 'Game/DivineBeastsArena.uproject'
$author = Join-Path $PSScriptRoot 'AuthorLakePhotorealP1UE58.py'
$exe = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($path in @($project,$author,$exe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "必需文件缺失：$path" }
}
if ($RunId -notmatch '^[A-Za-z0-9_-]{1,64}$') { throw 'RunId必须使用不超过64字符的ASCII身份' }
if ($Mode -eq 'inspect') {
    Write-Output "P1水体独立审图：默认不写入；需要原生UE模式时明确传入Mode"
    Write-Output "正式Village不修改；原生资产作者=$author"
    exit 0
}
$running = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('UnrealEditor.exe','UnrealEditor-Cmd.exe','UnrealBuildTool.exe')
})
if ($running.Count -gt 0) {
    throw "有其他UE编辑器或UBT正在运行：$($running.ProcessId -join ',')，禁止共享工程同时写入"
}
$out = Join-Path $workspace 'Saved/Validation/WaterLakePhotorealP1_20261024'
[IO.Directory]::CreateDirectory($out) | Out-Null
$log = Join-Path $out ("UE_" + $Mode + "_" + $RunId + ".log")
if (Test-Path -LiteralPath $log) { throw "旧日志已存在，拒绝覆盖：$log" }
$env:SSLM_WATER_P1_MODE = $Mode
$map = if ($Mode -eq 'capture') {
    '/Game/Development/Water/Maps/L_SSLM_LakePhotorealP1'
} else { '/Engine/Maps/Entry' }
$argv = @(
    $project, $map, ("-ExecutePythonScript=" + $author),
    '-unattended','-NoSplash','-nop4','-NoLiveCoding',
    '-NoCompile','-NoHotReload','-DisablePlugins=Monolith',
    ("-abslog=" + $log)
)
if ($Mode -ne 'capture') { $argv += '-NullRHI' }
else { $argv += '-d3d12' }
# PowerShell Start-Process -Wait可能等待已退出编辑器的子进程；只等待本次直接启动的UE进程。
$proc = Start-Process -FilePath $exe -ArgumentList $argv -PassThru -WindowStyle Hidden
Write-Output "P1_UE_PID=$($proc.Id) MODE=$Mode LOG=$log"
$finished = $proc.WaitForExit(120000)
if (-not $finished) { throw '本次UE进程尚未退出，未停止该进程；不继续写入其他资源' }
Write-Output "P1_UE_EXIT=$($proc.ExitCode)"
if (-not (Test-Path -LiteralPath $log)) { throw "UE原生日志未生成" }
$errors = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error|Fatal error|LogPluginManager: Error' -ErrorAction SilentlyContinue)
$results = @(Select-String -LiteralPath $log -Pattern 'SSLM_WATER_P1_' -ErrorAction SilentlyContinue)
$results | Select-Object -Last 35 | ForEach-Object {Write-Output $_.Line}
if ($errors.Count -gt 0) {
    $errors | Select-Object -Last 8 | ForEach-Object {Write-Output $_.Line}
    throw 'UE虽可能退出0，但Python/引擎原生日志存在错误'
}
$required = switch ($Mode) {
    'materials' {'SSLM_WATER_P1_MATERIAL_VERIFIED'}
    'material_verify' {'SSLM_WATER_P1_MATERIAL_VERIFIED'}
    'map_copy' {'SSLM_WATER_P1_MAP_COPIED'}
    'map_bind' {'SSLM_WATER_P1_MAP_BOUND'}
    'map_verify' {'SSLM_WATER_P1_MAP_VERIFIED'}
    'capture' {'SSLM_WATER_P1_CAPTURE_COMPLETE'}
}
if ($proc.ExitCode -ne 0 -or -not (@($results | Where-Object { $_.Line -like ("*" + $required + "*") }).Count)) {
    throw "UE运行退出或本模式必须成功标记缺失：$required"
}
Write-Output "P1_MODE_PASSED=$Mode"
