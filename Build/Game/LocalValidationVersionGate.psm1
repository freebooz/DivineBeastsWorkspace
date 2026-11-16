#requires -Version 7.0
# 快速登录前同步重读实际构建、Cook/Stage、Windows 进程及同一 Compose 的控制面。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1')
Import-Module (Join-Path $PSScriptRoot 'FoundationCookEvidence.psm1')
Import-Module (Join-Path $PSScriptRoot 'FoundationTargetAssembly.psm1')
Import-Module (Join-Path $PSScriptRoot 'LocalValidationPackageEvidence.psm1')
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1')

function Read-ValidationProcessReceipt([string]$Path, $Result, $Tool) {
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Foundation process.json 不接受链接。' }
    $process = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable
    if ($process.ProcessId -ne $Result.ProcessId -or ([datetime]$process.StartTimeUtc).ToUniversalTime().Ticks -ne $Result.StartTimeTicks -or
        $process.FilePath -ine $Tool.Executable -or $process.Arguments[0] -ine $Tool.Dll) { throw 'Foundation 实际工具进程记录与结果不一致。' }
    return $process.Arguments
}

function Assert-ValidationBuildPackage($Build, $Package, [string]$Workspace, [string]$Target, [string]$BattleInput) {
    $expectedTarget = "DivineBeastsArena$Target"
    $cook = $Package.Result
    $targets = @($Build.Targets | Where-Object Target -CEQ $expectedTarget)
    if (@($Build.OnlyModules).Count -or @($Build.AdditionalPlugins).Count -or $Build.UsePrecompiled -or
        $Build.Platform -cne 'Win64' -or $Build.Configuration -cne 'Development' -or $targets.Count -ne 1 -or
        $Build.EngineRoot -ine $cook.EngineRoot -or [datetime]$Build.TimestampUtc -gt [datetime]$cook.TimestampUtc) {
        throw "Foundation 证据未证明 $Target 完整构建先于对应 Cook/Stage。"
    }
    $targetBuild = $targets[0]
    if ($targetBuild.Result.ExitCode -ne 0 -or $targetBuild.Result.TimedOut -or -not $targetBuild.Result.ProcessId -or
        [datetime]$Build.TimestampUtc -gt [datetime]::new([long]$cook.Result.StartTimeTicks,[DateTimeKind]::Utc) -or
        $targetBuild.BattleInputSHA256 -cne $BattleInput -or $Package.Proof.BattleInputSHA256 -cne $BattleInput) {
        throw "$Target 构建、Cook/Stage 与当前战斗或复制合同不一致。请完成双端同步构建及烘焙。"
    }
    $project = Join-Path $Workspace 'Game/DivineBeastsArena.uproject'
    $buildDirectory = Join-Path $Workspace "Saved/Validation/FoundationM0/$($Build.RunId)/Build/$Target"
    if ($targetBuild.Evidence -ine $buildDirectory) { throw "$Target 完整构建进程的记录目录不一致。" }
    $engine = Get-FoundationEngine $Build.EngineRoot
    $buildArgs = @(Read-ValidationProcessReceipt (Join-Path $buildDirectory 'process.json') $targetBuild.Result (Get-FoundationManagedTool $engine UnrealBuildTool))
    if ($buildArgs[1] -cne $expectedTarget -or $buildArgs[2] -cne 'Win64' -or $buildArgs[3] -cne 'Development' -or
        $buildArgs -inotcontains "-Project=$project" -or @($buildArgs | Where-Object { $_ -match '^-Module(?:=|$)|^-DisablePlugin(?:=|$)' }).Count) {
        throw "$Target 实际 UBT 参数不符合完整标准目标。"
    }
    $cookArgs = @(Read-ValidationProcessReceipt (Join-Path (Split-Path $cook.StageDirectory) 'process.json') $cook.Result (Get-FoundationManagedTool $engine AutomationTool))
    foreach ($arg in 'BuildCookRun',"-project=$project","-target=$expectedTarget",'-cook','-stage','-pak',"-stagingdirectory=$($cook.StageDirectory)") {
        if ($cookArgs -inotcontains $arg) { throw "$Target 实际 UAT 进程缺少 $arg。" }
    }
    $receipt = Read-FoundationTargetAssembly -Workspace $Workspace -Target $Target
    $bin = Join-Path $Workspace "Game/Binaries/Win64/$expectedTarget.exe"
    if ($targetBuild.TargetAssembly.ReceiptPath -ine $receipt.ReceiptPath -or $cook.TargetAssembly.ReceiptPath -ine $receipt.ReceiptPath -or
        $targetBuild.TargetAssembly.ReceiptSha256 -cne $receipt.ReceiptSha256 -or $cook.TargetAssembly.ReceiptSha256 -cne $receipt.ReceiptSha256 -or
        $targetBuild.ExecutableSHA256 -cne $Package.Proof.ExecutableSHA256 -or
        (Get-FileHash -LiteralPath $bin -Algorithm SHA256).Hash -cne $Package.Proof.ExecutableSHA256) {
        throw "$Target 完整目标收据或构建程序与 Stage 不匹配。"
    }
}

