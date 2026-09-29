[CmdletBinding()]
param([string]$WorkspaceRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
$auditModule = Join-Path $PSScriptRoot 'DesignBaselineAudit.psm1'
Import-Module $auditModule -Force
$report = Test-DesignBaselineWorkspace -WorkspaceRoot $WorkspaceRoot
Import-Module (Join-Path $PSScriptRoot 'PluginCompositionAudit.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'InheritanceBoundaryAudit.psm1') -Force
$inheritanceReport = Test-InheritanceBoundaries -WorkspaceRoot $WorkspaceRoot
$compositionErrors = New-Object 'System.Collections.Generic.List[string]'

# 分别验证公共与竞技装配，不靠当前Foundation最小.uproject隐藏未启用的依赖。
foreach ($target in @('Client','Server','Editor')) {
    $publicRoots = if ($target -eq 'Server') { @('DBAGameplay','DBAWorlds','DBAServer') } else { @('DBAGameplay','DBAWorlds','DBAClient') }
    $publicReport = Test-PluginComposition -WorkspaceRoot $WorkspaceRoot -RootPlugins $publicRoots -Target $target -DisabledPlugins @('DBAArena','GamePlatformArena','MobaPresentation')
    foreach ($message in $publicReport.Errors) { $compositionErrors.Add("公共$target 装配：$message") }
    $disabled = if ($target -eq 'Server') { @('DBAClient','MobaPresentation') } else { @() }
    $arenaReport = Test-PluginComposition -WorkspaceRoot $WorkspaceRoot -RootPlugins @('DBAArena') -Target $target -DisabledPlugins $disabled
    foreach ($message in $arenaReport.Errors) { $compositionErrors.Add("竞技$target 装配：$message") }
    Write-Output ("声明装配{0}：公共={1}；竞技={2}" -f $target,$publicReport.Passed,$arenaReport.Passed)
}
Write-Output ("继承边界：PublicHeaders={0}；Types={1}；Edges={2}；Passed={3}" -f $inheritanceReport.PublicHeaderCount,$inheritanceReport.TypeCount,$inheritanceReport.InheritanceEdgeCount,$inheritanceReport.Passed)

Write-Output "工作空间：$($report.WorkspaceRoot)"
Write-Output ("插件描述：{0}/{1}；GamePlatform：{2}/{3}；DBA：{4}/{5}" -f `
    $report.PluginCounts.Actual, $report.PluginCounts.Expected,
    $report.PluginCounts.GamePlatform, $report.PluginCounts.ExpectedGamePlatform,
    $report.PluginCounts.Project, $report.PluginCounts.ExpectedProject)
Write-Output ("工程：{0}；Target：{1}/3；必选默认配置：{2}/{3}；配置文件总数：{4}" -f $report.ProjectCount, $report.TargetCount, $report.RequiredConfigCount, $report.ExpectedConfigCount, $report.ConfigCount)
Write-Output ("代码／机制基线：{0}；内容插件：{1}/{2}（单独登记，不计入{3}个GamePlatform身份）" -f $report.PluginCounts.Baseline,$report.PluginCounts.ContentPacks,$report.PluginCounts.ExpectedContentPacks,$report.PluginCounts.ExpectedGamePlatform)
Write-Output 'GamePlatform 分类数量：'
foreach ($category in $report.CategoryCounts.Keys) {
    Write-Output ("  {0}: {1}" -f $category, $report.CategoryCounts[$category])
}

$inheritanceErrors = @($inheritanceReport.Findings | ForEach-Object { "继承边界：$_" })
$allErrors = @($report.Errors) + @($compositionErrors.ToArray()) + $inheritanceErrors
if ($allErrors.Count -gt 0) {
    Write-Output ("审计失败：{0} 项" -f $allErrors.Count)
    foreach ($errorMessage in $allErrors) {
        Write-Output ("  - {0}" -f $errorMessage)
    }
    exit 1
}

Write-Output '结构审计通过。此结果不代表 UE 编译、Cook/Stage、资产引用、网络或运行时验收通过。'
exit 0
