#Requires -Version 7.0
<#
.SYNOPSIS
启动独立三服务+PostgreSQL联调，不接管现有容器或数据库。
.DESCRIPTION
默认仅显示计划；必须-Execute -ConfirmNewIsolatedDatabase才创建资源。RunId为小写D格式GUID。
输出Saved/Validation/GamePlatformOnline/<RunId>/run.json和StartResult.json；秘密仅Secrets受限目录。
000001完整core迁移会同时创建未使用的match_results表；绝不执行000003或其他未列出的迁移。
构建仅在本轮临时容器内下载go.mod锁定依赖，不修改宿主go.mod/go.sum/generated。
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunId,
    [ValidateRange(1024,65535)][int]$GatewayPort=28081,
    [ValidateRange(30,600)][int]$StartupTimeoutSeconds=120,
    [ValidateRange(60,3600)][int]$BuildTimeoutSeconds=1200,
    [ValidateRange(1,24)][int]$CredentialLifetimeHours=2,
    [string]$GoImage='golang@sha256:60deed95d3888cc5e4d9ff8a10c54e5edc008c6ae3fba6187be6fb592e19e8c0',
    [string]$PostgresImage='postgres@sha256:742f40ea20b9ff2ff31db5458d127452988a2164df9e17441e191f3b72252193',
    [switch]$Execute,
    [switch]$ConfirmNewIsolatedDatabase
)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
Assert-OnlineRunId $RunId
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$directory=Get-OnlineRunDirectory $RunId
$migrationNames=@('000001_core.sql','000005_online_identity.sql','000006_online_profile_idempotency.sql')
if (-not $Execute) {
    [pscustomobject]@{ RunId=$RunId; Status='NotExecuted'; Gateway="http://127.0.0.1:$GatewayPort"; NamePrefix="dba-online-$RunId-";
        Roles=@('postgres','identity','player','gateway'); PostgreSQLPublished=$false; Database=('online_'+$RunId.Replace('-',''));
        Migrations=$migrationNames; OutputDirectory=$directory; SecretsDirectory=(Join-Path $directory 'Secrets'); RequiresNewDatabaseConfirmation=$true }
    return
}
if (-not $ConfirmNewIsolatedDatabase) { throw '必须显式确认仅创建本轮新隔离数据库。' }
if (Test-Path -LiteralPath $directory) { throw 'RunId输出目录已存在；禁止用启动脚本重跑迁移或接管已有环境，请使用重启脚本。' }
foreach ($name in $migrationNames) { if (-not (Test-Path -LiteralPath (Join-Path $workspace "Backend/migrations/$name"))) { throw "锁定迁移尚未写入：$name" } }
foreach ($image in @($GoImage,$PostgresImage)) { $null=Invoke-OnlineDocker -Arguments @('image','inspect','--format','{{.Id}}',$image) }
# Windows监听表可能不反映Docker转发，同时检查Docker发布信息并实际探测回环绑定能力。
$ports=(Invoke-OnlineDocker -Arguments @('ps','--format','{{.Ports}}')).Output
if ($ports -match ":$GatewayPort->") { throw '计划网关端口已被Docker发布。' }
$listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,$GatewayPort)
try { $listener.Start() } finally { $listener.Stop() }
[void][IO.Directory]::CreateDirectory($directory)
$secrets=Join-Path $directory 'Secrets'; Protect-OnlineDirectory $secrets
$state=[pscustomobject]@{
    RunId=$RunId; OwnerToken=[Guid]::NewGuid().ToString('N'); Workspace=$workspace; GatewayPort=$GatewayPort
    Database=('online_'+$RunId.Replace('-','')); GoImage=$GoImage; PostgresImage=$PostgresImage; Resources=@()
    SourceFingerprint='NotBuilt'; Migrated=$false; SecretsExpireAt=[DateTimeOffset]::UtcNow.AddHours($CredentialLifetimeHours).ToString('o')
    Migrations=@(); Status='Starting'; CreatedAt=[DateTimeOffset]::UtcNow.ToString('o')
}
Save-OnlineState $state
$cases=[Collections.Generic.List[object]]::new()
try {
    # 只复制源码扩展名，禁止把工作区环境文件/凭据带入普通构建证据；构建使用冻结快照。
    $snapshot=Join-Path $directory 'SourceSnapshot'; [void][IO.Directory]::CreateDirectory($snapshot)
    $backend=Join-Path $workspace 'Backend'
    $files=@(Get-ChildItem $backend -File | Where-Object Name -in @('go.mod','go.sum'))
    foreach ($folder in @('cmd','internal','generated','migrations')) {
        $files+=@(Get-ChildItem (Join-Path $backend $folder) -Recurse -File | Where-Object Extension -in @('.go','.sql','.proto'))
    }
    $hashLines=[Collections.Generic.List[string]]::new()
    foreach ($file in ($files | Sort-Object FullName)) {
        $relative=[IO.Path]::GetRelativePath($backend,$file.FullName)
        $target=Assert-OnlineChildPath $snapshot (Join-Path $snapshot $relative)
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        $before=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        Copy-Item -LiteralPath $file.FullName -Destination $target
        $copied=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($before -ne $copied -or $before -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) { throw '源码在复制期间变化；禁止混合快照构建。' }
        $hashLines.Add("$copied  $($relative.Replace('\','/'))")
    }
    $hashFile=Join-Path $directory 'SourceSnapshot.sha256'
    [IO.File]::WriteAllLines($hashFile,$hashLines,[Text.UTF8Encoding]::new($false))
    $state.SourceFingerprint=(Get-FileHash -LiteralPath $hashFile -Algorithm SHA256).Hash
    Save-OnlineState $state
    $binaries=Join-Path $directory 'Binaries'; [void][IO.Directory]::CreateDirectory($binaries)
    $buildCommand=@'
set -eu
cp -R /source /tmp/backend
cd /tmp/backend
go version > /output/GoVersion.txt
go mod download
go test -mod=mod -tags=productiondeps ./internal/tools/onlinetestdata
for service in gatewayservice identityservice playerdataservice; do
  CGO_ENABLED=0 go build -mod=mod -trimpath -tags=productiondeps,grpcdeps -o "/output/$service" "./cmd/$service"
done
CGO_ENABLED=0 go build -mod=mod -trimpath -tags=productiondeps -o /output/onlinetestdata ./internal/tools/onlinetestdata
cp go.mod /output/Build.go.mod
cp go.sum /output/Build.go.sum
'@
    $null=Invoke-OnlineJob $state 'build' @('--mount',"type=bind,source=$snapshot,target=/source,readonly",'--mount',"type=bind,source=$binaries,target=/output",
        '--env','GOTOOLCHAIN=local',$GoImage,'sh','-c',$buildCommand) $BuildTimeoutSeconds
    $cases.Add(@{Name='BuildFrozenSource';Status='Passed'})
    $network=Add-OnlineResource $state 'network' 'network' @('--internal')
    $volume=Add-OnlineResource $state 'volume' 'dbdata' @()
    $password=New-OnlineSecret
    Write-OnlinePrivateText (Join-Path $secrets 'postgres-password') $password
    $pgName="dba-online-$RunId-postgres"
    $dsn="postgres://online_owner:$password@$($pgName):5432/$($state.Database)?sslmode=disable"
    Write-OnlinePrivateText (Join-Path $secrets 'database.env') "POSTGRES_DSN=$dsn`n"
    $password=$null; $dsn=$null
    $pg=Add-OnlineResource $state 'container' 'postgres' @('--network',$network.Name,
        '--mount',"type=volume,source=$($volume.Name),target=/var/lib/postgresql/data",
        '--mount',"type=bind,source=$secrets,target=/run/online-secrets,readonly",'--env',"POSTGRES_DB=$($state.Database)",
        '--env','POSTGRES_USER=online_owner','--env','POSTGRES_PASSWORD_FILE=/run/online-secrets/postgres-password',$PostgresImage)
    $null=Invoke-OnlineDocker -Arguments @('start',$pg.Id)
    $deadline=[DateTimeOffset]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    do {
        $ready=Invoke-OnlineDocker -Arguments @('exec',$pg.Id,'pg_isready','-U','online_owner','-d',$state.Database) -AllowFailure
        if ($ready.ExitCode -eq 0) { break }; Start-Sleep -Milliseconds 500
    } while ([DateTimeOffset]::UtcNow -lt $deadline)
    if ($ready.ExitCode -ne 0) { throw '本轮PostgreSQL未就绪。' }
    $null=Get-OnlineOwnedRecord $state 'postgres'
    $tableCount=Invoke-OnlineDocker -Arguments @('exec',$pg.Id,'psql','-X','-U','online_owner','-d',$state.Database,'-Atc',"SELECT count(*) FROM pg_tables WHERE schemaname='public'")
    if ($tableCount.Output -ne '0') { throw '新隔离库不是空库，拒绝执行迁移。' }
    $sql="BEGIN;`nCREATE TABLE online_integration_owner (run_id text PRIMARY KEY);`nINSERT INTO online_integration_owner VALUES ('$RunId');`nCREATE TABLE online_integration_migrations (name text PRIMARY KEY, sha256 text NOT NULL);`n"
    foreach ($name in $migrationNames) {
        $path=Join-Path $snapshot "migrations/$name"
        $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        $sql+=[IO.File]::ReadAllText($path)+"`nINSERT INTO online_integration_migrations VALUES ('$name','$hash');`n"
        $state.Migrations+=@{Name=$name;SHA256=$hash}
    }
    $sql+="COMMIT;`n"
    $null=Invoke-OnlineDocker -Arguments @('exec','-i',$pg.Id,'psql','-X','-v','ON_ERROR_STOP=1','-U','online_owner','-d',$state.Database) -InputText $sql
    $state.Migrated=$true; Save-OnlineState $state
    $cases.Add(@{Name='OwnedNewDatabaseSelectedMigrations';Status='Passed'; Detail='000001完整core含未使用match_results；未执行Session/Outbox迁移'})
    foreach ($role in @('identity','player','gateway')) {
        $service=switch ($role) { 'identity' {'identityservice'} 'player' {'playerdataservice'} 'gateway' {'gatewayservice'} }
        $httpPort=switch ($role) { 'identity' {8081} 'player' {8082} 'gateway' {8080} }
        $grpcPort=$httpPort+1000
        $arguments=@('--network',$network.Name,'--mount',"type=bind,source=$binaries,target=/app,readonly",'--env',"SERVICE_NAME=$service",'--env',"SERVICE_PORT=$httpPort",'--env',"GRPC_PORT=$grpcPort",'--env',"SERVICE_VERSION=$RunId",'--env','SERVICE_BIND_ADDRESS=0.0.0.0','--env','SHUTDOWN_TIMEOUT=15s')
        if ($role -eq 'gateway') {
            $arguments+=@('--publish',"127.0.0.1:$($GatewayPort):8080",'--env','ONLINE_ONLY=true','--env',"IDENTITY_GRPC_TARGET=dba-online-$RunId-identity:9081",'--env',"PLAYER_DATA_GRPC_TARGET=dba-online-$RunId-player:9082")
        } else { $arguments+=@('--env-file',(Join-Path $secrets 'database.env')) }
        $arguments+=@($GoImage,"/app/$service")
        $record=Add-OnlineResource $state 'container' $role $arguments
        $null=Invoke-OnlineDocker -Arguments @('start',$record.Id)
    }
    Wait-OnlineProbe $state $StartupTimeoutSeconds
    $cases.Add(@{Name='ThreeServicesRealGRPCProbe';Status='Passed'})
    $state.Status='Running'; Save-OnlineState $state
    $path=Write-OnlineResult $state 'StartResult.json' 'Passed' 0 $cases.ToArray() @($hashFile,(Join-Path $binaries 'GoVersion.txt'))
    Write-Output "RunId=$RunId Status=Passed Result=$path Gateway=http://127.0.0.1:$GatewayPort"
} catch {
    $state.Status='Failed'; Save-OnlineState $state
    $cases.Add(@{Name='StartOrBuild';Status='Failed';Detail='步骤失败；未输出可能含秘密的外部异常正文。'})
    $path=Write-OnlineResult $state 'StartResult.json' 'Failed' 1 $cases.ToArray()
    throw "RunId=$RunId 启动失败；已登记资源和PG卷保留，使用StopOnlineIntegration按清单处理。Result=$path"
}