function Get-LocalValidationComposePort {
    param([string]$Workspace,[string]$ComposeProject,[string]$Service,[int]$ContainerPort)
    $ids = @(& docker ps --filter "label=com.docker.compose.project=$ComposeProject" --filter "label=com.docker.compose.service=$Service" --format '{{.ID}}' 2>$null)
    if ($LASTEXITCODE -ne 0 -or $ids.Count -ne 1 -or $ids[0] -cnotmatch '^[a-f0-9]{12,64}$') {
        throw "本地 Compose 服务未唯一运行：$Service"
    }
    try {
        $labels = (& docker inspect --type container --format '{{json .Config.Labels}}' $ids[0] 2>$null) | ConvertFrom-Json -AsHashtable
        if ($LASTEXITCODE -ne 0 -or $labels.'com.docker.compose.project' -cne $ComposeProject -or
            $labels.'com.docker.compose.service' -cne $Service -or
            [IO.Path]::GetFullPath($labels.'com.docker.compose.project.working_dir') -ine $Workspace) { throw 'Compose 工作区或服务身份不匹配' }
        $ports = (& docker inspect --type container --format '{{json .NetworkSettings.Ports}}' $ids[0] 2>$null) | ConvertFrom-Json -AsHashtable
        $binding = @($ports["$ContainerPort/tcp"])
        if ($LASTEXITCODE -ne 0 -or $binding.Count -ne 1 -or $binding[0].HostIp -cne '127.0.0.1' -or
            [int]$binding[0].HostPort -notin 1..65535) { throw '必须发布唯一的 IPv4 回环端口' }
        @{ Id=$ids[0]; Port=[int]$binding[0].HostPort }
    } catch { throw "无法核对本地 Compose 服务身份与端口：$Service" }
}

