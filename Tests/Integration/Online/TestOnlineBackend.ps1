#Requires -Version 7.0
<#
真实三服务+PostgreSQL验收。不使用Mock，不创建后门，不打印HTTP正文、密码或令牌。
必须先成功StartOnlineIntegration和PrepareOnlineTestData；会修改本轮A账号显示名并重启本轮三个服务。
结果逐项标Passed/Failed/NotExecuted，任何未完成步骤不能因已有通过数量被判整体成功。
此脚本不是UE验收；不删除数据库记录/卷，账号B用于验证不同主体隔离。
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunId)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $workspace 'Build/Backend/OnlineIntegrationTools.psm1') -Force
$state=Read-OnlineState $RunId
$directory=Get-OnlineRunDirectory $RunId
$credentialPath=Join-Path $directory 'Secrets/credentials.json'
$caseNames=@('OwnedEnvironment','ProbeCompatibility','LoginAccountA','LoginAccountB','ReadProfileA','ReadProfileB',
    'BadPasswordRejected','MissingAuthenticationRejected','UpdateDisplayName','IdempotencySameBodyReplay',
    'IdempotencyDifferentBodyRejected','StaleRevisionRejected','NonWhitelistFieldRejected','AccountIsolation',
    'RefreshRotation','LogoutConfirmed','OldAccessRejectedAfterLogout','OldRefreshRejectedAfterLogout',
    'OwnedServiceRestart','LoginSameIdentityAfterRestart','ProfilePersistsAfterRestart','AccountBAfterRestart')
