<#
.SYNOPSIS
在本次独占PostgreSQL容器验证准入事务；不连接既有数据库，不启动或冒充UE。
.DESCRIPTION
临时库只允许容器网络命名空间中的回环连接，采用显式测试trust认证。
只有本脚本创建成功的容器会在finally删除；匿名数据卷只属于该容器。
证据目录包含真实命令输出；源码只读挂载，依赖解析在容器临时副本进行。
#>
[CmdletBinding()]
param([string]$GoImage = 'golang:1.23', [string]$PostgresImage = 'postgres:17-alpine')
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$runId = [guid]::NewGuid().ToString('N')
$evidence = Join-Path $workspace "Saved/Validation/GamePlatformSession/Backend-$runId"
$null = New-Item -ItemType Directory -Path $evidence
$containerName = "divinebeasts-session-test-$runId"
$ownedContainer = $false
$resultCode = 1
$logPath = Join-Path $evidence 'backend.log'
function Invoke-SessionDocker {
    param([string[]]$Arguments)
    $preference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & docker @Arguments 2>&1 | Tee-Object -FilePath $logPath -Append | Out-Host
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $preference }
    if ($code -ne 0) { throw "docker exit code: $code" }
}
function Wait-SessionDatabase {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt 45) {
        & docker exec $containerName pg_isready -U postgres -d session_integration *> $null
        if ($LASTEXITCODE -eq 0) { return }
        Start-Sleep -Milliseconds 250
    }
    throw 'Isolated PostgreSQL readiness timeout'
}
try {
    if (-not (Test-Path -LiteralPath (Join-Path $workspace 'AGENTS.md'))) { throw 'Workspace not found' }
    Invoke-SessionDocker @('run','--rm','-v',"${workspace}:/workspace:ro",'-v',"${evidence}:/evidence",$GoImage,'sh','-c',
        'cp -a /workspace/Backend /tmp/backend && cd /tmp/backend && go version && go test -race -mod=mod -tags=sessionintegration -c -o /evidence/session.test ./internal/platform/database/postgresadmission')
    Invoke-SessionDocker @('run','--detach','--name',$containerName,'--network','none','-e','POSTGRES_HOST_AUTH_METHOD=trust','-e','POSTGRES_DB=session_integration',$PostgresImage)
    $ownedContainer = $true
    Wait-SessionDatabase
    $testArguments = @('run','--rm','--network',"container:$containerName",'-e','SESSION_INTEGRATION_ISOLATED=1','-v',"${workspace}:/workspace:ro",'-v',"${evidence}:/evidence:ro",$GoImage,'/evidence/session.test','-test.v','-test.timeout=60s')
    Invoke-SessionDocker ($testArguments + @('-test.run=^TestMigrate$'))
    Invoke-SessionDocker ($testArguments + @('-test.run=^Test(AtomicClaimAndCommit|CapacityAndIdentity|MigrationFenceAndExpiry|PersistencePrepare)$'))
    Invoke-SessionDocker @('restart','--timeout','10',$containerName)
    Wait-SessionDatabase
    Invoke-SessionDocker ($testArguments + @('-test.run=^TestPersistenceAfterRestart$'))
    $resultCode = 0
} catch {
    $_.Exception.Message | Tee-Object -FilePath $logPath -Append | Out-Host
} finally {
    if ($ownedContainer) {
        try { Invoke-SessionDocker @('rm','--force','--volumes',$containerName) }
        catch { $resultCode = 1; $_.Exception.Message | Out-Host }
    }
    [pscustomobject]@{ RunId=$runId; ExitCode=$resultCode; GoImage=$GoImage; PostgresImage=$PostgresImage; Evidence=$logPath; Scope='PostgreSQL kernel only; no UE or authenticated HTTP integration' } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
}
Write-Output "Session backend evidence: $evidence"
exit $resultCode
