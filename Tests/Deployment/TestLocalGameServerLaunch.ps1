#requires -Version 7.0
<#
职责：本地专服启动回归。先检查真实启动入口强制声明PlayerData，再调用实际环境装配函数。
范围：部署工具层；不启动游戏、不读取凭据、不修改玩家数据。进程环境验证不能替代真实角色准入。
关键回归：遗漏资料地址必须在创建进程前失败，合法地址须进入子进程而不污染父进程。
#>
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
foreach($taskRelative in @('Deploy/Local/RunMainArenaPool.ps1','Deploy/Local/StartVillageDevelopment.ps1')){
    $taskPath=Join-Path $taskRoot $taskRelative
    if(!(Test-Path -LiteralPath $taskPath)){throw "缺少正式启动入口：$taskRelative"}
    $taskTokens=$null;$taskErrors=$null
    $taskAst=[Management.Automation.Language.Parser]::ParseFile($taskPath,[ref]$taskTokens,[ref]$taskErrors)
    if($taskErrors.Count){throw "启动入口无法解析：$taskRelative"}
    $taskParameter=$taskAst.ParamBlock.Parameters|Where-Object {$_.Name.VariablePath.UserPath-eq'PlayerDataBaseUrl'}
    if(!$taskParameter-or$taskParameter.DefaultValue-or$taskParameter.Extent.Text-notmatch 'Mandatory'){
        throw "PlayerData地址必须显式且必填，防止已注册专服在角色准入时失败：$taskRelative"
    }
}
Import-Module (Join-Path $taskRoot 'Deploy/Local/LocalPlayerDataEnvironment.psm1') -Force
$taskOriginal=[Environment]::GetEnvironmentVariable('PLAYERDATA_BASE_URL','Process')
$taskInfo=[Diagnostics.ProcessStartInfo]::new()
$taskInfo.Environment['UNCHANGED_TEST_VALUE']='preserved'
Set-LocalPlayerDataEnvironment -StartInfo $taskInfo -PlayerDataBaseUrl 'http://127.0.0.1:18182/'
if($taskInfo.Environment['PLAYERDATA_BASE_URL']-cne'http://127.0.0.1:18182'){throw '子进程未收到显式PlayerData地址。'}
if($taskInfo.Environment['UNCHANGED_TEST_VALUE']-cne'preserved'){throw '误改无关子进程环境。'}
if([Environment]::GetEnvironmentVariable('PLAYERDATA_BASE_URL','Process')-cne$taskOriginal){throw '污染了父进程配置。'}
foreach($taskInvalid in @('', '/relative', 'ftp://127.0.0.1:18182', 'http://example.com', 'http://user:secret@127.0.0.1', 'http://127.0.0.1:18182?x=1', 'http://127.0.0.1:18182#x', "http://127.0.0.1:18182`n", ' http://127.0.0.1:18182', 'http://127.0.0.1:18182 ', 'http:\\127.0.0.1:18182', 'http:/\127.0.0.1:18182')){
    $taskRejected=$false
    try{Set-LocalPlayerDataEnvironment -StartInfo $taskInfo -PlayerDataBaseUrl $taskInvalid}catch{$taskRejected=$true}
    if(!$taskRejected){throw '未拒绝缺失或非法本地PlayerData地址。'}
}
Write-Output 'LOCAL_GAME_SERVER_PLAYERDATA_PASS: two launchers, child environment, parent isolation, twelve invalid inputs'
