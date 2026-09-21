#requires -Version 7.0
# 报告门禁负例仅构造内存对象，不写伪UE报告或资产，不启动引擎。
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Validation/FoundationAutomationReport.psm1') -Force
$required=@('GamePlatform.Data.Identity','GamePlatform.Data.RealLease')
$passed=0
function Assert-Rejected([object[]]$Cases) {
    $rejected=$false
    try { $null=Assert-FoundationAutomationReport -Cases $Cases -RequiredPaths $required -Prefix 'GamePlatform.Data.' }
    catch { $rejected=$true }
    if (-not $rejected) { throw '不完整/失败/重复/错组报告被错误接受' }
}
Assert-Rejected @([pscustomobject]@{fullTestPath=$required[0];state='Success'}); $passed++
Assert-Rejected @(); $passed++
Assert-Rejected @([pscustomobject]@{fullTestPath=$required[0];state='Success'},[pscustomobject]@{fullTestPath=$required[1];state='Fail'}); $passed++
Assert-Rejected @([pscustomobject]@{fullTestPath=$required[0];state='Success'},[pscustomobject]@{fullTestPath=$required[0];state='Success'},[pscustomobject]@{fullTestPath=$required[1];state='Success'}); $passed++
Assert-Rejected @([pscustomobject]@{fullTestPath='GamePlatform.Core.Identity';state='Success'}); $passed++
$valid=@($required | ForEach-Object { [pscustomobject]@{fullTestPath=$_;state='Success'} })
$count=Assert-FoundationAutomationReport -Cases $valid -RequiredPaths $required -Prefix 'GamePlatform.Data.'
if ($count -ne 2) { throw '完整报告没有返回真实数量' }; $passed++
Assert-Rejected ($valid + [pscustomobject]@{fullTestPath='GamePlatform.Data.Additional';state='NotRun'}); $passed++
Write-Output "AutomationReportCases=$passed Failed=0"
