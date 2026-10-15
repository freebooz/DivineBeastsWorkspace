#requires -Version 7.0
<#
职责：Windows本地开发MainArena进程池，复用正式Server Target、Profile和真实控制面。
输入：开发Cook后的ServerExe、区域、控制地址及必填PlayerData根地址；内部凭据仅从父进程环境读取，不写命令行/文件/日志。
输出：RunId独占目录中的非秘密进程身份、日志和池状态。每局结束由后端确认released后才替换本槽位进程。
生命周期：启动失败五次即停止；Stop.json或父进程退出时只清理PID/启动时间/路径均匹配的自有进程。
此运维进程定期核对控制面回收事实，不参与UI更新、不决定胜负；生产环境使用Agones/Fleet，不能用本脚本冒充集群验收。
#>
param([Parameter(Mandatory)][string]$ServerExe,
    [Parameter(Mandatory)][string]$ControlBaseUrl,
    [Parameter(Mandatory)][string]$PlayerDataBaseUrl,
    [string]$RegionId='us-west',[ValidateRange(1,8)][int]$Count=1,
    [ValidateRange(1024,65520)][int]$FirstPort=17881,[guid]$RunId=[guid]::NewGuid())
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskExe=(Resolve-Path -LiteralPath $ServerExe).Path
if([IO.Path]::GetFileName($taskExe)-ne'DivineBeastsArenaServer.exe'){throw '必须提供正式Server Target。'}
if(-not $taskExe.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '本地池只运行当前工作空间产物。'}
$taskUri=[uri]$ControlBaseUrl
if($taskUri.Scheme-ne'http'-or -not $taskUri.IsLoopback-or $taskUri.UserInfo-or $taskUri.Query-or $taskUri.Fragment){throw '本地控制面必须是无凭据回环HTTP地址。'}
$taskToken=[Environment]::GetEnvironmentVariable('GAMESERVERCONTROL_INTERNAL_TOKEN','Process')
if([string]::IsNullOrWhiteSpace($taskToken)){throw '缺少父进程内部控制凭据。'}
# 不能以控制面注册成功代替角色资料可读；缺失地址时必须在创建池及专服之前拒绝启动。
Import-Module (Join-Path $PSScriptRoot 'LocalPlayerDataEnvironment.psm1') -Force
$taskProbe=[Diagnostics.ProcessStartInfo]::new()
Set-LocalPlayerDataEnvironment -StartInfo $taskProbe -PlayerDataBaseUrl $PlayerDataBaseUrl -CheckReady
$taskDirectory=Join-Path $taskRoot "Saved/Validation/MainArenaPool/$RunId"
if(Test-Path -LiteralPath $taskDirectory){throw '拒绝覆盖既有池证据。'}
$null=New-Item -ItemType Directory -Path $taskDirectory
$taskHeaders=@{Authorization='Bearer '+$taskToken}
$taskSlots=@()
function Stop-OwnedArena($Slot){
    $taskChild=Get-Process -Id $Slot.PID -ErrorAction SilentlyContinue
    if($taskChild-and $taskChild.StartTime.ToUniversalTime().Ticks.ToString()-ceq $Slot.StartUtcTicks-and $taskChild.Path-ieq $taskExe){
        # 此处只终止本池自有进程；正常换代此前必须已获后端released且真实零人数确认。
        Stop-Process -Id $taskChild.Id -ErrorAction Stop
        $null=$taskChild.WaitForExit(5000)
    }
}
function Start-OwnedArena([int]$Index,[int]$Generation,[int]$Failures){
    $taskServerId="mainarena-local-$($RunId.ToString('N'))-$Index-$Generation"
    $taskBoot=[guid]::NewGuid().ToString('D')
    $taskPort=$FirstPort+$Index
    $taskLog=Join-Path $taskDirectory "Arena-$Index-$Generation.log"
    $taskInfo=[Diagnostics.ProcessStartInfo]::new()
    Set-LocalPlayerDataEnvironment -StartInfo $taskInfo -PlayerDataBaseUrl $PlayerDataBaseUrl
    $taskInfo.FileName=$taskExe;$taskInfo.WorkingDirectory=Split-Path $taskExe
    $taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
    $taskEnvironment=@{
        GAME_SERVER_ID=$taskServerId;GAME_SERVER_BOOT_ID=$taskBoot;GAME_SERVER_WORLD_ID='divinebeasts.world.main_arena@1'
        GAME_SERVER_REGION_ID=$RegionId;GAME_SERVER_CLUSTER_ID='local-development';GAME_SERVER_NODE_ID=$env:COMPUTERNAME
        GAME_SERVER_BUILD_VERSION='MainArena.Development.20261015.1';GAME_SERVER_PUBLIC_ENDPOINT="127.0.0.1:$taskPort"
        GAME_SERVER_CAPACITY='10';GAME_SERVER_PROTOCOL_VERSION='1';DBA_SERVER_PROFILE_ROOT=(Join-Path $taskRoot 'Deploy/Server')
        GAMESERVERCONTROL_BASE_URL=$ControlBaseUrl;GAMESERVERCONTROL_INTERNAL_TOKEN=$taskToken
    }
    foreach($taskName in $taskEnvironment.Keys){$taskInfo.Environment[$taskName]=[string]$taskEnvironment[$taskName]}
    foreach($taskArgument in @('/DBAWorldPack_MainArena/Maps/L_DBA_MainArena','-ServerRole=MainArena','-CustomConfig=VillageDevelopmentServer','-DBADevelopmentArena','-DBADevelopmentSkills','-textconfig','-server','-nullrhi','-nosound','-unattended','-nosplash','-nosteam',"-port=$taskPort",'-MULTIHOME=127.0.0.1',"-abslog=$taskLog")){$taskInfo.ArgumentList.Add($taskArgument)}
    $taskChild=[Diagnostics.Process]::Start($taskInfo)
    [pscustomobject]@{Index=$Index;Generation=$Generation;Failures=$Failures;PID=$taskChild.Id;StartUtcTicks=$taskChild.StartTime.ToUniversalTime().Ticks.ToString();Exe=$taskExe;ServerId=$taskServerId;BootId=$taskBoot;Port=$taskPort;Log=$taskLog;MatchId='';State='Starting'}
}
try{
    foreach($taskIndex in 0..($Count-1)){$taskSlots+=Start-OwnedArena $taskIndex 1 0}
    while(-not(Test-Path -LiteralPath (Join-Path $taskDirectory 'Stop.json'))){
        foreach($taskSlot in $taskSlots){
            $taskChild=Get-Process -Id $taskSlot.PID -ErrorAction SilentlyContinue
            $taskReplace=$null-eq$taskChild
            if($taskReplace){$taskSlot.Failures++;if($taskSlot.Failures-ge5){throw '本地竞技进程连续失败五次；查看本池日志。'}}
            else{
                try{
                    if(-not $taskSlot.MatchId){
                        $taskReadHeaders=$taskHeaders.Clone();$taskReadHeaders['X-Game-Server-Id']=$taskSlot.ServerId;$taskReadHeaders['X-Game-Server-Boot-Id']=$taskSlot.BootId
                        $taskAssignment=Invoke-RestMethod -Uri ($ControlBaseUrl.TrimEnd('/')+'/internal/v1/gameservers/assignment?gameServerId='+[uri]::EscapeDataString($taskSlot.ServerId)) -Headers $taskReadHeaders -TimeoutSec 3
                        if($taskAssignment.Arena){$taskSlot.MatchId=$taskAssignment.Arena.MatchID;$taskSlot.State='Assigned'}
                    }else{
                        $taskBody=@{matchId=$taskSlot.MatchId}|ConvertTo-Json -Compress
                        $taskStatus=Invoke-RestMethod -Method Post -Uri ($ControlBaseUrl.TrimEnd('/')+'/internal/v1/arena/status') -Headers $taskHeaders -ContentType 'application/json' -Body $taskBody -TimeoutSec 3
                        if($taskStatus.Record.State-eq'released'-and $taskStatus.CurrentPlayers-eq0){Stop-OwnedArena $taskSlot;$taskReplace=$true;$taskSlot.Failures=0}
                    }
                }catch{
                    # 尚未注册/分配或控制面短暂失联时保留实例，不按本地超时擅自回收仍有玩家的进程。
                    $taskSlot.State='WaitingForControlFact'
                }
            }
            if($taskReplace){$taskNew=Start-OwnedArena $taskSlot.Index ($taskSlot.Generation+1) $taskSlot.Failures;foreach($taskProperty in $taskNew.PSObject.Properties){$taskSlot.($taskProperty.Name)=$taskProperty.Value}}
        }
        $taskSlots|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $taskDirectory 'Pool.json') -Encoding utf8
        Start-Sleep -Seconds 3
    }
}finally{foreach($taskSlot in $taskSlots){Stop-OwnedArena $taskSlot};$taskToken=$null;$taskHeaders.Clear()}
