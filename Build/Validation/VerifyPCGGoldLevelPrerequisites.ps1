#requires -Version 5.1
<#
.SYNOPSIS
GamePlatformPCG（游戏平台PCG）Gold Level（金标准关卡）前置门禁。
.DESCRIPTION
只验证源码、模板合同、编辑器生成入口和验收文档已准备，不验证真实.umap/.uasset。
真实地图/模板必须由Unreal Editor生成后另行执行Automation/DataValidation/Cook。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}

$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/World/GamePlatformPCG'
$required = @(
    (Join-Path $pluginRoot 'Docs/GoldLevelAcceptance.md'),
    (Join-Path $pluginRoot 'Docs/组件清单与使用说明.md'),
    (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp'),
    (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp'),
    (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGFoundationTemplatesCommandlet.cpp'),
    (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Validators/GamePlatformPCGWorldValidator.cpp')
)

$errors = [Collections.Generic.List[string]]::new()
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $errors.Add("缺少Gold Level前置文件：$path")
    }
}

if ($errors.Count -eq 0) {
    $authoring = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp') -Raw -Encoding UTF8
    $graph = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp') -Raw -Encoding UTF8
    $validator = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Validators/GamePlatformPCGWorldValidator.cpp') -Raw -Encoding UTF8
    $gold = Get-Content -LiteralPath (Join-Path $pluginRoot 'Docs/GoldLevelAcceptance.md') -Raw -Encoding UTF8

    foreach ($token in @('CreateFoundationTemplateAssets','CreateFoundationSubgraphAssets','CreateFoundationAssets')) {
        if (-not $authoring.Contains($token)) { $errors.Add("Editor Library缺少：$token") }
    }
    foreach ($token in @('CreateFoundationTemplateGraph','CreateFoundationSubgraphGraph')) {
        if (-not $graph.Contains($token)) { $errors.Add("Foundation Graph生成器缺少：$token") }
    }
    foreach ($token in @('Directors.Num() != 1','ValidateParticipantSet','Registered.Num() != ParticipantsInWorld.Num()')) {
        if (-not $validator.Contains($token)) { $errors.Add("World Validator缺少门禁：$token") }
    }
    foreach ($token in @('G01','G16','Dedicated Server','RequiredDefinitions','OutputFingerprint')) {
        if (-not $gold.Contains($token)) { $errors.Add("Gold Level验收规范缺少：$token") }
    }
}

if ($errors.Count -gt 0) {
    Write-Output ("GamePlatformPCG Gold Level前置门禁失败：{0}项" -f $errors.Count)
    $errors | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformPCG Gold Level前置门禁通过。'
Write-Output '已确认：M0/M1模板生成源码、Commandlet、World Validator和Gold Level验收矩阵已准备。'
Write-Output '注意：此结果不代表Foundation .uasset、Gold Level .umap、Automation、Cook或性能验收已完成。'
exit 0
