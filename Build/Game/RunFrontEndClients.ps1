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
    # 用户放大后的角色创建面板至少需要1920×1080工作区；仅调整窗口，不缩放文字/输入。
    # 保留960×540可显式选择用于登录小窗口回归，但不代表大型角色页完整可见。
    [ValidateRange(960,3840)][int]$WindowWidth=1920,
    [ValidateRange(540,2160)][int]$WindowHeight=1080,
    # 同机双窗口验证默认每端60帧，避免无限制渲染争用显卡；0保留游戏设置，不修改持久化玩家偏好。
    [ValidateRange(0,240)][int]$FrameRateLimit=60,
    # 只记录运动/复制的事件采样，供人工移动后按三端时间戳定位延迟，不包含登录信息。
    [switch]$MovementDiagnostics,
    # 仅显式开发验证加载已批准候选技能；三个参数共同选择文本INI、开发配置及运行开关。
    # 首屏仍为人工登录，不向进程传账号或密码；默认false保持正式前端流程。
    [switch]$DevelopmentSkills,
    # 怪物仅在已启用开发技能的人工验证配置内创建，仍须独立运行开关，默认禁止正式流程生成。
    [switch]$DevelopmentMonsters,
    # 真实组队竞技开发入口；必须使用同一开发技能/内容Cook，首屏仍为人工登录。
    [switch]$DevelopmentArena,
    # 历史28081与28181拥有独立身份和玩家资料；当前本地保留28181，调用方仍须明确选择目标网关。
    [string]$GatewayUrl,
    [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
if ($DevelopmentArena -and !$DevelopmentSkills) { throw '开发竞技必须使用包含必要英雄定义和技能的DevelopmentSkills配置。' }
if ($DevelopmentMonsters -and !$DevelopmentSkills) { throw '开发怪物验证必须同时显式启用DevelopmentSkills。' }
$gatewayModulePath=Join-Path $PSScriptRoot 'FrontEndClientGateway.psm1'
Import-Module $gatewayModulePath -Force
# 在创建任何证据目录或启动客户端前拒绝缺失/非法网关，防止把用户引导到错误的角色档案环境。
$GatewayUrl=Resolve-FrontEndClientGatewayUrl -GatewayUrl $GatewayUrl
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe=(Resolve-Path -LiteralPath $ClientExe).Path
if ([IO.Path]::GetFileName($exe)-ne'DivineBeastsArenaClient.exe') { throw '必须指定正式Client Target产物。' }
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
    foreach($argument in @('/DBAFrontEndPack/Maps/L_DBA_FrontEnd','-windowed',"-ResX=$WindowWidth","-ResY=$WindowHeight","-WinX=$((($i-1)*40)+20)",'-WinY=40','-nosplash',"-abslog=$log")) {
        $info.ArgumentList.Add($argument)
    }
    if ($FrameRateLimit -gt 0) { $info.ArgumentList.Add("-ExecCmds=t.MaxFPS $FrameRateLimit") }
    if ($MovementDiagnostics) { $info.ArgumentList.Add('-DBAMovementDiagnostics') }
    if ($DevelopmentSkills) {
        foreach ($argument in @('-CustomConfig=VillageDevelopmentClient','-DBADevelopmentSkills','-textconfig','-dumpiniloads')) {
            $info.ArgumentList.Add($argument)
        }
    }
    if ($DevelopmentMonsters) { $info.ArgumentList.Add('-DBADevelopmentMonsters') }
    if ($DevelopmentArena) { $info.ArgumentList.Add('-DBADevelopmentArena') }
    $process=[Diagnostics.Process]::Start($info)
    $records+=@{PID=$process.Id;Exe=$exe;StartUtcTicks=$process.StartTime.ToUniversalTime().Ticks;Log=$log;GatewayUrl=$GatewayUrl;InitialScreen='Login';AutoLogin=$false;FrameRateLimit=$FrameRateLimit;WindowWidth=$WindowWidth;WindowHeight=$WindowHeight;MovementDiagnostics=[bool]$MovementDiagnostics;DevelopmentSkills=[bool]$DevelopmentSkills;DevelopmentMonsters=[bool]$DevelopmentMonsters;DevelopmentArena=[bool]$DevelopmentArena}
}
$records|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $directory 'Clients.json') -Encoding utf8
$records|Select-Object PID,InitialScreen,AutoLogin|ConvertTo-Json
Write-Host "Evidence: $directory"
