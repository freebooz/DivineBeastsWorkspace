#requires -Version 5.1
<#
.SYNOPSIS
分别执行PCG原生测试、锁定UE三目标及文档存在性检查；不替代生成、重开或Cook。
.DESCRIPTION
每次使用新证据目录，子进程有超时。只终止本次启动且身份一致的进程树，不扫描或关闭用户编辑器。
选择检查全部成功时仍返回2表示完整UE交付未验收；外部失败优先返回其实际退出码。
#>
param([switch]$NativeTests, [ValidateSet('Editor','Client','Server')][string[]]$Targets = @(),
    [string]$EngineRoot = $env:UE_ROOT, [string]$CMake = 'cmake',
    [ValidateRange(1,7200)][int]$TimeoutSeconds = 180, [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$plugin = Join-Path $root 'Game/Plugins/GameFoundation/Gameplay/GamePlatformPCG'
$evidence = Join-Path $root "Saved/Validation/GamePlatformPCG/$RunId"
if (Test-Path -LiteralPath $evidence) { throw '拒绝覆盖已有证据' }
$null = New-Item -ItemType Directory -Path $evidence
$records = [Collections.Generic.List[object]]::new()
function Invoke-PCGCheck([string]$Name,[string]$Executable,[string[]]$Arguments,[string]$WorkingDirectory) {
    $directory = Join-Path $evidence $Name
    $null = New-Item -ItemType Directory -Path $directory
    $quoted = @($Arguments | ForEach-Object { '"' + ([regex]::Replace([regex]::Replace($_,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')) + '"' })
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$Executable; $start.Arguments=$quoted -join ' '; $start.WorkingDirectory=$WorkingDirectory
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$start
    $stdout=[IO.File]::Create((Join-Path $directory 'stdout.log'))
    $stderr=[IO.File]::Create((Join-Path $directory 'stderr.log'))
    $started=$false; $timedOut=$false; $code=1
    try {
        $started=$process.Start(); $ticks=$process.StartTime.ToUniversalTime().Ticks
        @{Executable=$Executable;Arguments=$Arguments;ProcessId=$process.Id;StartTimeTicks=$ticks} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $directory 'process.json') -Encoding UTF8
        $outTask=$process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask=$process.StandardError.BaseStream.CopyToAsync($stderr)
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $timedOut=$true; $code=124
            if (-not $process.HasExited -and $process.StartTime.ToUniversalTime().Ticks -eq $ticks) {
                & "$env:SystemRoot/System32/taskkill.exe" /PID $process.Id /T /F | Out-Null
                $null=$process.WaitForExit(5000)
            }
        } else { $code=$process.ExitCode }
        $null=[Threading.Tasks.Task]::WaitAll(@($outTask,$errTask),3000)
    } finally {
        if ($started -and -not $process.HasExited -and $process.StartTime.ToUniversalTime().Ticks -eq $ticks) {
            & "$env:SystemRoot/System32/taskkill.exe" /PID $process.Id /T /F | Out-Null
        }
        $stdout.Dispose(); $stderr.Dispose(); $process.Dispose()
    }
    $record=@{Name=$Name;ExitCode=$code;TimedOut=$timedOut;Status=$(if($code -eq 0){'通过'}else{'失败'});Evidence=$directory}
    $records.Add($record)
    $record | ConvertTo-Json | Set-Content (Join-Path $directory 'result.json') -Encoding UTF8
    return $code
}
$exitCode=2; $message='分项检查不代表完整UE验收。'
try {
    $docs=@('README.md','Docs/Architecture.md','Docs/API.md','Docs/GenerationProfiles.md','Docs/GraphAuthoring.md','Docs/LifecycleAndCleanup.md','Docs/AuthorityAndConsistency.md','Docs/WorldAndLoadingIntegration.md','Docs/BackendIntegration.md','Docs/ConfigurationAndRun.md','Docs/TestingAndEvidence.md','Docs/Troubleshooting.md','Docs/MigrationAndHandover.md','Docs/ManualReview.md')
    $missing=@($docs | Where-Object { -not (Test-Path -LiteralPath (Join-Path $plugin $_)) })
    $records.Add(@{Name='DocumentPresenceOnly';ExitCode=$(if($missing.Count){1}else{0});Status=$(if($missing.Count){'失败'}else{'通过'});Missing=$missing})
    if ($NativeTests) {
        $cmakePath=(Get-Command $CMake -ErrorAction Stop).Source
        $native=Join-Path $evidence 'Native'
        $configured=Invoke-PCGCheck 'Configure' $cmakePath @('-S',(Join-Path $plugin 'Tests'),'-B',$native) $root
        if ($configured -eq 0) {
            foreach ($config in @('Debug','Release')) {
                $built=Invoke-PCGCheck "Build-$config" $cmakePath @('--build',$native,'--config',$config) $root
                if ($built -eq 0) { $null=Invoke-PCGCheck "Test-$config" (Join-Path (Split-Path $cmakePath) 'ctest.exe') @('--test-dir',$native,'-C',$config,'--output-on-failure','-V') $root }
            }
        }
    }
    if ($Targets.Count) {
        if (-not $EngineRoot) { throw '请设置UE_ROOT或EngineRoot，不推测个人引擎路径' }
        $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
        $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
        if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) { throw '仅接受锁定UE5.8，不降低版本' }
        $version | ConvertTo-Json | Set-Content (Join-Path $evidence 'engine-version.json') -Encoding UTF8
        $project=Join-Path $root 'Game/DivineBeastsArena.uproject'
        $ubt=Join-Path $engine 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'
        $dotnet=Join-Path $engine 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe'
        foreach ($target in $Targets) {
            $null=Invoke-PCGCheck "UE-$target" $dotnet @($ubt,"DivineBeastsArena$target",'Win64','Development',"-Project=$project",'-MaxParallelActions=2','-NoSharedPCH','-NoHotReloadFromIDE',"-Log=$(Join-Path $evidence "UBT-$target.log")") (Join-Path $engine 'Engine/Source')
        }
    }
    $failed=@($records.ToArray() | Where-Object { $_.ExitCode -ne 0 })
    if ($failed.Count) { $exitCode=[int]$failed[0].ExitCode }
} catch { $exitCode=1; $message=$_.Exception.Message }
finally {
    $head=& git -C $root rev-parse HEAD
    @{Date=[DateTime]::UtcNow.ToString('O');Head=$head;ExitCode=$exitCode;Message=$message;Checks=@($records.ToArray());NotExecuted=@('真实PCG生成与取消','资产保存及独立进程重开','双PIE隔离','Client与Server干净Cook及碰撞探针','真实Session网络链','人工签审')} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $evidence 'result.json') -Encoding UTF8
    Write-Host "PCG evidence: $evidence"
}
exit $exitCode
