param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../../../../..')).Path
)

# GamePlatformSave（游戏平台本地存档插件）静态架构门禁。
# 该脚本只验证插件结构、端侧、依赖与关键实现证据，不替代UE编译、Automation或真实掉电恢复测试。
$ErrorActionPreference = 'Stop'
$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/Application/GamePlatformSave'
$errors = New-Object 'System.Collections.Generic.List[string]'

function Add-Error([string]$Message) {
    $errors.Add($Message)
}

$descriptorPath = Join-Path $pluginRoot 'GamePlatformSave.uplugin'
if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
    Add-Error '缺少 GamePlatformSave.uplugin。'
} else {
    try {
        $descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($descriptor.CanContainContent -ne $false) { Add-Error 'Save不应声明CanContainContent=true。' }

        $modules = @($descriptor.Modules)
        if ($modules.Count -ne 1 -or $modules[0].Name -ne 'GamePlatformSaveClient') {
            Add-Error 'Save必须只有一个GamePlatformSaveClient模块。'
        } elseif ($modules[0].Type -ne 'ClientOnly') {
            Add-Error 'GamePlatformSaveClient必须为ClientOnly。'
        } else {
            $targets = @($modules[0].TargetAllowList)
            if ($targets -notcontains 'Client' -or $targets -notcontains 'Editor' -or $targets -contains 'Server') {
                Add-Error 'Save目标必须仅允许Client/Editor，不得进入Server。'
            }
        }

        $pluginDeps = @($descriptor.Plugins | ForEach-Object Name)
        if (($pluginDeps -join '|') -ne 'GamePlatformCore') {
            Add-Error 'Save插件级依赖只允许GamePlatformCore。'
        }
    } catch {
        Add-Error ("插件描述解析失败：{0}" -f $_.Exception.Message)
    }
}

$required = @(
    'Source/GamePlatformSaveClient/Public/Interfaces/IGamePlatformSaveService.h',
    'Source/GamePlatformSaveClient/Public/Interfaces/IGamePlatformSaveProvider.h',
    'Source/GamePlatformSaveClient/Public/Types/GamePlatformSaveTypes.h',
    'Source/GamePlatformSaveClient/Private/Policy/GamePlatformSavePolicy.h',
    'Source/GamePlatformSaveClient/Private/Policy/GamePlatformSavePolicy.cpp',
    'Source/GamePlatformSaveClient/Private/Storage/GamePlatformSaveStorage.h',
    'Source/GamePlatformSaveClient/Private/Storage/GamePlatformSaveStorage.cpp',
    'Source/GamePlatformSaveClient/Private/Subsystems/GamePlatformSaveSubsystem.h',
    'Source/GamePlatformSaveClient/Private/Subsystems/GamePlatformSaveSubsystem.cpp',
    'Source/GamePlatformSaveClient/Private/Services/GamePlatformSaveService.cpp',
    'Source/GamePlatformSaveClient/Private/Tests/GamePlatformSavePolicyTests.cpp',
    'Docs/Architecture.md',
    'Docs/API.md',
    'Docs/PerformanceAndSecurity.md',
    'Docs/TestingAndEvidence.md',
    'Docs/ManualReview.md',
    'Docs/审查整改方案与执行计划.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $pluginRoot $relative) -PathType Leaf)) {
        Add-Error ("缺少必需文件：{0}" -f $relative)
    }
}

$sourceRoot = Join-Path $pluginRoot 'Source'
$chunks = @()
if (Test-Path -LiteralPath $sourceRoot) {
    $chunks = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include '*.h','*.cpp','*.cs' |
        ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 })
}
$sourceText = $chunks -join [Environment]::NewLine

foreach ($forbidden in @(
    'GamePlatformOnline',
    'GamePlatformSession',
    'GamePlatformInventory',
    'GamePlatformEquipment',
    'GamePlatformProgression',
    'GamePlatformCommerce',
    'GamePlatformSettingsClient',
    'GamePlatformData',
    'HttpModule',
    'Niagara',
    'UMG',
    'Slate'
)) {
    if ($sourceText -match [regex]::Escape($forbidden)) {
        Add-Error ("Save源码存在禁止的横向/业务依赖：{0}" -f $forbidden)
    }
}

$buildPath = Join-Path $pluginRoot 'Source/GamePlatformSaveClient/GamePlatformSaveClient.Build.cs'
if (Test-Path -LiteralPath $buildPath) {
    $build = Get-Content -LiteralPath $buildPath -Raw -Encoding UTF8
    foreach ($requiredDependency in @('"Core"','"CoreUObject"','"Engine"','"GamePlatformCore"')) {
        if ($build -notmatch [regex]::Escape($requiredDependency)) {
            Add-Error ("Build.cs缺少依赖：{0}" -f $requiredDependency)
        }
    }
}

$subsystemPath = Join-Path $pluginRoot 'Source/GamePlatformSaveClient/Private/Subsystems/GamePlatformSaveSubsystem.cpp'
if (Test-Path -LiteralPath $subsystemPath) {
    $subsystem = Get-Content -LiteralPath $subsystemPath -Raw -Encoding UTF8
    if ($subsystem -notmatch 'IsRunningDedicatedServer\(\)' -or $subsystem -notmatch 'IsRunningCommandlet\(\)') {
        Add-Error 'Save子系统缺少专服/命令行运行门禁。'
    }
    if ($subsystem -match '\bTick\s*\(' -or $subsystem -match 'AddTicker') {
        Add-Error 'Save不得依赖Tick/Ticker轮询。'
    }
    if ($subsystem -notmatch 'Async\(' -or $subsystem -notmatch 'AsyncTask\(') {
        Add-Error 'Save磁盘IO必须异步执行并回到游戏线程完成回调。'
    }
}

$storagePath = Join-Path $pluginRoot 'Source/GamePlatformSaveClient/Private/Storage/GamePlatformSaveStorage.cpp'
if (Test-Path -LiteralPath $storagePath) {
    $storage = Get-Content -LiteralPath $storagePath -Raw -Encoding UTF8
    foreach ($evidence in @('ProjectSavedDir', '.tmp.', '.bak', 'Move(')) {
        if ($storage -notmatch [regex]::Escape($evidence)) {
            Add-Error ("Save存储实现缺少关键证据：{0}" -f $evidence)
        }
    }
    if ($storage -notmatch 'if\s*\(\s*!Handle->Flush\(true\)\s*\)') {
        Add-Error 'Save必须检查Full Flush返回值，Flush失败不得继续替换主档。'
    }
}

$policyPath = Join-Path $pluginRoot 'Source/GamePlatformSaveClient/Private/Policy/GamePlatformSavePolicy.cpp'
if (Test-Path -LiteralPath $policyPath) {
    $policy = Get-Content -LiteralPath $policyPath -Raw -Encoding UTF8
    if ($policy -notmatch 'MemCrc32' -or $policy -notmatch 'SchemaVersion') {
        Add-Error 'Save封装策略必须具备CRC完整性校验与SchemaVersion版本字段。'
    }
}

if ($errors.Count -gt 0) {
    Write-Output ("GamePlatformSave架构门禁失败：{0}项" -f $errors.Count)
    $errors | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformSave架构门禁通过：ClientOnly、低耦合、异步IO、版本化封装、原子替换、备份恢复与中文文档边界完整。'
exit 0
