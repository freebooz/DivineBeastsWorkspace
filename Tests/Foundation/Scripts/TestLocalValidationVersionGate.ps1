#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$launcher = Join-Path $root 'Build/Game/StartLocalTestClients.ps1'
$invalidRun = [guid]::NewGuid()
$launch = @{
    ClientExe = (Join-Path $root 'Saved/NoSuchQuickLoginClient.exe'); GatewayUrl = 'http://127.0.0.1:28181'
    ClientBuildRunId = $invalidRun; ServerBuildRunId = $invalidRun
    ClientCookRunId = $invalidRun; ServerCookRunId = $invalidRun
    VillageRunId = $invalidRun; ArenaPoolRunId = $invalidRun
}
$script:dockerCalled = $false
function docker { $script:dockerCalled = $true; throw 'Docker must remain untouched when build evidence is missing.' }
try {
    & $launcher @launch
    throw 'Launcher accepted missing build evidence.'
} catch {
    if ($dockerCalled -or $_.Exception.Message -notlike '*Foundation*证据*') { throw }
}

$gate = Import-Module (Join-Path $root 'Build/Game/LocalValidationVersionGate.psm1') -Force -PassThru
$fixture = & $gate {
    $exe = 'E:\test\DivineBeastsArenaServer.exe'
    $package = '1.0.0-validation-' + ('A' * 64)
    $villageRun = [guid]::NewGuid()
    $poolRun = [guid]::NewGuid()
    $records = @(
        @{ Role='Village'; PID=40; StartUtcTicks='100001'; Port=18782; Exe=$exe; BuildVersion=$package;
           ServerId="village-local-$($villageRun.ToString('N'))"; BootId=[guid]::NewGuid().ToString('D') },
        @{ Role='MainArena'; PID=42; StartUtcTicks='100002'; Port=18881; Exe=$exe; BuildVersion=$package; Index=0; Generation=1;
           ServerId="mainarena-local-$($poolRun.ToString('N'))-0-1"; BootId=[guid]::NewGuid().ToString('D') },
        @{ Role='MainArena'; PID=43; StartUtcTicks='100003'; Port=18882; Exe=$exe; BuildVersion=$package; Index=1; Generation=2;
           ServerId="mainarena-local-$($poolRun.ToString('N'))-1-2"; BootId=[guid]::NewGuid().ToString('D') }
    )
    $instances = foreach ($record in $records) {
        @{ gameServerId=$record.ServerId; serverBootId=$record.BootId; serverRoleId="GameServer.Role.$($record.Role)"
           experienceId=if ($record.Role -eq 'Village') { 'Experience.Village.Tutorial' } else { 'Experience.MainArena.Main' }
           publicEndpoint="127.0.0.1:$($record.Port)"; buildVersion=$package; protocolVersion=1; status='Ready'; heartbeatUtc=[datetime]::UtcNow }
    }
    $processes = foreach ($record in $records) { @{ PID=$record.PID; StartUtcTicks=$record.StartUtcTicks; Exe=$exe; Role=$record.Role; Port=$record.Port } }
    @{ Records=$records; Processes=@($processes); Instances=@($instances); BuildVersion=$package; Exe=$exe }
}
function Test-Fixture([scriptblock]$Change, [string]$Expected) {
    $test = $fixture | ConvertTo-Json -Depth 10 | ConvertFrom-Json -AsHashtable
    if ($Change) { & $Change $test }
    try {
        & $gate { param($test) Assert-ValidationLiveServers -Records $test.Records -Processes $test.Processes -Instances $test.Instances -Exe $test.Exe -BuildVersion $test.BuildVersion } $test
    } catch {
        if ($Expected -and $_.Exception.Message -like $Expected) { return }
        throw
    }
    if ($Expected) { throw "Expected version gate rejection $Expected." }
}
Test-Fixture $null ''
Test-Fixture { param($data) $data.Instances[2].buildVersion = 'older-package' } '*构建版本*'
Test-Fixture { param($data) $data.Processes = @($data.Processes[0],$data.Processes[1]) } '*进程已退出*'
Test-Fixture { param($data) $data.Processes[2].StartUtcTicks = '100009' } '*启动时间*'
Test-Fixture { param($data) $data.Processes[1].Role = 'Village' } '*角色*'
Test-Fixture { param($data) $data.Instances[1].status = 'Starting' } '*未就绪*'
Test-Fixture { param($data) $data.Instances[1].serverBootId = [guid]::NewGuid().ToString('D') } '*Boot*'
Test-Fixture { param($data) $extra = $data.Instances[2].Clone(); $extra.gameServerId = 'old-arena'; $data.Instances += $extra } '*未纳入*'

