#requires -Version 7.0
<#
.SYNOPSIS
启动可见前端客户端，首屏保留空白用户登录表单，不自动认证或进入角色页。
.DESCRIPTION
输入已验证客户端可执行文件、窗口数量及本地后端地址；只启动新进程，不终止其他客户端。
从子进程环境删除开发密码，不传入开发账号/自动登录参数；账号密码由用户在登录页输入。
输出Saved下独占运行证据和准确进程身份；启动成功不代表登录、角色选择或Village准入通过。
#>
param([Parameter(Mandatory)][string]$ClientExe,[ValidateRange(1,4)][int]$Count=2,
    # 同机双窗口验证默认每端60帧，避免无限制渲染争用显卡；0保留游戏设置，不修改持久化玩家偏好。
    [ValidateRange(0,240)][int]$FrameRateLimit=60,
    # 只记录运动/复制的事件采样，供人工移动后按三端时间戳定位延迟，不包含登录信息。
    [switch]$MovementDiagnostics,
    [string]$GatewayUrl='http://127.0.0.1:28081',[guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe=(Resolve-Path -LiteralPath $ClientExe).Path
if ([IO.Path]::GetFileName($exe)-ne'DivineBeastsArenaClient.exe') { throw '必须指定正式Client Target产物。' }
$url=[uri]$GatewayUrl
if (!$url.IsAbsoluteUri -or $url.Scheme -notin @('http','https') -or $url.UserInfo -or $url.Query -or $url.Fragment) { throw '后端地址必须为不含凭据的HTTP(S)服务地址。' }
$directory=Join-Path $workspace "Saved/Validation/FrontEndClients/$RunId"
if (Test-Path -LiteralPath $directory) { throw '拒绝复用本次启动证据目录。' }
$null=New-Item -ItemType Directory -Path $directory
$records=@()
foreach($i in 1..$Count) {
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$exe; $info.WorkingDirectory=Split-Path $exe
    $info.UseShellExecute=$false; $info.CreateNoWindow=$false; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Normal
    $info.Environment.Remove('DBA_DEV_LOGIN_SECRET')|Out-Null
    $info.Environment['DIVINEBEASTS_GATEWAY_BASE_URL']=$GatewayUrl
    $log=Join-Path $directory "Client$i.log"
    foreach($argument in @('/DBAFrontEndPack/Maps/L_DBA_FrontEnd','-windowed','-ResX=960','-ResY=540',"-WinX=$((($i-1)*980)+20)",'-WinY=60','-nosplash',"-abslog=$log")) {
        $info.ArgumentList.Add($argument)
    }
    if ($FrameRateLimit -gt 0) { $info.ArgumentList.Add("-ExecCmds=t.MaxFPS $FrameRateLimit") }
    if ($MovementDiagnostics) { $info.ArgumentList.Add('-DBAMovementDiagnostics') }
    $process=[Diagnostics.Process]::Start($info)
    $records+=@{PID=$process.Id;Exe=$exe;StartUtcTicks=$process.StartTime.ToUniversalTime().Ticks;Log=$log;InitialScreen='Login';AutoLogin=$false;FrameRateLimit=$FrameRateLimit;MovementDiagnostics=[bool]$MovementDiagnostics}
}
$records|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $directory 'Clients.json') -Encoding utf8
$records|Select-Object PID,InitialScreen,AutoLogin|ConvertTo-Json
Write-Host "Evidence: $directory"
