#requires -Version 7.0
<#
职责：启动一台本地Village开发专服，复用正式Server Target/Profile和现有控制面、资料服务。
输入：显式ServerExe、ControlBaseUrl、PlayerDataBaseUrl；内部凭据只继承父进程环境，不进入参数或证据。
副作用：创建独立Saved运行目录和一个隐藏专服，不结束其他进程、不重建后端、不改账号角色数据。
生命周期：输出PID/启动时间/路径供调用方精确管理；启动不等于玩家准入成功。缺失资料地址/健康失败在创建进程前拒绝。
#>
param([Parameter(Mandatory)][string]$ServerExe,
    [Parameter(Mandatory)][string]$ControlBaseUrl,
    [Parameter(Mandatory)][string]$PlayerDataBaseUrl,
    [string]$RegionId='us-west',[ValidateRange(1024,65535)][int]$Port=17781,
    [switch]$DevelopmentArena,[switch]$DevelopmentMonsters,[guid]$RunId=[guid]::NewGuid())
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskExe=(Resolve-Path -LiteralPath $ServerExe).Path
if([IO.Path]::GetFileName($taskExe)-ne'DivineBeastsArenaServer.exe'-or
    !$taskExe.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
    throw '必须使用当前工作空间正式Server Target产物。'
}
$taskControl=[uri]$ControlBaseUrl
if($taskControl.Scheme-ne'http'-or!$taskControl.IsLoopback-or$taskControl.UserInfo-or$taskControl.Query-or$taskControl.Fragment){throw '控制面必须是无凭据回环HTTP地址。'}
$taskToken=[Environment]::GetEnvironmentVariable('GAMESERVERCONTROL_INTERNAL_TOKEN','Process')
if([string]::IsNullOrWhiteSpace($taskToken)){throw '缺少父进程内部控制凭据。'}
$taskInfo=$null;$taskEnvironment=$null;$taskChild=$null
try {
Import-Module (Join-Path $PSScriptRoot 'LocalPlayerDataEnvironment.psm1') -Force
$taskInfo=[Diagnostics.ProcessStartInfo]::new()
Set-LocalPlayerDataEnvironment -StartInfo $taskInfo -PlayerDataBaseUrl $PlayerDataBaseUrl -CheckReady
$taskDirectory=Join-Path $taskRoot "Saved/Validation/VillageDevelopment/$RunId"
if(Test-Path -LiteralPath $taskDirectory){throw '拒绝覆盖既有运行证据。'}
$null=New-Item -ItemType Directory -Path $taskDirectory
$taskLog=Join-Path $taskDirectory 'VillageServer.log'
$taskServerId='village-local-'+$RunId.ToString('N');$taskBoot=[guid]::NewGuid().ToString('D')
$taskInfo.FileName=$taskExe;$taskInfo.WorkingDirectory=Split-Path $taskExe
$taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$taskEnvironment=@{
    GAME_SERVER_ID=$taskServerId;GAME_SERVER_BOOT_ID=$taskBoot;GAME_SERVER_WORLD_ID='divinebeasts.world.village_tutorial@1'
    GAME_SERVER_REGION_ID=$RegionId;GAME_SERVER_CLUSTER_ID='local-development';GAME_SERVER_NODE_ID=$env:COMPUTERNAME
    GAME_SERVER_BUILD_VERSION='PartyArena.Development.20261015.1';GAME_SERVER_PUBLIC_ENDPOINT="127.0.0.1:$Port"
    GAME_SERVER_CAPACITY='20';GAME_SERVER_PROTOCOL_VERSION='1';DBA_SERVER_PROFILE_ROOT=(Join-Path $taskRoot 'Deploy/Server')
    GAMESERVERCONTROL_BASE_URL=$ControlBaseUrl;GAMESERVERCONTROL_INTERNAL_TOKEN=$taskToken
}
foreach($taskName in $taskEnvironment.Keys){$taskInfo.Environment[$taskName]=[string]$taskEnvironment[$taskName]}
foreach($taskArgument in @('/DBAWorldPack_Village/Maps/L_Village_Start','-ServerRole=Village','-CustomConfig=VillageDevelopmentServer',
    '-DBADevelopmentSkills','-textconfig','-server','-nullrhi','-nosound','-unattended','-nosplash','-nosteam',"-port=$Port",'-MULTIHOME=127.0.0.1',"-abslog=$taskLog")){$taskInfo.ArgumentList.Add($taskArgument)}
if($DevelopmentArena){$taskInfo.ArgumentList.Add('-DBADevelopmentArena')}
if($DevelopmentMonsters){$taskInfo.ArgumentList.Add('-DBADevelopmentMonsters')}
$taskChild=[Diagnostics.Process]::Start($taskInfo)
$taskRecord=[pscustomobject]@{PID=$taskChild.Id;StartUtcTicks=$taskChild.StartTime.ToUniversalTime().Ticks.ToString();Exe=$taskExe;
    ServerId=$taskServerId;BootId=$taskBoot;Port=$Port;Log=$taskLog;PlayerDataConfigured=$true;PlayerAdmissionVerified=$false}
$taskRecord|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskDirectory 'Server.json') -Encoding utf8
$taskRecord|ConvertTo-Json
} catch {
    # Start返回的Process持有本次创建的Windows进程句柄；证据落盘失败时不按可复用PID寻找或清理其他进程。
    if($taskChild-and!$taskChild.HasExited){$taskChild.Kill();$null=$taskChild.WaitForExit(5000)}
    throw
} finally {
    $taskToken=$null
    if($taskEnvironment){$taskEnvironment.Clear()}
    if($taskInfo){$taskInfo.Environment.Clear()}
    if($taskChild){$taskChild.Dispose()}
}
