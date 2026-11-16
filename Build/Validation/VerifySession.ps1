<#
.SYNOPSIS
验证Session已落地的独立内核并报告缺失前置；退出2表示完整插件联调未完成。
.PARAMETER NativeTests
实际编译并运行Session状态内核的Debug/Release原生测试。
.PARAMETER BackendTests
调用隔离PostgreSQL测试入口，实际重启本次数据库。不是已认证HTTP或UE联调。
#>
[CmdletBinding()]
param(
    [switch]$NativeTests,
    [switch]$BackendTests,
    [string]$EngineRoot = $env:UE_ROOT
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$runId = [guid]::NewGuid().ToString('N')
$evidence = Join-Path $workspace "Saved/Validation/GamePlatformSession/Verify-$runId"
$null = New-Item -ItemType Directory -Path $evidence
$checks = [Collections.Generic.List[object]]::new()
$exitCode = 2
try {
    $plugin = Join-Path $workspace 'Game/Plugins/GamePlatform/OnlineServices/GamePlatformSession'
    $descriptor = Get-Content -Raw -Encoding UTF8 (Join-Path $plugin 'GamePlatformSession.uplugin') | ConvertFrom-Json
    if ($descriptor.Modules[0].TargetAllowList -contains 'Server' -or $descriptor.Modules.Count -ne 1) { throw 'Session target boundary invalid' }
    $checks.Add(@{Name='DescriptorBoundary';Status='Passed';Evidence='One module; Client and Editor only; static check'})
    if ($NativeTests) {
        # 原生内核测试不依赖已运行的UE Editor；优先使用PATH，缺失时只读定位锁定UE5.8自带CMake。
        # 显式使用绝对可执行文件路径，不修改全局PATH/系统环境/引擎目录，也不调用下载器。
        $cmakeCommand = Get-Command 'cmake.exe' -ErrorAction SilentlyContinue
        $ctestCommand = Get-Command 'ctest.exe' -ErrorAction SilentlyContinue
        $cmakeExe = if ($cmakeCommand) { $cmakeCommand.Source } else { '' }
        $ctestExe = if ($ctestCommand) { $ctestCommand.Source } else { '' }
        if (!$cmakeExe -or !$ctestExe) {
            $roots = @($EngineRoot, 'D:/UnrealEngine-5.8.0-release') | Where-Object { $_ }
            foreach ($root in $roots) {
                $bin = Join-Path $root 'Engine/Extras/ThirdPartyNotUE/CMake/bin'
                $candidateCMake = Join-Path $bin 'cmake.exe'
                $candidateCTest = Join-Path $bin 'ctest.exe'
                if ((Test-Path -LiteralPath $candidateCMake -PathType Leaf) -and
                    (Test-Path -LiteralPath $candidateCTest -PathType Leaf)) {
                    if (!$cmakeExe) { $cmakeExe = $candidateCMake }
                    if (!$ctestExe) { $ctestExe = $candidateCTest }
                    break
                }
            }
        }
        if (!$cmakeExe -or !$ctestExe) {
            throw 'Native CMake/CTest不存在；请指定锁定的UE5.8 EngineRoot，不得把未运行的测试记为通过。'
        }
        $output = Join-Path $evidence 'Native'
        & $cmakeExe -S (Join-Path $plugin 'Tests') -B $output -G 'Visual Studio 17 2022' -A x64 *> (Join-Path $evidence 'configure.log')
        if ($LASTEXITCODE -ne 0) { throw 'Native configure failed' }
        foreach ($configuration in @('Debug','Release')) {
            & $cmakeExe --build $output --config $configuration *> (Join-Path $evidence "build-$configuration.log")
            if ($LASTEXITCODE -ne 0) { throw "Native $configuration build failed" }
            & $ctestExe --test-dir $output -C $configuration --output-on-failure *> (Join-Path $evidence "test-$configuration.log")
            if ($LASTEXITCODE -ne 0) { throw "Native $configuration test failed" }
            $checks.Add(@{Name="Native-$configuration";Status='Passed';Evidence="test-$configuration.log"})
        }
    } else { $checks.Add(@{Name='Native';Status='NotExecuted';Evidence='NativeTests not selected'}) }
    if ($BackendTests) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $workspace 'Tests/Integration/Session/TestSessionBackend.ps1') *> (Join-Path $evidence 'backend.log')
        if ($LASTEXITCODE -ne 0) { throw 'PostgreSQL admission tests failed' }
        $checks.Add(@{Name='PostgresKernel';Status='Passed';Evidence='backend.log'})
    } else { $checks.Add(@{Name='PostgresKernel';Status='NotExecuted';Evidence='BackendTests not selected'}) }
    foreach ($name in @('OnlineAuthenticationIntegration','UETravelAndProtectedHandshake','UEEditorClientServerBuild','UERealDualServerIntegration','CleanCook','HumanReview')) {
        $checks.Add(@{Name=$name;Status='NotVerified';Evidence='See plugin Docs/DeliveryStatus.md; no evidence adapter for this stage'})
    }
} catch {
    $exitCode = 1
    $checks.Add(@{Name='Execution';Status='Failed';Evidence=$_.Exception.Message})
} finally {
    [pscustomobject]@{RunId=$runId;ExitCode=$exitCode;Checks=$checks} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
}
Write-Output "Session verification: $evidence; exit=$exitCode (2 means incomplete integration)"
exit $exitCode
