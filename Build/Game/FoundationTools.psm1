#requires -Version 7.0
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function New-FoundationContext {
    <# 创建本次操作独占证据目录；相同RunId和操作不能覆盖旧证据。根路径只从模块位置解析。 #>
    param([string]$Operation, [guid]$RunId)
    if ($RunId -eq [guid]::Empty) { throw 'RunId不能为零GUID。' }
    $workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $directory = Join-Path $workspace "Saved/Validation/FoundationM0/$($RunId.ToString('D'))/$Operation"
    if (Test-Path -LiteralPath $directory) { throw "拒绝复用证据目录：$directory" }
    $null = New-Item -ItemType Directory -Path $directory
    [pscustomobject]@{ Workspace=$workspace; Project=(Join-Path $workspace 'Game/DivineBeastsArena.uproject'); Directory=$directory; RunId=$RunId.ToString('D'); Operation=$Operation }
}

function Stop-FoundationOwnedProcess {
    <# 只终止调用者刚创建且PID与启动时间仍匹配的进程；不按名称杀进程，不递归杀树。返回是否已结束。 #>
    param([Diagnostics.Process]$Process, [long]$StartTimeTicks)
    if ($Process.HasExited) { return $true }
    $current = Get-Process -Id $Process.Id -ErrorAction SilentlyContinue
    if ($null -eq $current) { return $true }
    try {
        if ($current.StartTime.ToUniversalTime().Ticks -ne $StartTimeTicks) { return $false }
        $current.Kill()
        return $current.WaitForExit(5000)
    } catch [InvalidOperationException] {
        return $Process.HasExited
    } catch [ComponentModel.Win32Exception] {
        return $Process.HasExited
    } finally { $current.Dispose() }
}

function Invoke-FoundationProcess {
    <# 使用ArgumentList逐参数传递；标准输出与错误异步落盘。超时/取消均清理本次PID。
       ReadyLog必须为本次新文件；只接受完整日志行的匹配代次。就绪后保留短暂观察窗，防止立即崩溃被误判。
       返回真实ExitCode、PID、启动时间、超时和就绪状态；工具非零码不改写。 #>
    param([Parameter(Mandatory)][string]$FilePath, [string[]]$Arguments = @(),
        [Parameter(Mandatory)][string]$WorkingDirectory, [Parameter(Mandatory)][string]$OutputDirectory,
        [ValidateRange(1,86400)][int]$TimeoutSeconds = 120, [string]$ReadyLog, [string]$ReadyMarker,
        [hashtable]$Environment = @{})
    if ($ReadyMarker -and (Test-Path -LiteralPath $ReadyLog)) { throw '拒绝使用已有就绪日志。' }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $FilePath
    $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    foreach ($key in $Environment.Keys) { $start.Environment[$key] = [string]$Environment[$key] }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $stdout = $null; $stderr = $null; $started = $false; $ticks = 0L
    $exitCode = 1; $ready = $false; $timedOut = $false; $cleaned = $true
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $readyAt = $null
    try {
        $stdout = [IO.File]::Create((Join-Path $OutputDirectory 'stdout.log'))
        $stderr = [IO.File]::Create((Join-Path $OutputDirectory 'stderr.log'))
        $started = $process.Start()
        $ticks = $process.StartTime.ToUniversalTime().Ticks
        $identity = @{ ProcessId=$process.Id; StartTimeUtc=$process.StartTime.ToUniversalTime().ToString('O'); FilePath=$FilePath; Arguments=$Arguments }
        $identity | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'process.json') -Encoding utf8
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        while ($true) {
            if ($process.HasExited) {
                $exitCode = $process.ExitCode
                if ($exitCode -eq 0 -and $ReadyMarker) { $exitCode = 1 }
                break
            }
            if ($ReadyMarker -and (Test-Path -LiteralPath $ReadyLog)) {
                # UE仍持有写句柄；ReadAllText的默认共享模式会拒绝打开活跃日志。
                $logStream = [IO.File]::Open($ReadyLog,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                $reader = [IO.StreamReader]::new($logStream,[Text.Encoding]::UTF8,$true)
                try { $content = $reader.ReadToEnd() } finally { $reader.Dispose() }
                $pattern = '(?m)^(?:\[[^\r\n]*?\])*(?:Log[A-Za-z0-9_]+:[ \t]*(?:(?:Display|Warning|Verbose):[ \t]*)?)?' + [regex]::Escape($ReadyMarker) + '(?:[ \t]+[A-Za-z][A-Za-z0-9_]*=[^\r\n]*)?[ \t]*\r?$'
                if ($content -match $pattern) {
                    if ($null -eq $readyAt) { $readyAt = $watch.Elapsed.TotalSeconds }
                    if (($watch.Elapsed.TotalSeconds - $readyAt) -ge 0.5) { $ready = $true; $exitCode = 0; break }
                }
            }
            if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { $timedOut = $true; $exitCode = 1; break }
            Start-Sleep -Milliseconds 50
        }
    } finally {
        if ($started) {
            $cleaned = Stop-FoundationOwnedProcess -Process $process -StartTimeTicks $ticks
            if (-not $cleaned) { $exitCode = 1 }
            # 不无限等待继承了管道的外部子进程；本函数不把未知后代视为已授权PID。
            if ($null -ne (Get-Variable outTask -ErrorAction SilentlyContinue)) { $null = [Threading.Tasks.Task]::WaitAll(@($outTask,$errTask),3000) }
        }
        if ($null -ne $stdout) { $stdout.Dispose() }
        if ($null -ne $stderr) { $stderr.Dispose() }
        $processId = if ($started) { $process.Id } else { 0 }
        $process.Dispose()
    }
    [pscustomobject]@{ ExitCode=$exitCode; ProcessId=$processId; StartTimeTicks=$ticks; Ready=$ready; TimedOut=$timedOut; Cleaned=$cleaned; DurationSeconds=$watch.Elapsed.TotalSeconds }
}

