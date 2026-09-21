#requires -Version 7.0
Set-StrictMode -Version Latest
function Assert-FoundationAutomationReport {
    <# 只接受本组全部必需测试身份各出现一次且Success；额外失败同样拒绝。返回实际本组数量。
       Cases由调用方从本次独占目录读取，函数不访问旧日志；RequiredPaths不能为空。 #>
    param([AllowEmptyCollection()][object[]]$Cases, [string[]]$RequiredPaths, [string]$Prefix)
    if (-not $RequiredPaths.Count -or [string]::IsNullOrWhiteSpace($Prefix)) { throw '测试门禁必须指定非空必需身份集合与组前缀' }
    $seen=@{}
    foreach ($case in $Cases) {
        if (-not ([string]$case.fullTestPath).StartsWith($Prefix,[StringComparison]::Ordinal)) { continue }
        if ($seen.ContainsKey($case.fullTestPath)) { throw "本次报告存在重复测试身份：$($case.fullTestPath)" }
        if ($case.state -cne 'Success') { throw "本次测试未成功：$($case.fullTestPath) = $($case.state)" }
        $seen[$case.fullTestPath]=$true
    }
    foreach ($path in $RequiredPaths) {
        if (-not $seen.ContainsKey($path)) { throw "缺少本次必需测试：$path；不能用其他成功项抵消" }
    }
    return $seen.Count
}
Export-ModuleMember -Function Assert-FoundationAutomationReport
