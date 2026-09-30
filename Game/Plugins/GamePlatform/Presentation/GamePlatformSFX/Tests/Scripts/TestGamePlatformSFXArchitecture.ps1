<#
只读检查SFX端侧、租约、无业务Tick及原生音频生命周期接线。输出诊断，失败退出1；
不启动UE/音频、Cook或网络，源码存在性证据不能代替动态回归。创建不播放及Stopped回收为2026-09-30修复合同。
#>
param()

$ErrorActionPreference = 'Stop'
$errors = [System.Collections.Generic.List[string]]::new()
function Add-Error([string]$Message) { $errors.Add($Message) }

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$pluginRoot = (Resolve-Path (Join-Path $scriptDir '../..')).Path

$upluginPath = Join-Path $pluginRoot 'GamePlatformSFX.uplugin'
if (-not (Test-Path -LiteralPath $upluginPath -PathType Leaf)) {
    Add-Error '缺少GamePlatformSFX.uplugin。'
}
else {
    $descriptor = Get-Content -LiteralPath $upluginPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($descriptor.CanContainContent -ne $false) {
        Add-Error 'SFX机制插件当前不应内置项目内容资产，CanContainContent应为false。'
    }

    $modules = @($descriptor.Modules)
    if ($modules.Count -ne 1) {
        Add-Error 'SFX当前只允许一个真实GamePlatformSFXClient模块。'
    }
    elseif ($modules[0].Name -ne 'GamePlatformSFXClient' -or $modules[0].Type -ne 'ClientOnly') {
        Add-Error 'SFX模块必须为GamePlatformSFXClient ClientOnly。'
    }
    else {
        $targets = @($modules[0].TargetAllowList)
        $hasClient = $targets -contains 'Client'
        $hasEditor = $targets -contains 'Editor'
        if (($targets.Count -ne 2) -or ($hasClient -eq $false) -or ($hasEditor -eq $false)) {
            Add-Error 'SFX TargetAllowList必须精确包含Client和Editor。'
        }
    }

    $pluginNames = @($descriptor.Plugins | ForEach-Object { $_.Name })
    $requiredPlugins = @('GamePlatformCore', 'GamePlatformData', 'GamePlatformPresentation')
    foreach ($requiredPlugin in $requiredPlugins) {
        if ($pluginNames -notcontains $requiredPlugin) {
            Add-Error (('缺少插件依赖：{0}') -f $requiredPlugin)
        }
    }
}

$required = @(
    'README.md',
    'Docs/ImplementationSpecification.md',
    'Docs/Architecture.md',
    'Docs/API.md',
    'Docs/PerformanceAndSecurity.md',
    'Docs/TestingAndEvidence.md',
    'Docs/ManualReview.md',
    'Docs/审查整改方案与执行计划.md',
    'Source/GamePlatformSFXClient/Public/Types/GamePlatformSFXTypes.h',
    'Source/GamePlatformSFXClient/Public/Definitions/GamePlatformSFXDefinition.h',
    'Source/GamePlatformSFXClient/Public/Interfaces/IGamePlatformSFXService.h',
    'Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp',
    'Source/GamePlatformSFXClient/Private/Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.cpp',
    'Source/GamePlatformSFXClient/Private/Tests/GamePlatformSFXPolicyTests.cpp'
)
foreach ($relative in $required) {
    if (-not (Test-Path (Join-Path $pluginRoot $relative) -PathType Leaf)) {
        Add-Error ("缺少必需文件：{0}" -f $relative)
    }
}

$sourceRoot = Join-Path $pluginRoot 'Source'
$sourceText = ''
if (Test-Path $sourceRoot) {
    $sourceText = (Get-ChildItem $sourceRoot -Recurse -File -Include '*.h','*.cpp','*.cs' |
        ForEach-Object { Get-Content $_.FullName -Raw -Encoding UTF8 }) -join [Environment]::NewLine
}

foreach ($forbidden in @('DivineBeasts','MobaPresentation','GamePlatformArena','GamePlatformOnline','GamePlatformSession','UMG','Slate','Niagara')) {
    if ($sourceText -match [regex]::Escape($forbidden)) {
        Add-Error ("SFX源码出现禁止的业务/横向依赖：{0}" -f $forbidden)
    }
}
if ($sourceText -match '\bTick\s*\(' -or $sourceText -match 'AddTicker') { Add-Error 'SFX不得使用Tick/Ticker轮询生命周期。' }
if ($sourceText -match 'LoadSynchronous') { Add-Error 'SFX不得同步加载音频资源。' }

$definitionPath = Join-Path $pluginRoot 'Source/GamePlatformSFXClient/Public/Definitions/GamePlatformSFXDefinition.h'
if (Test-Path $definitionPath) {
    $definition = Get-Content $definitionPath -Raw -Encoding UTF8
    foreach ($evidence in @('UGamePlatformDefinitionBase','TSoftObjectPtr<USoundBase>','USoundAttenuation','USoundConcurrency','AssetBundles="SFXRuntime"')) {
        if ($definition -notmatch [regex]::Escape($evidence)) { Add-Error ("SFX Definition缺少关键证据：{0}" -f $evidence) }
    }
}

$subsystemPath = Join-Path $pluginRoot 'Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp'
if (Test-Path $subsystemPath) {
    $subsystem = Get-Content $subsystemPath -Raw -Encoding UTF8
    if ($subsystem -match '\bSpawnSound(2D|AtLocation|Attached)\s*\(') {
        Add-Error 'SFX不得在委托与参数登记前用自动播放的SpawnSound入口启动。'
    }
    foreach ($evidence in @('AcquireDefinition','EGamePlatformDataLifetime::World','OnAudioFinishedNative','FAudioDevice::CreateComponent','Params.bPlay = false','OnAudioPlayStateChangedNative','HandleAudioPlayStateChanged','IsRunningCommandlet')) {
        if ($subsystem -notmatch [regex]::Escape($evidence)) { Add-Error ("SFX执行器缺少关键证据：{0}" -f $evidence) }
    }
}

$bridgePath = Join-Path $pluginRoot 'Source/GamePlatformSFXClient/Private/Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.cpp'
if (Test-Path $bridgePath) {
    $bridge = Get-Content $bridgePath -Raw -Encoding UTF8
    $hasChannel = $bridge.Contains('TEXT("SFX")')
    $hasCancellation = $bridge.Contains('EGamePlatformSFXPredictionState::Cancelled')
    if (($hasChannel -eq $false) -or ($hasCancellation -eq $false)) {
        Add-Error 'SFX Presentation桥缺少ProviderChannel或取消映射。'
    }
}

if ($errors.Count -gt 0) {
    Write-Output ("GamePlatformSFX架构门禁失败：{0}项" -f $errors.Count)
    $errors | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformSFX架构门禁通过：ClientOnly、三层边界、Definition软引用、异步租约、无Tick、原生音频生命周期与Presentation桥接线的静态证据满足当前基线（不代表动态音频验收）。'
exit 0
