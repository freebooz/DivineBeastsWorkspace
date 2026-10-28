#requires -Version 7.0
<#
.SYNOPSIS
SSLM登录按钮专用桥接：复用唯一演示启动器及完整Editor证据门禁，不绕过认证或自动构建。
.DESCRIPTION
仅接收引擎根、新RunId与完整Editor构建结果路径，不接收账号、密码、可执行命令或任意地图。
BuildResultPath缺失或不匹配当前源码时由既有门禁失败关闭，工作目录按自身解析。
status.txt只报告启动进程状态，不代表门户Ready或插件验证通过。保留父登录窗口；本桥等待
本次游戏进程退出后结束，父界面可据此防止重复启动。不会关闭任何已有UE进程。
#>
param([Parameter(Mandatory)][string]$EngineRoot,[Parameter(Mandatory)][guid]$RunId,[string]$BuildResultPath)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($RunId -eq [guid]::Empty){throw 'RunId不能为空'}
$directory=Join-Path $root "Saved/Validation/PluginDemos/LoginLaunch/$RunId"
if(Test-Path -LiteralPath $directory){throw '拒绝复用登录启动证据目录'}
$null=New-Item -ItemType Directory -Path $directory
function Write-LaunchStatus([string]$Status){
    # 同目录临时文件替换，避免读取到一半的状态；只含固定枚举，无敏感信息。
    $temp=Join-Path $directory 'status.pending'
    [IO.File]::WriteAllText($temp,$Status,[Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temp,(Join-Path $directory 'status.txt'),$true)
}
function Test-PluginDemoChildIdentity {
    param([Diagnostics.Process]$Process,[object]$RecordedStart)
    try {
        # 先绑定并保留内核句柄，再读启动时间，避免退出与PID复用时校验/等待指向不同进程。
        if($Process.Handle -eq [IntPtr]::Zero){return $false}
        # ConvertFrom-Json 会把 ISO 时间直接转成 DateTime；再次 Parse 会丢失小数精度和时区。
        $recorded=[datetime]$RecordedStart
        return -not $Process.HasExited -and $Process.StartTime.ToUniversalTime().Ticks -eq $recorded.ToUniversalTime().Ticks
    } catch { return $false }
}
function Wait-PluginDemoChildExit {
    param([Diagnostics.Process]$Process)
    # 必须在退出之前取得并保留真实进程句柄；仅按PID重新附着后等待，退出码可能不可读取。
    if($Process.Handle -eq [IntPtr]::Zero){throw '无法取得门户进程句柄'}
    $Process.WaitForExit()
    $code=$Process.ExitCode
    if($null -eq $code){throw '无法读取门户进程退出码'}
    return [int]$code
}
Write-LaunchStatus 'Preparing'
try {
    # 桥不继承生产环境凭据到演示进程；OS运行必需环境保留，其余显式移除。
    $allow=@('SystemRoot','WINDIR','PATH','PATHEXT','TEMP','TMP','USERPROFILE','LOCALAPPDATA','APPDATA','PROGRAMDATA','PROGRAMFILES','PROGRAMFILES(X86)','PROGRAMW6432','COMSPEC','SYSTEMDRIVE','HOMEDRIVE','HOMEPATH','PROCESSOR_ARCHITECTURE','NUMBER_OF_PROCESSORS','PSModulePath')
    foreach($entry in @(Get-ChildItem Env:)){if($entry.Name -notin $allow){[Environment]::SetEnvironmentVariable($entry.Name,$null,'Process')}}
    # 返回JSON及详细异常只进入本次磁盘证据；UI只显示固定脱敏提示及RunId。
    # 显式指定已完成的完整构建证据；不自动选择可能属于旧源码的“最新”记录。
    $output=& (Join-Path $PSScriptRoot 'RunPluginDemo.ps1') -Start -Target EditorGame -ManualFixture -EngineRoot $EngineRoot -RunId $RunId -BuildResultPath $BuildResultPath
    $output | Set-Content -LiteralPath (Join-Path $directory 'launcher-output.log') -Encoding utf8
    $recordPath=Join-Path $root "Saved/Validation/PluginDemos/Launch/$RunId/Launch.json"
    $record=Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
    $child=[Diagnostics.Process]::GetProcessById([int]$record.PID)
    try {
        if(-not (Test-PluginDemoChildIdentity -Process $child -RecordedStart $record.StartedUtc)){throw '子进程身份失效'}
        Write-LaunchStatus 'Started'
        $code=Wait-PluginDemoChildExit -Process $child
    } finally { $child.Dispose() }
    if($code -ne 0){throw "演示子进程退出码：$code"}
    Write-LaunchStatus 'Exited'
    exit 0
} catch {
    $_.Exception.Message | Set-Content -LiteralPath (Join-Path $directory 'failure.log') -Encoding utf8
    Write-LaunchStatus 'Failed'
    exit 1
}
