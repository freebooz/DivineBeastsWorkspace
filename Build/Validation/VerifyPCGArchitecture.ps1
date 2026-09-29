#requires -Version 5.1
<#
.SYNOPSIS
GamePlatformPCG（游戏平台程序化内容生成插件）M0/M1源码架构门禁。
.DESCRIPTION
验证双模块、三层边界、P0-P9原语、Schema、模板合同和开发组件清单。
本脚本不替代UHT/C++编译、UE Automation、真实PCG资产、Gold Level或Cook。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}

$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/World/GamePlatformPCG'
$descriptorPath = Join-Path $pluginRoot 'GamePlatformPCG.uplugin'
$runtimeBuild = Join-Path $pluginRoot 'Source/GamePlatformPCG/GamePlatformPCG.Build.cs'
$schemaHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Schema/GamePlatformPCGSchema.h'
$primitiveHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Types/GamePlatformPCGEnvironmentTypes.h'
$templateHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Services/GamePlatformPCGTemplateContract.h'
$componentDoc = Join-Path $pluginRoot 'Docs/组件清单与使用说明.md'
$findings = [Collections.Generic.List[string]]::new()

function Assert-True([bool]$Condition,[string]$Message) {
    if (-not $Condition) { $script:findings.Add($Message) }
}

foreach ($path in @($descriptorPath,$runtimeBuild,$schemaHeader,$primitiveHeader,$templateHeader,$componentDoc)) {
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) ("缺少必要文件：{0}" -f $path)
}

if ($findings.Count -eq 0) {
    $descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True (@($descriptor.Modules).Count -eq 2) '插件必须保持GamePlatformPCG Runtime + GamePlatformPCGEditor Editor双模块。'
    Assert-True (-not [bool]$descriptor.CanContainContent) '生产模板资产尚未验收前CanContainContent必须保持false。'

    $runtimeFiles = Get-ChildItem (Join-Path $pluginRoot 'Source/GamePlatformPCG') -Recurse -File -Include *.h,*.cpp,*.cs
    $runtimeSource = ($runtimeFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"
    foreach ($token in @('ProjectPCG','DivineBeastsPCG','MobaCommon/','DivineBeasts/')) {
        Assert-True (-not $runtimeSource.Contains($token)) ("平台运行源码出现禁止的平行/上层身份：{0}" -f $token)
    }

    $primitive = Get-Content -LiteralPath $primitiveHeader -Raw -Encoding UTF8
    foreach ($id in 0..9) { Assert-True ($primitive.Contains(("P{0}_" -f $id))) ("缺少固定原语P{0}" -f $id) }
    Assert-True (-not $primitive.Contains('P10_')) '禁止新增P10原语。'
    Assert-True (-not $primitive.Contains('L11')) 'Override不得伪装为L11原语。'

    $schema = Get-Content -LiteralPath $schemaHeader -Raw -Encoding UTF8
    foreach ($name in @('BiomeId','LayerName','SpawnMeshSetId','ExcludeMask','ExecSeed','MutableId')) {
        Assert-True ($schema.Contains($name)) ("Schema缺少关键字段：{0}" -f $name)
    }

    $environmentDefinitions = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Definitions/GamePlatformPCGEnvironmentDefinitions.cpp') -Raw -Encoding UTF8
    $profileDefinitions = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Validation/PCGDefinitions.cpp') -Raw -Encoding UTF8
    Assert-True ($environmentDefinitions.Contains('RequiredDefinitions.Contains(Id)')) '环境Definition字段引用必须纳入RequiredDefinitions统一租约。'
    Assert-True ($profileDefinitions.Contains('RequiredDefinitions.Contains(Id)')) 'Profile的ExecPreset/PriorityTable引用必须纳入RequiredDefinitions统一租约。'
    $graphInspection = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Validation/PCGGraphInspection.cpp') -Raw -Encoding UTF8
    Assert-True ($graphInspection.Contains('PCGTemplateRuntimeExecutionDeferred')) 'Template Contract运行时执行在真实资产闭环前必须失败关闭。'

    $template = Get-Content -LiteralPath $templateHeader -Raw -Encoding UTF8
    foreach ($name in @('ScatterSurface','LinearDresser','EnclosureClosed','CropField','RailingAttached')) {
        Assert-True ($template.Contains($name)) ("Template Contract缺少模板ID：{0}" -f $name)
    }

    $docs = Get-Content -LiteralPath $componentDoc -Raw -Encoding UTF8
    foreach ($token in @('怎么使用','当前实现状态','M2','不允许的用法')) {
        Assert-True ($docs.Contains($token)) ("开发组件清单缺少内容：{0}" -f $token)
    }
}

if ($findings.Count -gt 0) {
    Write-Output ("GamePlatformPCG架构门禁失败：{0}项" -f $findings.Count)
    $findings | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformPCG架构门禁通过。'
Write-Output '已确认：双模块、无平行PCG插件、P0-P9固定原语、Schema关键字段、模板合同和开发组件清单。'
Write-Output '此结果不替代UE5.8编译、Automation、真实模板资产、Gold Level、Cook或性能验收。'
exit 0