function Assert-FoundationFile {
    <# 缺失或空白前置材料归未执行2，避免调用工具后误报编译失败。 #>
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -eq 0) {
        throw [IO.FileNotFoundException]::new("缺失或空白前置文件：$Path")
    }
}

function Get-FoundationEngine {
    <# EngineRoot优先，其次UE_ROOT；只接受UE5.8，不搜索个人磁盘或静默降级。 #>
    param([string]$EngineRoot)
    if (-not $EngineRoot) { $EngineRoot = $env:UE_ROOT }
    if (-not $EngineRoot) { throw [IO.FileNotFoundException]::new('请传入-EngineRoot或设置UE_ROOT。') }
    $root = [IO.Path]::GetFullPath($EngineRoot)
    $versionPath = Join-Path $root 'Engine/Build/Build.version'
    Assert-FoundationFile $versionPath
    $version = Get-Content -Raw -LiteralPath $versionPath | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) { throw [IO.FileNotFoundException]::new('需要UE5.8；未执行任何构建。') }
    [pscustomobject]@{ Root=$root; Version="$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)" }
}

function Get-FoundationManagedTool {
    <# 直接调用引擎自带dotnet与真实UBT/UAT，避免cmd转义破坏中文/空格参数。
       工具必须已编译；本入口不暗中编译引擎工具。SDK版本从该工具runtimeconfig读取。 #>
    param($Engine, [ValidateSet('UnrealBuildTool','AutomationTool')][string]$Name)
    $dll = Join-Path $Engine.Root "Engine/Binaries/DotNET/$Name/$Name.dll"
    Assert-FoundationFile $dll
    $configPath = [IO.Path]::ChangeExtension($dll,'runtimeconfig.json')
    Assert-FoundationFile $configPath
    $config = Get-Content -Raw $configPath | ConvertFrom-Json
    # UBT使用framework单对象，UAT使用frameworks数组（含WindowsDesktop），两者均取.NETCore版本。
    $frameworks = if ($config.runtimeOptions.PSObject.Properties['framework']) { @($config.runtimeOptions.framework) }
        elseif ($config.runtimeOptions.PSObject.Properties['frameworks']) { @($config.runtimeOptions.frameworks) }
        else { @() }
    $runtime = @($frameworks | Where-Object name -eq 'Microsoft.NETCore.App')
    if ($runtime.Count -ne 1) { throw [IO.FileNotFoundException]::new("无法确定工具的.NETCore版本：$configPath") }
    $major = ([version]$runtime[0].version).Major
    $architecture = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq 'Arm64') { 'win-arm64' } else { 'win-x64' }
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $Engine.Root 'Engine/Binaries/ThirdParty/DotNet') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "^$major\." } | Sort-Object { [version]$_.Name } -Descending)
    foreach ($candidate in $candidates) {
        $dotnet = Join-Path $candidate.FullName "$architecture/dotnet.exe"
        if (Test-Path -LiteralPath $dotnet) {
            return [pscustomobject]@{ Executable=$dotnet; Dll=$dll; Environment=@{ DOTNET_ROOT=(Split-Path $dotnet); DOTNET_MULTILEVEL_LOOKUP='0'; DOTNET_ROLL_FORWARD='LatestMajor'; PATH=((Split-Path $dotnet) + [IO.Path]::PathSeparator + $env:PATH) } }
        }
    }
    throw [IO.FileNotFoundException]::new("找不到引擎自带.NET $major ($architecture)。")
}

