#Requires -Version 7.0
<#
为本轮新隔离库准备A/B两个真实密码账号。JSON形状与UE锁定一致，密码来自系统CSPRNG。
凭据文件只在仅当前用户ACL目录存在；有效期在run.json，禁止控制台打印/命令行传密码。
工具直接持有两个仓储，但分别调用Identity.EnsureAccount和PlayerData.EnsureProfile所属领域用例。
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunId, [ValidateRange(30,300)][int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
$state=Read-OnlineState $RunId
$directory=Get-OnlineRunDirectory $RunId
$cases=[Collections.Generic.List[object]]::new()
try {
    if (-not $state.Migrated -or [DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($state.SecretsExpireAt)) { throw '未迁移或短期凭据有效期已结束。' }
    $null=Get-OnlineOwnedRecord $state 'postgres'
    $network=Get-OnlineOwnedRecord $state 'network'
    $secrets=Join-Path $directory 'Secrets'
    $credentialPath=Join-Path $secrets 'credentials.json'
    if (Test-Path -LiteralPath $credentialPath) { throw '凭据已存在；禁止覆盖已准备的测试账号口令。' }
    $suffix=$RunId.Replace('-','')
    $credentials=[ordered]@{runId=$RunId;gameId='divine-beasts';accounts=[ordered]@{
        A=[ordered]@{accountName="online_${suffix}_a";password=(New-OnlineSecret);playerId=''}
        B=[ordered]@{accountName="online_${suffix}_b";password=(New-OnlineSecret);playerId=''}
    }}
    Write-OnlinePrivateText $credentialPath ($credentials | ConvertTo-Json -Depth 6)
    $credentials=$null
    $binaries=Join-Path $directory 'Binaries'
    $null=Invoke-OnlineJob $state 'seed' @('--network',$network.Name,'--env-file',(Join-Path $secrets 'database.env'),
        '--mount',"type=bind,source=$binaries,target=/app,readonly",'--mount',"type=bind,source=$secrets,target=/run/online-secrets",
        $state.GoImage,'/app/onlinetestdata','-run-id',$RunId,'-credentials-file','/run/online-secrets/credentials.json') $TimeoutSeconds
    Assert-OnlinePrivateFile $credentialPath
    $prepared=Get-Content -LiteralPath $credentialPath -Raw | ConvertFrom-Json
    if ($prepared.runId -cne $RunId -or $prepared.gameId -cne 'divine-beasts' -or
        [string]::IsNullOrWhiteSpace($prepared.accounts.A.playerId) -or [string]::IsNullOrWhiteSpace($prepared.accounts.B.playerId) -or
        $prepared.accounts.A.playerId -eq $prepared.accounts.B.playerId) { throw '所属领域初始化未产生两个不同的真实玩家身份。' }
    $prepared=$null
    $cases.Add(@{Name='TwoRunOwnedPersistentAccounts';Status='Passed'})
    $cases.Add(@{Name='ProfileInitializationViaOwningService';Status='Passed'})
    $path=Write-OnlineResult $state 'PrepareResult.json' 'Passed' 0 $cases.ToArray() @('凭据仅存受限Secrets目录，不复制进结果JSON。')
    Write-Output "RunId=$RunId Status=Passed Result=$path CredentialsFile=$credentialPath"
} catch {
    $cases.Add(@{Name='PrepareAccounts';Status='Failed'})
    $path=Write-OnlineResult $state 'PrepareResult.json' 'Failed' 1 $cases.ToArray()
    throw "本轮账号准备失败，保留数据及受限文件用于核查，不打印凭据。RunId=$RunId Result=$path"
}
