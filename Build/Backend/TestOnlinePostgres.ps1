#Requires -Version 7.0
<#
只在本轮新隔离数据库中创建两份全新测试schema；运行冻结快照编译的真实PG测试二进制。
Identity schema仅000005；Player schema为000001完整core（含未使用match_results）+000006。
本脚本不清schema/表/卷。领域测试自身仅清理各自随机测试主体；不得fallback到public。
原始测试正文仅在内存处理；结果只保留白名单用例名及PASS/FAIL，SKIP算未执行且整体失败。
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunId)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
$state=Read-OnlineState $RunId
$directory=Get-OnlineRunDirectory $RunId
$names=@('TestOnlineIdentityPGConcurrentRefreshReplayRevokes','TestOnlineIdentityPGLogoutHistoricalTokenAndIsolation',
    'TestOnlineIdentityPGDisabledExpiryAndScope','TestOnlineIdentityPGLogoutRefreshRace',
    'TestOnlinePlayerPGReplayPersistsAndOnlyEditsDisplayName','TestOnlinePlayerPGConcurrentUpdates',
    'TestOnlinePlayerPGKeysAreScopedToSubject','TestOnlinePlayerPGCancelledInsertRollsBackProfile','TestOnlinePlayerPGCancelledKeyWait')
$cases=@($names | ForEach-Object { [pscustomobject]@{Name=$_;Status='NotExecuted'} })
$status='Failed'; $code=1
try {
    if (-not $state.Migrated -or [DateTimeOffset]::UtcNow -ge [DateTimeOffset]::Parse($state.SecretsExpireAt)) { throw '本轮库/秘密期限不符合测试前提。' }
    $pg=Get-OnlineOwnedRecord $state 'postgres'; $network=Get-OnlineOwnedRecord $state 'network'
    $owner=Invoke-OnlineDocker -Arguments @('exec',$pg.Id,'psql','-X','-U','online_owner','-d',$state.Database,'-Atc','SELECT run_id FROM online_integration_owner')
    if ($owner.Output -cne $RunId) { throw '数据库所有权标记不匹配。' }
    $suffix=$RunId.Replace('-','')
    $identitySchema="online_identity_test_$suffix"; $playerSchema="online_player_test_$suffix"
    $migrations=Join-Path $directory 'SourceSnapshot/migrations'
    $sql="BEGIN;`nCREATE SCHEMA $identitySchema;`nSET LOCAL search_path TO $identitySchema;`n"+[IO.File]::ReadAllText((Join-Path $migrations '000005_online_identity.sql'))
    $sql+="`nCREATE SCHEMA $playerSchema;`nSET LOCAL search_path TO $playerSchema;`n"+[IO.File]::ReadAllText((Join-Path $migrations '000001_core.sql'))
    $sql+="`n"+[IO.File]::ReadAllText((Join-Path $migrations '000006_online_profile_idempotency.sql'))+"`nCOMMIT;`n"
    $null=Invoke-OnlineDocker -Arguments @('exec','-i',$pg.Id,'psql','-X','-v','ON_ERROR_STOP=1','-U','online_owner','-d',$state.Database) -InputText $sql
    $secrets=Join-Path $directory 'Secrets'; $databaseEnv=Join-Path $secrets 'database.env'
    Assert-OnlinePrivateFile $databaseEnv
    $envText=[IO.File]::ReadAllText($databaseEnv).Trim()
    if (-not $envText.StartsWith('POSTGRES_DSN=')) { throw '缺少本轮数据库输入。' }
    $dsn=$envText.Substring('POSTGRES_DSN='.Length)
    $testEnv="ONLINE_IDENTITY_TEST_DSN=$dsn&search_path=$identitySchema`nONLINE_IDENTITY_TEST_ISOLATED=1`nONLINE_PLAYER_TEST_DSN=$dsn&search_path=$playerSchema`nONLINE_PLAYER_TEST_ALLOW_WRITES=1`n"
    $testEnvPath=Join-Path $secrets 'pgtests.env'; Write-OnlinePrivateText $testEnvPath $testEnv
    $testEnv=$null; $dsn=$null; $envText=$null
    $binaries=Join-Path $directory 'Binaries'
    $record=Add-OnlineResource $state 'container' 'pgsuite' @('--network',$network.Name,'--env-file',$testEnvPath,
        '--mount',"type=bind,source=$binaries,target=/app,readonly",$state.GoImage,'/app/postgres.test','-test.run','^TestOnline(Identity|Player)PG','-test.count=1','-test.timeout=120s','-test.v')
    try { $run=Invoke-OnlineDocker -Arguments @('start','--attach',$record.Id) -TimeoutSeconds 150 -AllowFailure }
    catch { $null=Get-OnlineOwnedRecord $state 'pgsuite'; $null=Invoke-OnlineDocker -Arguments @('stop','--time','5',$record.Id) -AllowFailure; throw }
    foreach ($case in $cases) {
        $escaped=[regex]::Escape($case.Name)
        if ($run.Output -match "(?m)^--- PASS: $escaped ") { $case.Status='Passed' }
        elseif ($run.Output -match "(?m)^--- FAIL: $escaped ") { $case.Status='Failed' }
    }
    $actual=Get-OnlineResource 'container' $record.Id; Assert-OnlineOwnedResource $state $record $actual
    if ($run.ExitCode -eq 0 -and $actual.ExitCode -eq 0 -and @($cases | Where-Object Status -ne 'Passed').Count -eq 0) { $status='Passed'; $code=0 }
} catch { }
finally {
    $path=Write-OnlineResult $state 'PostgresResult.json' $status $code $cases @('冻结快照postgres.test；两个独立search_path schema；未修改服务public数据。')
    Write-Output "RunId=$RunId Status=$status Cases=$($cases.Count) Result=$path"
}
if ($code -ne 0) { throw '真实PG测试失败或有未执行项；没有输出原始测试诊断。' }
