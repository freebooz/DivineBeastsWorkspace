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
# 完整身份按Ordinal区分大小写；错误后缀不能替代必需测试，前缀和状态语义保持严格。
Assert-Rejected @([pscustomobject]@{fullTestPath=$required[0];state='Success'},[pscustomobject]@{fullTestPath='GamePlatform.Data.realLease';state='Success'}); $passed++
Assert-Rejected @([pscustomobject]@{fullTestPath='gamePlatform.Data.Identity';state='Success'},[pscustomobject]@{fullTestPath=$required[1];state='Success'}); $passed++
Assert-Rejected @([pscustomobject]@{fullTestPath=$required[0];state='success'},[pscustomobject]@{fullTestPath=$required[1];state='Success'}); $passed++
# 仅大小写不同的额外成功身份是独立测试；正确必需项仍须各出现一次。
$count=Assert-FoundationAutomationReport -Cases ($valid + [pscustomobject]@{fullTestPath='GamePlatform.Data.realLease';state='Success'}) -RequiredPaths $required -Prefix 'GamePlatform.Data.'
if ($count -ne 3) { throw 'Ordinal身份没有保留独立的额外测试' }; $passed++
$count=Assert-FoundationAutomationReport -Cases ($valid + [pscustomobject]@{fullTestPath='GamePlatform.Core.Other';state='Fail'}) -RequiredPaths $required -Prefix 'GamePlatform.Data.'
if ($count -ne 2) { throw '错组报告改变了本组数量' }; $passed++
Write-Output "AutomationReportCases=$passed Failed=0"
