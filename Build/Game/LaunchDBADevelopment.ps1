#requires -Version 7.0
<#
.SYNOPSIS
神兽联盟开发启动器：Debug（真实登录）/PluginDev（免登录插件演示门户）。
.DESCRIPTION
显式-Start才能启动；默认Debug不设置任何登录旁路。
插件开发模式复用既有RunPluginDemo.ps1，地图缺失将失败关闭。
只启动本次子进程，输出RunId独立日志，不写认证凭据，不修改正式默认启动地图。
#>
param(
    [ValidateSet('Debug','PluginDev')][string]$Mode='Debug',
    [switch]$Start,
    [ValidateSet('EditorGame','Client')][string]$Target='EditorGame',
    [string]$EngineRoot,
    [string]$ClientExe,
    [ValidateRange(960,3840)][int]$WindowWidth=1920,
    [ValidateRange(540,2160)][int]$WindowHeight=1080,
    [guid]$RunId=[guid]::NewGuid()
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
if(-not $Start){throw '必须显式传入-Start，避免检查脚本误启动游戏。'}
if($RunId -eq [guid]::Empty){throw 'RunId不能为空。'}
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if([IO.Path]::GetFileName($workspace) -ne 'DivineBeastsWorkspace'){
    throw '只能从正式DivineBeastsWorkspace启动。'
}
$project=Join-Path $workspace 'Game/DivineBeastsArena.uproject'
Assert-FoundationFile $project

if($Mode -eq 'PluginDev'){
    # 使用唯一已有的免登录开关/地图门禁，禁止第二套绕过规则。
    # PowerShell脚本参数必须具名Splat（散列参数），不能用字符串数组展开当原生命令行参数。
    # 原写法会把'-Start'误解析为Target参数，导致实际双模式启动被拒绝。
    $delegate=@{
        Start=$true
        Target=$Target
        WindowWidth=$WindowWidth
        WindowHeight=$WindowHeight
        RunId=$RunId
    }
    if($EngineRoot){$delegate['EngineRoot']=$EngineRoot}
    if($ClientExe){$delegate['ClientExe']=$ClientExe}
    & (Join-Path $PSScriptRoot 'RunPluginDemo.ps1') @delegate
    return
}

$map='/DBAFrontEndPack/Maps/L_DBA_FrontEnd'
if($Target -eq 'EditorGame'){
    $engine=Get-FoundationEngine $EngineRoot
    $exe=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
    $arguments=@($project,$map,'-game')
}else{
    if(-not $ClientExe){throw 'Client模式需明确提供开发客户端程序。'}
    $exe=(Resolve-Path -LiteralPath $ClientExe -ErrorAction Stop).Path
    if([IO.Path]::GetFileName($exe) -ne 'DivineBeastsArenaClient.exe'){
        throw '不允许启动非正式的DivineBeastsArenaClient程序。'
    }
    $arguments=@($map)
}
Assert-FoundationFile $exe
$dir=Join-Path $workspace ("Saved/Validation/PluginDemos/Launch/"+$RunId.ToString('D'))
if(Test-Path -LiteralPath $dir){throw ('RunId已存在，禁止覆盖证据：'+$dir)}
$null=New-Item -ItemType Directory -Path $dir -Force
$log=Join-Path $dir 'DebugLogin.log'
$arguments+=@("-DBADevelopmentPowerShell=$(Join-Path $PSHOME 'pwsh.exe')",'-windowed',("-ResX="+$WindowWidth),("-ResY="+$WindowHeight),
    '-NoLiveCoding','-NoHotReload','-nosplash',("-abslog="+$log))
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=$exe
$start.WorkingDirectory=Split-Path $exe
$start.UseShellExecute=$false
$start.CreateNoWindow=$false
foreach($argument in $arguments){$start.ArgumentList.Add([string]$argument)}
# 不使用可被记录在进程命令行中的账号、密码；仍走真实应用流程。
$null=$start.Environment.Remove('DBA_DEV_LOGIN_SECRET')
$child=[Diagnostics.Process]::Start($start)
if(-not $child){throw '编辑器客户端未能启动。'}
$info=[ordered]@{
    RunId=$RunId.ToString('D');Mode='Debug';Target=$Target;ProcessId=$child.Id;
    Map=$map;Executable=$exe;Log=$log;StartedUtc=[DateTime]::UtcNow.ToString('O');
    ManualAuthentication=$true;DemoBypassEnabled=$false
}
$info|ConvertTo-Json -Depth 5|Set-Content (Join-Path $dir 'Launch.json') -Encoding utf8
$info|ConvertTo-Json -Depth 5