function Enter-FoundationEngineLock {
    <# 同一引擎的本组脚本串行化；不等待其他长构建，不夺取其进程。调用者finally释放。 #>
    param([string]$EngineRoot)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($EngineRoot.ToLowerInvariant()))).Replace('-','') }
    finally { $hasher.Dispose() }
    $mutex = [Threading.Mutex]::new($false,"Local\FoundationM0-$hash")
    try { $acquired = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $acquired = $true }
    if (-not $acquired) { $mutex.Dispose(); throw [IO.FileNotFoundException]::new('另一个Foundation构建或烘焙正在使用此引擎。') }
    return $mutex
}

function Get-FoundationMaps {
    <# 开发地图是显式合同，不从目录扫描任意内容加入Cook。 #>
    @('/Game/Development/Foundation/Maps/L_FoundationBootstrap','/Game/Development/Foundation/Maps/L_FoundationSandbox')
}

function Assert-FoundationMaps {
    <# 只接受开发目录长包名，拒绝URL、旅行选项与路径逃逸；UE负责真实包加载验证。 #>
    param($Context, [string[]]$Maps)
    if (-not $Maps -or $Maps.Count -eq 0) { throw '必须指定至少一张开发地图。' }
    foreach ($map in $Maps) {
        if ($map -cnotmatch '^/Game/Development/Foundation/Maps/[A-Za-z0-9_]+$') { throw "非法Foundation开发地图：$map" }
        Assert-FoundationFile (Join-Path $Context.Workspace ('Game/Content/' + $map.Substring(6) + '.umap'))
    }
}

function Write-FoundationResult {
    <# 每操作一个机器可读结果，记录真实退出码；没有执行必须显式NotExecuted/Incomplete。 #>
    param($Context, [int]$ExitCode, [string]$Message, [hashtable]$Details = @{}, [string]$Status)
    if (-not $Status) { $Status = if ($ExitCode -eq 0) { 'Passed' } elseif ($ExitCode -eq 2) { 'NotExecuted' } else { 'Failed' } }
    $report = [ordered]@{ Operation=$Context.Operation; RunId=$Context.RunId; Status=$Status; ExitCode=$ExitCode; Message=$Message; TimestampUtc=[DateTime]::UtcNow.ToString('O'); Workspace=$Context.Workspace }
    foreach ($key in $Details.Keys) { $report[$key] = $Details[$key] }
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Context.Directory 'result.json') -Encoding utf8
    Write-Host "$($Context.Operation): $Status (exit $ExitCode) $Message"
    Write-Host "Evidence: $($Context.Directory)"
}

Export-ModuleMember -Function New-FoundationContext,Stop-FoundationOwnedProcess,Invoke-FoundationProcess,Assert-FoundationFile,Get-FoundationEngine,Get-FoundationManagedTool,Enter-FoundationEngineLock,Get-FoundationMaps,Assert-FoundationMaps,Write-FoundationResult
