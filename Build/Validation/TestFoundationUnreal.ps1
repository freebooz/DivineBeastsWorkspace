#requires -Version 7.0
<#
.SYNOPSIS
在正式工程运行Core/Data/Flow反射自动化并核对本次导出，不自动构建、不修改资产或插件。
.DESCRIPTION
必须显式-Execute；EngineRoot未指定时使用UE_ROOT。所有目标需要先成功编译编辑器。
0=本次三个测试组均通过，1=校验失败，2=未执行；引擎非零退出码原样向上传播。
超时只清理本次PID。即使本脚本通过，也不证明Cook、图形、真实资产和多PIE验收通过。
#>
param([string]$EngineRoot, [switch]$Execute,
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 600, [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Game/FoundationTools.psm1') -Force
$context = $null; $code = 2; $message = ''; $details = @{ Suites=@() }; $failedExternal = $false
try {
    $context = New-FoundationContext UnrealTests $RunId
    if (-not $Execute) { throw [IO.FileNotFoundException]::new('需要显式-Execute；本脚本不会静默启动编辑器。') }
    $engine = Get-FoundationEngine $EngineRoot
    $editor = Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    Assert-FoundationFile $editor
    Assert-FoundationFile $context.Project
    # UBT扫描所有描述；保留历史坏文件，但不浪费时间反复启动已知不可用工程。
    foreach ($descriptor in @(Get-ChildItem (Join-Path $context.Workspace 'Game/Plugins') -Recurse -Filter '*.uplugin' -File)) {
        $value = Get-Content -LiteralPath $descriptor.FullName -Raw
        if ([string]::IsNullOrWhiteSpace($value) -or -not ($value | ConvertFrom-Json).FileVersion) {
            throw "正式工程描述阻断，保留原位：$($descriptor.FullName)"
        }
    }
    $code = 0
    foreach ($suite in @(@{Name='Core';Minimum=3},@{Name='Data';Minimum=1},@{Name='ApplicationFlow';Minimum=28})) {
        $directory = Join-Path $context.Directory $suite.Name
        $null = New-Item -ItemType Directory -Path $directory
        $report = Join-Path $directory 'Automation'
        $arguments = @($context.Project,'-unattended','-nop4','-nullrhi','-nosplash','-stdout','-FullStdOutLogOutput',
            "-FoundationRunId=$($context.RunId)","-abslog=$(Join-Path $directory 'Unreal.log')",
            '-GamePlatformFlowTestDefinition=GamePlatformDefinition:foundation.flow@1',
            '-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1',
            "-ExecCmds=Automation RunTests GamePlatform.$($suite.Name)",
            '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report")
        $result = Invoke-FoundationProcess -FilePath $editor -Arguments $arguments -WorkingDirectory (Split-Path $editor) -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds
        $details.Suites += @{Name=$suite.Name;Result=$result;Evidence=$directory}
        if ($result.ExitCode -ne 0) { $code=$result.ExitCode; $failedExternal=$true; break }
        $index = Join-Path $report 'index.json'
        if (-not (Test-Path -LiteralPath $index -PathType Leaf)) { throw "缺少本次Automation导出：$index" }
        $parsed = Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
        $cases = @($parsed.tests | Where-Object { $_.fullTestPath -like "GamePlatform.$($suite.Name).*" })
        if ($cases.Count -lt $suite.Minimum -or @($cases | Where-Object state -ne 'Success').Count) {
            throw "测试组$($suite.Name)缺项或失败；实际$($cases.Count)项，不能以进程退出0替代测试结果。"
        }
    }
    $message = '只接受当前独占证据目录内的实际测试报告；NullRHI不证明三维可见性。'
} catch [IO.FileNotFoundException] { $code=2; $message=$_.Exception.Message }
catch { $code=1; $message=$_.Exception.Message }
if ($context) {
    $status = if ($failedExternal -or $code -eq 1) { 'Failed' } elseif ($code -eq 0) { 'Passed' } else { 'NotExecuted' }
    Write-FoundationResult $context $code $message $details $status
} else { Write-Host $message }
exit $code
