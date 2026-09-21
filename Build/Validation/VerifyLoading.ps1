#requires -Version 5.1
<#
.SYNOPSIS
执行Loading原生算法或指定UE目标，记录本轮证据；未执行的游戏验收始终保留。
.DESCRIPTION
支持Windows PowerShell 5.1；使用独占证据目录和进程级超时。仅终止本次创建且启动身份一致的进程树。
没有完整UE运行/Cook证据时即使所选检查成功也返回2；外部失败码原样传播。
#>
param([switch]$NativeTests, [ValidateSet('Editor','Client','Server')][string[]]$Targets = @(),
    [string]$EngineRoot = $env:UE_ROOT, [ValidateRange(1,7200)][int]$TimeoutSeconds = 180,
    [string]$CMake = 'cmake', [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence = Join-Path $root "Saved/Validation/GamePlatformLoading/$RunId"
if (Test-Path -LiteralPath $evidence) { throw '拒绝覆盖已有证据目录' }
$null = New-Item -ItemType Directory -Path $evidence
$results = [Collections.Generic.List[object]]::new()
function Invoke-LoadingCheck([string]$Name,[string]$Executable,[string[]]$Arguments,[string]$WorkingDirectory) {
    $directory = Join-Path $evidence $Name
    $null = New-Item -ItemType Directory -Path $directory
    # Windows CreateProcess参数转义；不经过cmd或字符串拼接的Shell执行。
    $quoted = @($Arguments | ForEach-Object { '"' + ([regex]::Replace([regex]::Replace($_,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')) + '"' })
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable; $start.Arguments = $quoted -join ' '; $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    $stdout = [IO.File]::Create((Join-Path $directory 'stdout.log'))
    $stderr = [IO.File]::Create((Join-Path $directory 'stderr.log'))
    $started = $false; $timedOut = $false; $code = 1
    try {
        $started = $process.Start(); $ticks = $process.StartTime.ToUniversalTime().Ticks
        @{Executable=$Executable;Arguments=$Arguments;ProcessId=$process.Id;StartTimeTicks=$ticks} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $directory 'process.json') -Encoding UTF8
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $timedOut = $true
            if (-not $process.HasExited -and $process.StartTime.ToUniversalTime().Ticks -eq $ticks) {
                & "$env:SystemRoot/System32/taskkill.exe" /PID $process.Id /T /F | Out-Null
                $null = $process.WaitForExit(5000)
            }
            $code = 124
        } else { $code = $process.ExitCode }
        $null = [Threading.Tasks.Task]::WaitAll(@($outTask,$errTask),3000)
    } finally {
        if ($started -and -not $process.HasExited -and $process.StartTime.ToUniversalTime().Ticks -eq $ticks) {
            & "$env:SystemRoot/System32/taskkill.exe" /PID $process.Id /T /F | Out-Null
        }
        $stdout.Dispose(); $stderr.Dispose(); $process.Dispose()
    }
    $record = @{Name=$Name;ExitCode=$code;TimedOut=$timedOut;Evidence=$directory;Status=$(if($code -eq 0){'通过'}else{'失败'})}
    $results.Add($record)
    $record | ConvertTo-Json | Set-Content (Join-Path $directory 'result.json') -Encoding UTF8
    if ($code -ne 0) { throw "ExternalExitCode=$code" }
}
$exitCode = 2; $message = '所选检查完成不等于完整UE、PIE及Cook验收；详见未执行项。'
try {
    if ($NativeTests) {
        $cmakePath = (Get-Command $CMake -ErrorAction Stop).Source
        $native = Join-Path $evidence 'Native'
        Invoke-LoadingCheck 'Configure' $cmakePath @('-S',(Join-Path $root 'Game/Plugins/GameFoundation/Application/GamePlatformLoading/Tests'),'-B',$native) $root
        foreach ($config in @('Debug','Release')) {
            Invoke-LoadingCheck "Build-$config" $cmakePath @('--build',$native,'--config',$config) $root
            Invoke-LoadingCheck "Test-$config" (Join-Path (Split-Path $cmakePath) 'ctest.exe') @('--test-dir',$native,'-C',$config,'--output-on-failure','-V') $root
        }
    }
    if ($Targets.Count) {
        if (-not $EngineRoot) { throw 'UE_ROOT或EngineRoot未指定' }
        $engine = (Resolve-Path -LiteralPath $EngineRoot).Path
        $version = Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
        if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) { throw '不是锁定的UE5.8，不降低版本' }
        $project = Join-Path $root 'Game/DivineBeastsArena.uproject'
        $null = Get-Content $project -Raw -Encoding UTF8 | ConvertFrom-Json
        $ubt = Join-Path $engine 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'
        $dotnet = Join-Path $engine 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe'
        if (-not (Test-Path $ubt) -or -not (Test-Path $dotnet)) { throw '缺少本项目当前UBT及锁定.NET 10运行时' }
        foreach ($target in $Targets) {
            Invoke-LoadingCheck "UE-$target" $dotnet @($ubt,"DivineBeastsArena$target",'Win64','Development',"-Project=$project",'-MaxParallelActions=2','-NoSharedPCH','-NoHotReloadFromIDE',"-Log=$(Join-Path $evidence "UBT-$target.log")") (Join-Path $engine 'Engine/Source')
        }
    }
} catch {
    $message = $_.Exception.Message
    $exitCode = if ($message -match 'ExternalExitCode=(\d+)') { [int]$Matches[1] } else { 1 }
} finally {
    @{Date=[DateTime]::UtcNow.ToString('O');ExitCode=$exitCode;Message=$message;Checks=@($results.ToArray());NotExecuted=@('FoundationLoadingOnly真实运行','FoundationSessionLoading前置阻塞','双PIE','Client/Server干净Cook及产物审计','人工签审')} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $evidence 'result.json') -Encoding UTF8
    Write-Host "Loading evidence: $evidence"
}
exit $exitCode
