#requires -Version 7.0
<# SSLM工具收口回归：只在独占临时目录写普通结果夹具，验证请求摘要失败不得留下可提升的Passed。
   使用生产结果收口函数，不模拟UE/资产，不调用构建；结果的UEExecuted/BuildExecuted始终false。 #>
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
if (-not (Get-Command Write-PluginDemoTestOutput -ErrorAction SilentlyContinue)) { throw 'RED：最终结果尚未在全部请求输出之后收口。' }
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('PluginDemoOutputContract-'+[guid]::NewGuid().ToString('N')); $cases=0
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message};$script:cases++}
try {
    $null=New-Item -ItemType Directory -Path $temporary
    $context=[pscustomobject]@{Directory=$temporary;Workspace=$temporary;Operation='PluginDemoTests';RunId=[guid]::NewGuid().ToString('D')}
    $details=@{UEExecuted=$false;BuildExecuted=$false;FixtureOnly=$true}
    $summary=Join-Path $temporary 'summary.json'
    $first=Write-PluginDemoTestOutput -Context $context -ExitCode 0 -Message '明确离线夹具' -Details $details -RunId ([guid]$context.RunId) -ReportPath $summary
    $canonical=Get-Content -LiteralPath (Join-Path $temporary 'result.json') -Raw | ConvertFrom-Json
    Require ($first.ExitCode -eq 0 -and $canonical.ExitCode -eq 0 -and $canonical.Status -ceq 'Passed') '完成全部要求输出不能保持核心成功。'
    $summaryHash=(Get-FileHash -LiteralPath $summary).Hash
    $conflict=Write-PluginDemoTestOutput -Context $context -ExitCode 0 -Message '明确离线夹具' -Details $details -RunId ([guid]$context.RunId) -ReportPath $summary
    $canonical=Get-Content -LiteralPath (Join-Path $temporary 'result.json') -Raw | ConvertFrom-Json
    Require ($conflict.ExitCode -eq 1 -and $canonical.ExitCode -eq 1 -and $canonical.Status -ceq 'Failed') '追加冲突后canonical仍可被当成Passed。'
    Require ((Get-FileHash -LiteralPath $summary).Hash -ceq $summaryHash) '请求摘要冲突覆盖了旧文件。'
    $missing=Write-PluginDemoTestOutput -Context $context -ExitCode 0 -Message '明确离线夹具' -Details $details -RunId ([guid]$context.RunId) -ReportPath (Join-Path $temporary 'missing/summary.json')
    $canonical=Get-Content -LiteralPath (Join-Path $temporary 'result.json') -Raw | ConvertFrom-Json
    Require ($missing.ExitCode -eq 1 -and $canonical.Status -ceq 'Failed') '写入失败留下成功资格。'
    $alias=Write-PluginDemoTestOutput -Context $context -ExitCode 0 -Message '明确离线夹具' -Details $details -RunId ([guid]$context.RunId) -ReportPath (Join-Path $temporary 'result.json')
    Require ($alias.ExitCode -eq 1) '追加摘要不应占用canonical结果路径。'
    # 辅助入口接线门禁：入口不得绕过生产收口函数提前发布canonical；不是原生进程测试。
    $source=Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../../Build/Game/TestPluginDemo.ps1') -Raw
    Require ($source.Contains('Write-PluginDemoTestOutput') -and -not $source.Contains('Write-FoundationResult')) '测试入口仍提前发布canonical或绕过最终收口。'
} finally {
    $resolved=[IO.Path]::GetFullPath($temporary)
    if((Test-Path -LiteralPath $resolved) -and $resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -match '^PluginDemoOutputContract-[a-f0-9]{32}$'){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
Write-Output "PluginDemoTestOutputContractCases=$cases Failed=0 UEExecuted=False BuildExecuted=False"