Import-Module (Join-Path $root 'Build/Game/LocalValidationPackageEvidence.psm1')
$fixtureParent = Join-Path $root 'Saved/Validation/LocalValidationVersionGateFixtures'
$workspace = Join-Path $fixtureParent ([guid]::NewGuid().ToString('D'))
$cookId = [guid]::NewGuid()
$cookDirectory = Join-Path $workspace "Saved/Validation/FoundationM0/$cookId/Cook-Server"
$stage = Join-Path $cookDirectory 'Stage'
function Write-Fixture([string]$Path, [string]$Value) {
    $null = New-Item -ItemType Directory -Path (Split-Path $Path) -Force
    [IO.File]::WriteAllText($Path,$Value)
}
try {
    $profiles=@{}
    foreach ($role in 'Village','MainArena') {
        $relative = "Deploy/Server/$role/server-profile.json"
        $path = Join-Path $workspace $relative
        Write-Fixture $path (Get-Content -LiteralPath (Join-Path $root $relative) -Raw)
        $profiles[$role]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    $exe = Join-Path $stage 'WindowsServer/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaServer.exe'
    Write-Fixture $exe 'synthetic executable, must never start'
    Write-Fixture (Join-Path $stage 'WindowsServer/Manifest_UFSFiles_Win64.txt') 'synthetic manifest'
    $pak = Join-Path $stage 'WindowsServer/DivineBeastsArena/Content/Paks/DivineBeastsArena-WindowsServer'
    foreach ($ext in 'pak','utoc','ucas') { Write-Fixture "$pak.$ext" "synthetic $ext" }
    $products = Get-LocalValidationStageHash -StageDirectory $stage -Target Server
    $record = @{
        Operation='Cook-Server';RunId=$cookId.ToString('D');Status='Passed';ExitCode=0;EngineVersion='5.8.0';Workspace=$workspace
        Target='Server';StageDirectory=$stage;Executable=$exe;CustomConfig='VillageDevelopmentServer';TargetAssembly=@{Assembly='Default'}
        Result=@{ExitCode=0;TimedOut=$false;ProcessId=123};LocalValidationPackage=@{
            BattleInputSHA256=('B'*64);StageSHA256=$products.StageSHA256;ExecutableSHA256=$products.ExecutableSHA256;RoleProfiles=$profiles
            Maps=@('/DBAWorldPack_Village/Maps/L_Village_Start','/DBAWorldPack_MainArena/Maps/L_DBA_MainArena')
        }
    }
    Write-Fixture (Join-Path $cookDirectory 'result.json') ($record | ConvertTo-Json -Depth 12)
    $package = Read-LocalValidationCookPackage $workspace $cookId Server
    if ($package.Executable -ine $exe -or $package.BuildVersion -notmatch '^1\.0\.0-validation-[A-F0-9]{64}$') { throw 'A coherent Stage was not accepted.' }
    [IO.File]::WriteAllText("$pak.ucas",'replaced container with same executable')
    try {
        $null = Read-LocalValidationCookPackage $workspace $cookId Server
        throw 'Mismatched stage was accepted with unmodified exe.'
    } catch { if ($_.Exception.Message -notlike '*Stage*SHA256*') { throw } }
} finally {
    if (Test-Path -LiteralPath $workspace) {
        $resolved = (Resolve-Path -LiteralPath $workspace).Path
        if ($resolved.StartsWith($fixtureParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}
'LOCAL_VALIDATION_VERSION_GATE_PASS'
