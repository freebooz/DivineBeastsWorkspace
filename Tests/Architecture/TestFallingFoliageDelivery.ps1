#requires -Version 5.1
<#
.SYNOPSIS
只读校验神兽联盟通用自然飘落物的源资源、三级所有权、客户端烘焙扫描与服务器隔离。
.DESCRIPTION
纯静态门禁，不替代UE5.8资源真实加载、材质/Niagara GPU编译、客户端Cook或人工视觉。
报告仅写入Saved/Validation/FallingFoliage，不生成或覆盖任何UAsset。
#>
[CmdletBinding()]
param(
    [string]$ReportPath
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$Issues = New-Object 'System.Collections.Generic.List[string]'
$Checks = New-Object 'System.Collections.Generic.List[string]'
function Check([bool]$Condition,[string]$Label) {
    if (!$Condition) { $script:Issues.Add($Label) }
    else { $script:Checks.Add($Label) }
}
try {
    $sourceRelative = 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/FallingFoliage/Textures'
    $sourceRoot = Join-Path $Root $sourceRelative
    $manifestPath = Join-Path $sourceRoot 'FallingFoliageSourceManifest.json'
    Check (Test-Path -LiteralPath $manifestPath -PathType Leaf) '源纹理清单存在'
    if (!(Test-Path -LiteralPath $manifestPath)) { throw '没有可校验的源纹理清单' }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Check ($manifest.schemaVersion -eq 1 -and @($manifest.textures).Count -eq 8) '源纹理清单版本及数量'
    foreach ($spec in $manifest.textures) {
        $path = Join-Path $sourceRoot $spec.name
        $exists = Test-Path -LiteralPath $path -PathType Leaf
        Check $exists ("源纹理文件：" + $spec.name)
        if ($exists) {
            $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            Check ($actual -ceq ([string]$spec.sha256).ToLowerInvariant()) ("源纹理SHA-256：" + $spec.name)
        }
    }
    $pluginRoot = Join-Path $Root 'Game/Plugins'
    $platformRoot = 'GamePlatform/Presentation/GamePlatformVFX/Content/Environment/FallingFoliage/'
    $projectRoot = 'DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/FallingFoliage/'
    $frontendRoot = 'DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/Content/'
    # PowerShell数组中用完整表达式包装每个拼接项，防止多行加法被合并成一个字符串。
    $files = @(
        ($platformRoot + 'Materials/M_GP_FallingFoliage_Master.uasset'),
        ($platformRoot + 'EffectTypes/NET_GP_FallingFoliage_Ambient.uasset'),
        ($platformRoot + 'Niagara/NS_GP_FallingFoliage_Base.uasset'),
        ($platformRoot + 'Niagara/Modules/NM_GP_FoliageAirMotion.uasset'),
        ($platformRoot + 'Niagara/Modules/NM_GP_FoliageGroundMotion.uasset'),
        ($frontendRoot + 'CharacterPreview/Environment/Blueprints/BP_DBA_FoliageGroundAnchor.uasset'),
        ($frontendRoot + 'Maps/L_DBA_CharacterStudio.umap')
    )
    foreach ($kind in @('Peach','Maple','Bamboo','Ginkgo')) {
        $files += $projectRoot+"Definitions/DA_DBA_VFX_FallingFoliage_$kind.uasset"
        $files += $projectRoot+"Niagara/NS_DBA_FallingFoliage_$kind.uasset"
        $files += $projectRoot+"Materials/MI_DBA_Foliage_$kind.uasset"
        $files += $projectRoot+"Textures/T_DBA_Foliage_${kind}_Albedo.uasset"
        $files += $projectRoot+"Textures/T_DBA_Foliage_${kind}_Normal.uasset"
    }
    foreach ($file in $files) {
        $path = Join-Path $pluginRoot $file
        $item = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
        Check ($null -ne $item -and $item.Length -gt 0) ("UE资产文件：" + $file)
    }
    $globalData = Get-Content -LiteralPath (Join-Path $Root 'Game/Config/DefaultGame.ini') -Raw -Encoding UTF8
    Check ($globalData.Contains('(Path="/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Definitions")')) '主资产类型扫描包含客户端落叶Definitions'
    $clientData = Get-Content -LiteralPath (Join-Path $Root 'Game/Config/Custom/FrontEndClient/DefaultGame.ini') -Raw -Encoding UTF8
    foreach ($dir in @('/GamePlatformVFX/Environment/FallingFoliage','/DBAPresentationPack_Core/VFX/Environment/FallingFoliage','/DBAFrontEndPack/CharacterPreview/Environment')) {
        Check ($clientData.Contains('+DirectoriesToAlwaysCook=(Path="'+$dir+'")')) ("客户端Cook收录：" + $dir)
    }
    $server = Get-Content -LiteralPath (Join-Path $Root 'Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini') -Raw -Encoding UTF8
    foreach ($plugin in @('GamePlatformVFX','DBAPresentationPack_Core','DBAFrontEndPack')) {
        Check ($server.Contains('/'+$plugin+'/...')) ("服务器Pak规则排除：" + $plugin)
    }
    $app = Get-Content -LiteralPath (Join-Path $Root 'Game/DivineBeastsArena.uproject') -Raw -Encoding UTF8 | ConvertFrom-Json
    $displayPack = @($app.Plugins | Where-Object {$_.Name -eq 'DBAPresentationPack_Core'})
    Check ($displayPack.Count -eq 1 -and $displayPack[0].Enabled -and @($displayPack[0].TargetAllowList) -contains 'Client' -and @($displayPack[0].TargetAllowList) -notcontains 'Server') '内容插件仅面向Client/Editor'
    $catalogSource = Get-Content -LiteralPath (Join-Path $Root 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp') -Raw -Encoding UTF8
    foreach ($species in @('peach','maple','bamboo','ginkgo')) {
        Check ($catalogSource.Contains("presentation.dba.environment.fallingfoliage.$species@1")) ("中立表现目录逻辑ID：" + $species)
    }
    $consumer = Get-Content -LiteralPath (Join-Path $Root 'Tests/Assets/TestFrontEndClientContainer.ps1') -Raw -Encoding UTF8
    foreach ($token in @('NS_GP_FallingFoliage_Base.uasset','NM_GP_FoliageGroundMotion.uasset','DA_DBA_VFX_FallingFoliage_$style.uasset','BP_DBA_FoliageGroundAnchor.uasset')) {
        Check ($consumer.Contains($token)) ("IoStore静态检查约束：" + $token)
    }
    $tokens = $null
    $parserIssues = $null
    [Management.Automation.Language.Parser]::ParseInput($consumer,[ref]$tokens,[ref]$parserIssues) | Out-Null
    Check (@($parserIssues).Count -eq 0) 'IoStore验收脚本语法'
}
catch {
    $Issues.Add("校验异常：" + $_.Exception.Message)
}
$summary = [ordered]@{
    schemaVersion = 1
    testName = 'FallingFoliage.StaticDelivery'
    project = $Root
    passed = ($Issues.Count -eq 0)
    checksPassed = $Checks.Count
    checksFailed = $Issues.Count
    checks = @($Checks)
    failures = @($Issues)
    disclaimer = '本结果只证明源文件/配置逻辑成立，不证明UE资产蓝图编译、真实客户端Cook或风吹画面完成。'
}
if (!$ReportPath) { $ReportPath = Join-Path $Root 'Saved/Validation/FallingFoliage/StaticDeliveryAudit.json' }
$parent = Split-Path -Parent $ReportPath
if (!(Test-Path $parent)) { $null = New-Item -Path $parent -ItemType Directory -Force }
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
Write-Output ("FALLING_FOLIAGE_STATIC_AUDIT passed="+$summary.passed+" checks="+$summary.checksPassed+" failures="+$summary.checksFailed)
foreach ($failure in $Issues) { Write-Output ("ERROR: "+$failure) }
if ($Issues.Count -gt 0) { exit 1 }
exit 0