$cases=@($caseNames | ForEach-Object { [pscustomobject]@{Name=$_;Status='NotExecuted'} })
$script:activeCase=$null
function Begin-Case([string]$Name) { $script:activeCase=$Name }
function Complete-Case { ($cases | Where-Object Name -eq $script:activeCase).Status='Passed' }
function Assert-Condition([bool]$Condition) { if (-not $Condition) { throw '真实响应不满足本用例契约。' } }
$handler=[Net.Http.HttpClientHandler]::new(); $handler.AllowAutoRedirect=$false; $handler.UseProxy=$false
$client=[Net.Http.HttpClient]::new($handler); $client.Timeout=[TimeSpan]::FromSeconds(12); $client.MaxResponseContentBufferSize=1048576
function Send-Request([string]$Method,[string]$Relative,$Body=$null,[string]$AccessToken='',[string]$Key='') {
    if ($Relative -notmatch '^/v1/' -or $Relative.Contains('://')) { throw '只允许固定回环网关相对路径。' }
    $request=[Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::new($Method),"http://127.0.0.1:$($state.GatewayPort)$Relative")
    try {
        $request.Headers.Add('X-Request-ID',"$RunId-$([Guid]::NewGuid().ToString('N'))")
        if ($AccessToken) { $request.Headers.Authorization=[Net.Http.Headers.AuthenticationHeaderValue]::new('Bearer',$AccessToken) }
        if ($Key) { $request.Headers.Add('Idempotency-Key',$Key) }
        if ($null -ne $Body) { $request.Content=[Net.Http.StringContent]::new(($Body | ConvertTo-Json -Depth 6 -Compress),[Text.Encoding]::UTF8,'application/json') }
        $response=$client.SendAsync($request).GetAwaiter().GetResult()
        try {
            $status=[int]$response.StatusCode
            $text=$response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
            if ($status -eq 204) { Assert-Condition ([string]::IsNullOrWhiteSpace($text)); return [pscustomobject]@{Status=$status;Body=$null} }
            Assert-Condition ($response.Content.Headers.ContentType.MediaType -eq 'application/json')
            $bodyResult=$text | ConvertFrom-Json -Depth 12
            return [pscustomobject]@{Status=$status;Body=$bodyResult}
        } finally { $response.Dispose() }
    } finally { $request.Dispose() }
}
function Login-Account($Account) {
    $response=Send-Request 'POST' '/v1/auth/login' @{gameId='divine-beasts';provider='password';accountName=$Account.accountName;credential=$Account.password;clientVersion='1.0.0';deviceId=$RunId}
    Assert-Condition ($response.Status -eq 200 -and $response.Body.playerId -ceq $Account.playerId)
    Assert-Condition (-not [string]::IsNullOrWhiteSpace($response.Body.accessToken) -and -not [string]::IsNullOrWhiteSpace($response.Body.refreshToken))
    Assert-Condition ([DateTimeOffset]::Parse($response.Body.expiresAt) -gt [DateTimeOffset]::UtcNow)
    Assert-Condition ([DateTimeOffset]::Parse($response.Body.refreshExpiresAt) -ge [DateTimeOffset]::Parse($response.Body.expiresAt))
    return $response.Body
}
$status='Failed'; $exitCode=1
try {
    Begin-Case 'OwnedEnvironment'
    foreach ($role in @('postgres','identity','player','gateway')) { $null=Get-OnlineOwnedRecord $state $role }
    Assert-Condition ($state.Migrated -and [DateTimeOffset]::UtcNow -lt [DateTimeOffset]::Parse($state.SecretsExpireAt))
    Assert-OnlinePrivateFile $credentialPath
    $credentials=Get-Content -LiteralPath $credentialPath -Raw | ConvertFrom-Json
    Assert-Condition ($credentials.runId -ceq $RunId -and $credentials.gameId -ceq 'divine-beasts')
    Assert-Condition ($credentials.accounts.A.playerId -cne $credentials.accounts.B.playerId)
    Complete-Case
    Begin-Case 'ProbeCompatibility'; Wait-OnlineProbe $state 20; Complete-Case
    Begin-Case 'LoginAccountA'; $a=Login-Account $credentials.accounts.A; Complete-Case
    Begin-Case 'LoginAccountB'; $b=Login-Account $credentials.accounts.B; Complete-Case
    Begin-Case 'ReadProfileA'
    $profileA=Send-Request 'GET' '/v1/player/profile' $null $a.accessToken
    Assert-Condition ($profileA.Status -eq 200 -and $profileA.Body.playerId -ceq $a.playerId); Complete-Case
    Begin-Case 'ReadProfileB'
    $profileB=Send-Request 'GET' '/v1/player/profile' $null $b.accessToken
    Assert-Condition ($profileB.Status -eq 200 -and $profileB.Body.playerId -ceq $b.playerId); Complete-Case
    Begin-Case 'BadPasswordRejected'
    $wrong=Send-Request 'POST' '/v1/auth/login' @{gameId='divine-beasts';provider='password';accountName=$credentials.accounts.A.accountName;credential=(New-OnlineSecret);clientVersion='1.0.0'}
    Assert-Condition ($wrong.Status -eq 401); Complete-Case
    Begin-Case 'MissingAuthenticationRejected'
    $missing=Send-Request 'GET' '/v1/player/profile'; Assert-Condition ($missing.Status -eq 401); Complete-Case
    $expectedName='Online-'+[Guid]::NewGuid().ToString('N').Substring(0,12)
    $update=@{displayName=$expectedName;expectedRevision=[long]$profileA.Body.revision}
    $key="$RunId-$([Guid]::NewGuid().ToString('N'))"
    Begin-Case 'UpdateDisplayName'
    $updated=Send-Request 'PATCH' '/v1/player/profile' $update $a.accessToken $key
    Assert-Condition ($updated.Status -eq 200 -and $updated.Body.displayName -ceq $expectedName -and [long]$updated.Body.revision -eq ([long]$profileA.Body.revision+1)); Complete-Case
    Begin-Case 'IdempotencySameBodyReplay'
    $replay=Send-Request 'PATCH' '/v1/player/profile' $update $a.accessToken $key
    Assert-Condition ($replay.Status -eq 200 -and $replay.Body.revision -eq $updated.Body.revision -and $replay.Body.displayName -ceq $expectedName); Complete-Case
    Begin-Case 'IdempotencyDifferentBodyRejected'
    $different=Send-Request 'PATCH' '/v1/player/profile' @{displayName='DifferentBody';expectedRevision=$update.expectedRevision} $a.accessToken $key
    Assert-Condition ($different.Status -eq 409); Complete-Case
    Begin-Case 'StaleRevisionRejected'
    $stale=Send-Request 'PATCH' '/v1/player/profile' $update $a.accessToken "$key-stale"
    Assert-Condition ($stale.Status -eq 409); Complete-Case
    Begin-Case 'NonWhitelistFieldRejected'
    $forbidden=Send-Request 'PATCH' '/v1/player/profile' @{displayName='Forbidden';expectedRevision=$updated.Body.revision;playerId=$b.playerId} $a.accessToken "$key-other"
    Assert-Condition ($forbidden.Status -eq 400); Complete-Case
    Begin-Case 'AccountIsolation'
    $stillB=Send-Request 'GET' '/v1/player/profile' $null $b.accessToken
    Assert-Condition ($stillB.Status -eq 200 -and $stillB.Body.playerId -ceq $b.playerId -and $stillB.Body.revision -eq $profileB.Body.revision -and $stillB.Body.displayName -ceq $profileB.Body.displayName); Complete-Case
    Begin-Case 'RefreshRotation'
    $fresh=Send-Request 'POST' '/v1/auth/refresh' @{refreshToken=$a.refreshToken}
    Assert-Condition ($fresh.Status -eq 200 -and $fresh.Body.playerId -ceq $a.playerId -and $fresh.Body.refreshToken -cne $a.refreshToken -and $fresh.Body.accessToken -cne $a.accessToken)
    $freshProfile=Send-Request 'GET' '/v1/player/profile' $null $fresh.Body.accessToken
    Assert-Condition ($freshProfile.Status -eq 200); Complete-Case
    Begin-Case 'LogoutConfirmed'
    $logout=Send-Request 'POST' '/v1/auth/logout' @{refreshToken=$fresh.Body.refreshToken}
    Assert-Condition ($logout.Status -eq 204); Complete-Case
    Begin-Case 'OldAccessRejectedAfterLogout'
    foreach ($token in @($a.accessToken,$fresh.Body.accessToken)) {
        $rejected=Send-Request 'GET' '/v1/player/profile' $null $token; Assert-Condition ($rejected.Status -eq 401)
    }; Complete-Case
    Begin-Case 'OldRefreshRejectedAfterLogout'
    foreach ($token in @($a.refreshToken,$fresh.Body.refreshToken)) {
        $rejected=Send-Request 'POST' '/v1/auth/refresh' @{refreshToken=$token}; Assert-Condition ($rejected.Status -eq 401)
    }; Complete-Case
    Begin-Case 'OwnedServiceRestart'
    & (Join-Path $workspace 'Build/Backend/RestartOnlineIntegration.ps1') -RunId $RunId | Out-Null
    Complete-Case
    Begin-Case 'LoginSameIdentityAfterRestart'; $after=Login-Account $credentials.accounts.A; Complete-Case
    Begin-Case 'ProfilePersistsAfterRestart'
    $persisted=Send-Request 'GET' '/v1/player/profile' $null $after.accessToken
    Assert-Condition ($persisted.Status -eq 200 -and $persisted.Body.revision -eq $updated.Body.revision -and $persisted.Body.displayName -ceq $expectedName); Complete-Case
    Begin-Case 'AccountBAfterRestart'
    $afterB=Login-Account $credentials.accounts.B
    $persistedB=Send-Request 'GET' '/v1/player/profile' $null $afterB.accessToken
    Assert-Condition ($persistedB.Status -eq 200 -and $persistedB.Body.revision -eq $profileB.Body.revision -and $persistedB.Body.displayName -ceq $profileB.Body.displayName); Complete-Case
    $status='Passed'; $exitCode=0
} catch {
    if ($script:activeCase) { ($cases | Where-Object Name -eq $script:activeCase).Status='Failed' }
} finally {
    $credentials=$null; $a=$null; $b=$null; $fresh=$null; $after=$null; $afterB=$null; $token=$null
    $client.Dispose(); $handler.Dispose()
    $path=Write-OnlineResult $state 'BackendResult.json' $status $exitCode $cases @(
        (Join-Path $directory 'StartResult.json'),(Join-Path $directory 'PrepareResult.json'),(Join-Path $directory 'RestartResult.json'),
        "TestScriptSHA256=$((Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash)")
    Write-Output "RunId=$RunId Status=$status Cases=$($cases.Count) Result=$path"
}
if ($exitCode -ne 0) { throw "真实后端验收失败：$script:activeCase；后续用例保持NotExecuted，响应与秘密未写入证据。" }
