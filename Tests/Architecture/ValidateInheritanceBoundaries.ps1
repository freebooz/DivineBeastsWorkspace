[CmdletBinding()]
param([string]$WorkspaceRoot)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
Import-Module (Join-Path $PSScriptRoot 'InheritanceBoundaryAudit.psm1') -Force
$report = Test-InheritanceBoundaries -WorkspaceRoot $WorkspaceRoot
Write-Output ("继承边界扫描：PublicHeaders={0}; Types={1}; Edges={2}" -f $report.PublicHeaderCount,$report.TypeCount,$report.InheritanceEdgeCount)
if (-not $report.Passed) {
    Write-Output ("继承边界失败：{0} 项" -f $report.Findings.Count)
    foreach($finding in $report.Findings){ Write-Output ("  - {0}" -f $finding) }
    exit 1
}
Write-Output '三层C++继承与Public API源码边界通过。此结果不替代真实Blueprint/DataAsset资产父类的UE AssetRegistry/DataValidation检查。'
exit 0
