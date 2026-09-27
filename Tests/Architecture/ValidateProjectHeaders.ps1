<#
.SYNOPSIS
项目自有头文件存在性预检：0为未发现缺失，1为缺失，2为检查未完成。
.DESCRIPTION
与ValidateDesignBaseline.ps1分开运行，不用结构门禁替代源码可编译性；
此预检也不能替代UE实际构建、类型／方法兼容性检查或服务联调。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
try {
    Import-Module (Join-Path $PSScriptRoot 'ProjectHeaderAudit.psm1') -Force
    $report = Test-ProjectOwnedHeaders -WorkspaceRoot $WorkspaceRoot
    Write-Output "已检查$($report.CheckedIncludes)处自有头文件引用；发现$($report.Findings.Count)处缺失。"
    foreach ($finding in $report.Findings) {
        Write-Output ("{0}:{1}: 缺少 {2}" -f $finding.File,$finding.Line,$finding.Include)
    }
    if (-not $report.Passed) { exit 1 }
    Write-Output '未发现缺失文件；不代表include搜索路径、API签名、UE构建或运行验收通过。'
    exit 0
} catch {
    Write-Output "源码头文件预检未完成：$($_.Exception.Message)"
    exit 2
}
