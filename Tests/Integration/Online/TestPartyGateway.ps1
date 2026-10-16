#requires -Version 7.0
<#
职责：真实本地Gateway/Identity/Match HTTP组队回归，验证首次邀请、观察通知、入队、转队长与解散。
输入：-Run、回环Gateway和明确本地Compose项目；只在内存读取现有开发账号种子，挑两名当前无队伍用户。
副作用：创建短期登录会话及一支本测试队伍；finally只清理本轮创建的队伍和会话，不改角色/现有队伍。
输出：不含凭据的步骤报告；不自动操作游戏窗口，不把HTTP通过当作UE旅行或人工UI验收。
#>
param([switch]$Run,[string]$GatewayUrl='http://127.0.0.1:28181',
 [string]$ComposeProject='sslm-validation-8238653a',[string]$ReportPath)
$ErrorActionPreference='Stop'
if(!$Run){throw '需要显式-Run，确认执行真实本地接口回归。'}
$taskUri=[uri]$GatewayUrl
if($taskUri.Scheme-ne'http'-or!$taskUri.IsLoopback-or$taskUri.UserInfo-or$taskUri.Query-or$taskUri.Fragment){throw '只接受无凭据的回环HTTP网关。'}
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if(!$ReportPath){$ReportPath=Join-Path $taskRoot ('Saved/Validation/PartyGateway/'+[guid]::NewGuid().ToString('N')+'/Result.json')}
$taskReportFull=[IO.Path]::GetFullPath($ReportPath)
if(!$taskReportFull.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '报告必须写在本工作空间。'}
$taskSessions=@();$taskSelected=@();$taskPartyId='';$taskSteps=@();$taskPassed=$false;$taskHttp=$null
# 所有请求只携带本人令牌；令牌不进入URL、报告或命令行，底层异常不输出请求正文。
function Invoke-PartyPublic([string]$Method,[string]$Path,$Login,$Payload){
 $arguments=@{Method=$Method;Uri=($GatewayUrl.TrimEnd('/')+$Path);TimeoutSec=5}
 if($Login){$arguments.Headers=@{Authorization='Bearer '+$Login.accessToken}}
 if($null-ne$Payload){$arguments.ContentType='application/json';$arguments.Body=($Payload|ConvertTo-Json -Depth 6 -Compress)}
 Invoke-RestMethod @arguments
}
try{
 $taskIdentity=(docker inspect ($ComposeProject+'-identityservice-1')|ConvertFrom-Json)[0]
 if($LASTEXITCODE-ne0){throw '无法读取指定开发身份容器。'}
 $taskSeedsText=($taskIdentity.Config.Env|Where-Object {$_.StartsWith('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON=')}).Substring('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON='.Length)
 $taskSeeds=@($taskSeedsText|ConvertFrom-Json)
 for($index=$taskSeeds.Count-1;$index-ge0-and$taskSelected.Count-lt2;$index--){
  $seed=$taskSeeds[$index]
  $login=Invoke-PartyPublic POST /v1/auth/login $null @{gameId='divine-beasts';provider='password';accountName=$seed.accountName;credential=$seed.password;clientVersion='PartyArena.Development.20261015.1';deviceId=('party-http-'+[guid]::NewGuid().ToString('N'))}
  $taskSessions+=,$login
  $current=Invoke-PartyPublic GET /v1/party/current $login $null
  if(!$current.hasParty){$taskSelected+=,$login}
 }
 $taskSeeds=$null;$taskSeedsText=$null;$seed=$null;$taskIdentity=$null
 if($taskSelected.Count-ne2){throw '没有两名当前无队伍的开发用户，未改现有队伍。'}
 $taskLeaderLogin=$taskSelected[0];$taskInviteeLogin=$taskSelected[1];$taskSteps+='AuthenticatedTwoDevelopmentUsers'
 $initial=Invoke-PartyPublic POST /v1/party/observe $taskInviteeLogin @{cursor='';waitMilliseconds=0}
 $taskHttp=[Net.Http.HttpClient]::new();$taskHttp.Timeout=[TimeSpan]::FromSeconds(5)
 $watch=[Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Post,($GatewayUrl.TrimEnd('/')+'/v1/party/observe'))
 $watch.Headers.Authorization=[Net.Http.Headers.AuthenticationHeaderValue]::new('Bearer',$taskInviteeLogin.accessToken)
 $watch.Content=[Net.Http.StringContent]::new((@{cursor=$initial.cursor;waitMilliseconds=2000}|ConvertTo-Json -Compress),[Text.Encoding]::UTF8,'application/json')
 $waiting=$taskHttp.SendAsync($watch)
 $invitationId=[guid]::NewGuid().ToString('N')
 $invited=Invoke-PartyPublic POST /v1/party/commands $taskLeaderLogin @{partyId='';action='invite';targetPlayerId=$taskInviteeLogin.playerId;membershipRequestId=$invitationId;clientRequestId=([guid]::NewGuid().ToString('N'));expectedRevision='0'}
 $taskPartyId=$invited.affectedPartyId;if(!$taskPartyId){throw '首次邀请没有原子创建队伍。'}
 $response=$waiting.GetAwaiter().GetResult();$response.EnsureSuccessStatusCode()|Out-Null
 $event=($response.Content.ReadAsStringAsync().GetAwaiter().GetResult())|ConvertFrom-Json
 $invitation=@($event.invitations|Where-Object {$_.requestId-eq$invitationId})
 if(!$event.changed-or$invitation.Count-ne1){throw '本人观察没有收到真实邀请。'}
 $taskSteps+='InitialInviteAtomicallyCreatedParty';$taskSteps+='ObservationDeliveredInvitation'
 $accepted=Invoke-PartyPublic POST /v1/party/commands $taskInviteeLogin @{partyId=$taskPartyId;action='acceptRequest';membershipRequestId=$invitationId;clientRequestId=([guid]::NewGuid().ToString('N'));expectedRevision=[string]$invitation[0].revision}
 $partyA=Invoke-PartyPublic GET /v1/party/current $taskLeaderLogin $null;$partyB=Invoke-PartyPublic GET /v1/party/current $taskInviteeLogin $null
 if($partyA.partyId-ne$taskPartyId-or$partyB.partyId-ne$taskPartyId-or@($partyA.members).Count-ne2-or@($partyB.members).Count-ne2){throw '双方快照未汇合为同一两人队伍。'}
 $taskSteps+='BothUsersObservedSameTwoMemberParty'
 $transferred=Invoke-PartyPublic POST /v1/party/commands $taskLeaderLogin @{partyId=$taskPartyId;action='transferLeader';targetPlayerId=$taskInviteeLogin.playerId;clientRequestId=([guid]::NewGuid().ToString('N'));expectedRevision=[string]$partyA.revision}
 $partyB=Invoke-PartyPublic GET /v1/party/current $taskInviteeLogin $null
 if($partyB.leaderPlayerId-ne$taskInviteeLogin.playerId){throw '权威队长转移没有提交。'}
 $taskSteps+='LeadershipTransferredWithExpectedRevision'
 $disbanded=Invoke-PartyPublic POST /v1/party/commands $taskInviteeLogin @{partyId=$taskPartyId;action='disband';clientRequestId=([guid]::NewGuid().ToString('N'));expectedRevision=[string]$partyB.revision}
 foreach($login in $taskSelected){$empty=Invoke-PartyPublic GET /v1/party/current $login $null;if($empty.hasParty){throw '测试队伍未清理。'}}
 $taskSteps+='OwnedTestPartyDisbandedAndMembershipsCleared';$taskPartyId='';$taskPassed=$true
}finally{
 if($taskPartyId){foreach($login in $taskSelected){
  try{$current=Invoke-PartyPublic GET /v1/party/current $login $null
   if($current.partyId-eq$taskPartyId-and$current.leaderPlayerId-eq$login.playerId){Invoke-PartyPublic POST /v1/party/commands $login @{partyId=$taskPartyId;action='disband';clientRequestId=([guid]::NewGuid().ToString('N'));expectedRevision=[string]$current.revision}|Out-Null;break}
  }catch{$taskPassed=$false;Write-Warning '本轮测试队伍清理失败，需检查报告。'}
 }}
 foreach($login in $taskSessions){try{Invoke-PartyPublic POST /v1/auth/logout $null @{refreshToken=$login.refreshToken}|Out-Null}catch{$taskPassed=$false;Write-Warning '本轮测试会话退出失败。'}}
 if($taskHttp){$taskHttp.Dispose()};$taskSessions=$null;$taskSelected=$null;$taskLeaderLogin=$null;$taskInviteeLogin=$null;$login=$null;$taskSeeds=$null;$taskSeedsText=$null;$taskIdentity=$null;$seed=$null
 New-Item -ItemType Directory -Force -Path (Split-Path $taskReportFull)|Out-Null
 @{Passed=$taskPassed;UTC=[DateTime]::UtcNow.ToString('o');Steps=$taskSteps;Scope='Real HTTP Gateway/Identity/Match party commands and observation; no UE gameplay acceptance';CredentialsPersisted=$false}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $taskReportFull -Encoding utf8
}
if(!$taskPassed){exit 1};Write-Output ('LIVE_GATEWAY_PARTY_PASS Steps='+$taskSteps.Count+' Evidence='+$taskReportFull)
