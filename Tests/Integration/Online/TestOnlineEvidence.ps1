#requires -Version 7.0
<# 只测试报告判定算法，不生成假的后端/UE成功报告。 #>
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Validation/OnlineEvidence.psm1') -Force
$count=0
function Assert-Code($Actual,$Expected) { if ($Actual -ne $Expected) { throw "预期$Expected，实际$Actual" };$script:count++ }
$passed=@{Name='Backend';Status='Passed';ExitCode=0;Evidence='本测试内存条目，非业务验收证据'}
Assert-Code (Get-OnlineVerificationVerdict -Checks @($passed) -RequiredNames @('Backend','UE')) 2
Assert-Code (Get-OnlineVerificationVerdict -Checks @($passed,@{Name='UE';Status='NotExecuted'}) -RequiredNames @('Backend','UE')) 2
Assert-Code (Get-OnlineVerificationVerdict -Checks @($passed,@{Name='UE';Status='Failed'}) -RequiredNames @('Backend','UE')) 1
Assert-Code (Get-OnlineVerificationVerdict -Checks @($passed) -RequiredNames @('Backend')) 0
foreach ($checks in @(
    @($passed,$passed),
    @(@{Name='Backend';Status='Passed';ExitCode=1;Evidence='错误码不能算通过'}),
    @(@{Name='Backend';Status='NotApplicable';Reason=''}),
    @(@{Name='Backend';Status='Unknown'})
)) {
    $rejected=$false
    try { $null=Get-OnlineVerificationVerdict -Checks $checks -RequiredNames @('Backend') } catch { $rejected=$true }
    if (-not $rejected) { throw '必须拒绝重复、矛盾或无效验收证据。' }
    $count++
}
Write-Host "Online报告门禁：$count 项通过（仅报告算法，不是业务验收）。"