function Get-ValidationControlInventory([string]$ContainerId, [string]$ControlBaseUrl) {
    $key = 'GAMESERVERCONTROL_INTERNAL_TOKEN'
    $template = '{{range .Config.Env}}{{if eq (index (split . "=") 0) "GAMESERVERCONTROL_INTERNAL_TOKEN"}}{{println .}}{{end}}{{end}}'
    $entries = @(& docker inspect --type container --format $template $ContainerId 2>$null | Where-Object { $_.StartsWith($key + '=') })
    if ($LASTEXITCODE -ne 0 -or $entries.Count -ne 1 -or [string]::IsNullOrWhiteSpace($entries[0].Substring($key.Length + 1))) {
        throw '本地控制面凭据不可用。'
    }
    $headers = @{Authorization='Bearer ' + $entries[0].Substring($key.Length + 1)}
    try {
        $response = Invoke-WebRequest -Uri "$ControlBaseUrl/internal/v1/gameservers/validation-inventory" -Headers $headers `
            -NoProxy -MaximumRedirection 0 -TimeoutSec 5
        $payload = $response.Content | ConvertFrom-Json -AsHashtable
        if ($response.StatusCode -ne 200 -or $payload.instances -isnot [array]) { throw '实例清单格式不符' }
        $payload.instances
    } catch { throw '控制面实例库存查询失败。请核对同一 Compose 的游戏服控制服务版本与就绪状态。' }
    finally { $entries = $null; $headers.Clear() }
}

function Get-ValidationLaunchRecords([string]$Workspace, [guid]$VillageRunId, [guid]$ArenaPoolRunId, [string]$ControlBaseUrl) {
    $villageDirectory = Join-Path $Workspace "Saved/Validation/VillageDevelopment/$VillageRunId"
    $poolDirectory = Join-Path $Workspace "Saved/Validation/MainArenaPool/$ArenaPoolRunId"
    $village = Get-Content -LiteralPath (Join-Path $villageDirectory 'Server.json') -Raw | ConvertFrom-Json -AsHashtable
    $owner = Get-Content -LiteralPath (Join-Path $poolDirectory 'Supervisor.json') -Raw | ConvertFrom-Json -AsHashtable
    # 读取旧或新一份完整快照，允许监督进程同时原子换代，不阻塞 Move。
    $poolStream = [IO.File]::Open((Join-Path $poolDirectory 'Pool.json'),[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $poolReader = [IO.StreamReader]::new($poolStream,[Text.Encoding]::UTF8,$true)
    try { $arena = @($poolReader.ReadToEnd() | ConvertFrom-Json -AsHashtable) } finally { $poolReader.Dispose() }
    if ($village.ServerId -cne "village-local-$($VillageRunId.ToString('N'))" -or $village.ControlBaseUrl -cne $ControlBaseUrl -or
        $owner.ControlBaseUrl -cne $ControlBaseUrl -or $owner.RunId -cne $ArenaPoolRunId.ToString('D') -or
        $null -eq $owner.Count -or $owner.Count -lt 1 -or $arena.Count -ne $owner.Count -or $arena.Count -gt 8 -or
        (Test-Path -LiteralPath (Join-Path $poolDirectory 'Stop.json'))) { throw 'Village 启动记录或 MainArena 池拓扑不完整。' }
    $village.Role = 'Village'
    $village.Log = Join-Path $villageDirectory 'VillageServer.log'
    $indices = [Collections.Generic.HashSet[int]]::new()
    foreach ($slot in $arena) {
        if ($null -eq $slot.Index -or [int]$slot.Index -lt 0 -or [int]$slot.Index -ge $owner.Count -or -not $indices.Add([int]$slot.Index) -or
            [int]$slot.Generation -lt 1 -or [int]$slot.Port -ne ([int]$owner.FirstPort + [int]$slot.Index) -or
            $slot.ServerId -cne "mainarena-local-$($ArenaPoolRunId.ToString('N'))-$($slot.Index)-$($slot.Generation)") {
            throw 'MainArena 池槽位、代次或启动身份不一致。'
        }
        $slot.Role = 'MainArena'
        $slot.Log = Join-Path $poolDirectory "Arena-$($slot.Index)-$($slot.Generation).log"
    }
    return @{ Records=@($village) + $arena; Owner=$owner }
}

function Get-ValidationRunningProcesses($Records, $Owner, [string]$Workspace, [guid]$ArenaPoolRunId) {
    $poolPath = Join-Path $Workspace 'Deploy/Local/RunMainArenaPool.ps1'
    $supervisor = Get-Process -Id ([int]$Owner.PID) -ErrorAction SilentlyContinue
    $supervisorMeta = Get-CimInstance Win32_Process -Filter "ProcessId=$([int]$Owner.PID)"
    $poolCommand = 'RunMainArenaPool\.ps1(?:[\s"]|$)'
    if (-not $supervisor -or $supervisor.HasExited -or $supervisor.StartTime.ToUniversalTime().Ticks.ToString() -cne [string]$Owner.StartUtcTicks -or
        $supervisorMeta.Name -ine 'pwsh.exe' -or $Owner.ScriptPath -ine $poolPath -or $supervisorMeta.CommandLine -notmatch $poolCommand -or
        $supervisorMeta.CommandLine -notmatch [regex]::Escape($ArenaPoolRunId.ToString('D'))) { throw 'MainArena 池监督进程身份或启动时间不一致。' }
    $udp = @(Get-NetUDPEndpoint)
    foreach ($record in $Records) {
        $processId = [int]$record.PID
        $proc = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if (-not $proc -or $proc.HasExited) { throw "$($record.Role) 进程已退出（PID $processId）。" }
        $meta = Get-CimInstance Win32_Process -Filter "ProcessId=$processId"
        $map = Get-LocalValidationWorldMap $record.Role
        foreach ($argument in @("-ServerRole=$($record.Role)",'-CustomConfig=VillageDevelopmentServer',"-port=$($record.Port)",'-MULTIHOME=127.0.0.1',
            '-DBADevelopmentSkills','-DBADevelopmentArena','-textconfig',"-abslog=$($record.Log)",$map)) {
            if ($meta.CommandLine -notmatch ('(?:^|[\s"])' + [regex]::Escape($argument) + '(?:$|[\s"])')) {
                throw "$($record.Role) 进程实际角色或启动参数缺少 $argument（PID $processId）。"
            }
        }
        if ($record.Role -ceq 'MainArena' -and $meta.ParentProcessId -ne $Owner.PID) { throw 'MainArena 进程脱离当前监督池。' }
        $listeners = @($udp | Where-Object LocalPort -EQ $record.Port)
        if (-not $listeners.Count -or @($listeners | Where-Object { $_.OwningProcess -ne $processId -or $_.LocalAddress -cne '127.0.0.1' }).Count) {
            throw "$($record.Role) 进程未独占预期回环 UDP 端口（PID $processId）。"
        }
        $logStream = [IO.File]::Open($record.Log,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $logReader = [IO.StreamReader]::new($logStream,[Text.Encoding]::UTF8,$true)
        try { $log = $logReader.ReadToEnd() } finally { $logReader.Dispose() }
        if ($log -notmatch [regex]::Escape("LoadMap: $map") -or $log -notmatch "listening on port $($record.Port)\b" -or
            $log -match 'NetChecksumMismatch|NetChecksum mismatch|Fatal error:|Failed to resolve.*subobject|Could not find.*replicated') {
            throw "$($record.Role) 没有可用的地图与网络启动证据（PID $processId）。"
        }
        @{ PID=$proc.Id; StartUtcTicks=$proc.StartTime.ToUniversalTime().Ticks.ToString(); Exe=$proc.Path; Role=$record.Role; Port=$record.Port }
    }
}

function Assert-ValidationLiveServers($Records, $Processes, $Instances, [string]$Exe, [string]$BuildVersion) {
    $expected = @{}
    foreach ($record in $Records) {
        if ($expected.ContainsKey($record.ServerId)) { throw '启动记录含重复服务器身份。' }
        $expected[$record.ServerId] = $record
        $tag = "$($record.Role) $($record.ServerId)"
        $running = @($Processes | Where-Object { $_.PID -eq $record.PID })
        if ($running.Count -ne 1) { throw "$tag 进程已退出或不唯一。" }
        $process = $running[0]
        if ([string]$process.StartUtcTicks -cne [string]$record.StartUtcTicks) { throw "$tag 进程启动时间变化（PID $($record.PID)）。" }
        if ($process.Role -cne $record.Role -or $process.Port -ne $record.Port) { throw "$tag 进程角色或端口不一致。" }
        if ($process.Exe -ine $Exe -or $record.Exe -ine $Exe -or $record.BuildVersion -cne $BuildVersion) { throw "$tag 启动制品路径或构建版本不一致。" }
        $registered = @($Instances | Where-Object { $_.gameServerId -ceq $record.ServerId })
        if ($registered.Count -ne 1) { throw "$tag 尚未在当前控制面就绪或注册不唯一。" }
        $instance = $registered[0]
        $boot = [guid]::Empty
        if (-not [guid]::TryParse([string]$record.BootId,[ref]$boot) -or $boot -eq [guid]::Empty -or $instance.serverBootId -cne $record.BootId) {
            throw "$tag 控制面 Boot 与启动记录不符。"
        }
        $experience = if ($record.Role -ceq 'Village') { 'Experience.Village.Tutorial' } else { 'Experience.MainArena.Main' }
        if ($instance.serverRoleId -cne "GameServer.Role.$($record.Role)" -or $instance.experienceId -cne $experience -or
            $instance.publicEndpoint -cne "127.0.0.1:$($record.Port)") { throw "$tag 控制面角色、体验或端点不符。" }
        if ($instance.buildVersion -cne $BuildVersion -or $instance.protocolVersion -ne 1) { throw "$tag 控制面构建版本或协议版本不符。" }
        $age = [datetime]::UtcNow - ([datetime]$instance.heartbeatUtc).ToUniversalTime()
        if ($instance.status -cnotin @('Ready','Active') -or $age.TotalSeconds -gt 30 -or $age.TotalSeconds -lt -5) { throw "$tag 控制面未就绪或心跳过期。" }
    }
    foreach ($instance in $Instances) {
        if (-not $expected.ContainsKey($instance.gameServerId)) { throw "控制面仍有未纳入本次门禁的候选实例：$($instance.gameServerId)（$($instance.serverRoleId)）。" }
    }
}

function Assert-LocalValidationVersionGate {
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)][string]$ClientExe,[Parameter(Mandatory)][uri]$Gateway,
        [Parameter(Mandatory)][string]$ComposeProject,[guid]$ClientBuildRunId,[guid]$ServerBuildRunId,
        [guid]$ClientCookRunId,[guid]$ServerCookRunId,[guid]$VillageRunId,[guid]$ArenaPoolRunId)
    $clientBuild = Read-LocalValidationFoundationResult $Workspace $ClientBuildRunId Build
    $serverBuild = Read-LocalValidationFoundationResult $Workspace $ServerBuildRunId Build
    $client = Read-LocalValidationCookPackage $Workspace $ClientCookRunId Client
    $server = Read-LocalValidationCookPackage $Workspace $ServerCookRunId Server
    if ([IO.Path]::GetFullPath($ClientExe) -ine $client.Executable) { throw '所选客户端程序与本次完成 Cook 的真实 WindowsClient 程序路径不一致。' }
    $clientGate = $client.Result.CookEditorGate
    $editorRunId = [guid]$clientGate.BuildRunId
    $null = Read-LocalValidationFoundationResult $Workspace $editorRunId Build
    $editorPath = Join-Path $Workspace "Saved/Validation/FoundationM0/$editorRunId/Build/result.json"
    $editor = Assert-FoundationCookEditorBuildResult -Workspace $Workspace -EngineRoot $client.Result.EngineRoot -BuildResultPath $editorPath
    foreach ($savedGate in $clientGate,$server.Result.CookEditorGate) {
        foreach ($name in 'BuildRunId','BuildResultSHA256','CookRelevantCompileSHA256','InputFileCount','ArtifactSHA256','Scope') {
            if ($savedGate.$name -cne $editor.$name) { throw "双端 Cook 未绑定当前完整 Editor 证据：$name。" }
        }
    }
    $battleInput = Get-FoundationBattleInputSHA256 -Workspace $Workspace -CookRelevantCompileSHA256 $editor.CookRelevantCompileSHA256
    Assert-ValidationBuildPackage $clientBuild $client $Workspace Client $battleInput
    Assert-ValidationBuildPackage $serverBuild $server $Workspace Server $battleInput
    if ($Gateway.Scheme -cne 'http' -or $Gateway.Host -notin @('127.0.0.1','localhost') -or $Gateway.AbsolutePath -cne '/') {
        throw '本地快速登录要求 Compose 的 IPv4 回环 HTTP 网关根地址。'
    }
    $gatewayPort = Get-LocalValidationComposePort $Workspace $ComposeProject 'gatewayservice' 8080
    if ($gatewayPort.Port -ne $Gateway.Port) { throw '显式网关端口与当前 Compose 不一致。' }
    $control = Get-LocalValidationComposePort $Workspace $ComposeProject 'gameservercontrolservice' 8084
    $controlUrl = "http://127.0.0.1:$($control.Port)"
    foreach ($endpoint in "http://127.0.0.1:$($gatewayPort.Port)",$controlUrl) {
        try {
            if ((Invoke-WebRequest -Uri "$endpoint/health/ready" -NoProxy -MaximumRedirection 0 -TimeoutSec 5).StatusCode -ne 200) { throw '健康检查未返回 200' }
        } catch { throw "本地 Compose 服务未就绪：$endpoint" }
    }
    $launch = Get-ValidationLaunchRecords $Workspace $VillageRunId $ArenaPoolRunId $controlUrl
    $running = @(Get-ValidationRunningProcesses $launch.Records $launch.Owner $Workspace $ArenaPoolRunId)
    $instances = @(Get-ValidationControlInventory $control.Id $controlUrl)
    Assert-ValidationLiveServers $launch.Records $running $instances $server.Executable $server.BuildVersion
    [pscustomobject]@{
        Passed=$true; RecordedAtUtc=[datetime]::UtcNow.ToString('o'); BattleInputSHA256=$battleInput
        ClientExe=$client.Executable; ServerExe=$server.Executable; ClientStageSHA256=$client.Proof.StageSHA256; ServerStageSHA256=$server.Proof.StageSHA256
        BuildRunIds=@{Editor=$editorRunId;Client=$ClientBuildRunId;Server=$ServerBuildRunId}; CookRunIds=@{Client=$ClientCookRunId;Server=$ServerCookRunId}
        VillageRunId=$VillageRunId; ArenaPoolRunId=$ArenaPoolRunId; ComposeProject=$ComposeProject; Gateway=$Gateway.AbsoluteUri
        RunningServers=$launch.Records; ControlInstances=$instances; ControlBaseUrl=$controlUrl; ControlContainerId=$control.Id
        ServerBuildVersion=$server.BuildVersion; RealAdmissionVerified=$false
    }
}

function Assert-LocalValidationLaunchStillCurrent {
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)]$Proof)
    # 账号读取与控制面查询期间可能有池自动换代。子进程创建前复核所有 Boot 和 PID。
    $instances = @(Get-ValidationControlInventory $Proof.ControlContainerId $Proof.ControlBaseUrl)
    $latest = Get-ValidationLaunchRecords $Workspace $Proof.VillageRunId $Proof.ArenaPoolRunId $Proof.ControlBaseUrl
    if ($latest.Records.Count -ne $Proof.RunningServers.Count) { throw '版本核验期间 MainArena 池槽数变化。' }
    $running = foreach ($record in $latest.Records) {
        $verified = @($Proof.RunningServers | Where-Object { $_.ServerId -ceq $record.ServerId -and $_.BootId -ceq $record.BootId -and $_.PID -eq $record.PID })
        $proc = Get-Process -Id ([int]$record.PID) -ErrorAction SilentlyContinue
        if ($verified.Count -ne 1 -or -not $proc -or $proc.HasExited -or $proc.Path -ine $Proof.ServerExe -or
            $proc.StartTime.ToUniversalTime().Ticks.ToString() -cne [string]$verified[0].StartUtcTicks) {
            throw "$($record.Role) 在核验期间换代。请等本池重新就绪后重试。"
        }
        @{ PID=$proc.Id; StartUtcTicks=$proc.StartTime.ToUniversalTime().Ticks.ToString(); Exe=$proc.Path; Role=$record.Role; Port=$record.Port }
    }
    Assert-ValidationLiveServers $latest.Records @($running) $instances $Proof.ServerExe $Proof.ServerBuildVersion
}

Export-ModuleMember -Function Assert-LocalValidationVersionGate,Assert-LocalValidationLaunchStillCurrent
