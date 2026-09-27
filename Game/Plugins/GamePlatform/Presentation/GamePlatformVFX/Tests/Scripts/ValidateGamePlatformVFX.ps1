[CmdletBinding()]
param([string]$PluginRoot = '')

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PluginRoot)) {
    $PluginRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
}
$PluginRoot = [IO.Path]::GetFullPath($PluginRoot)
$failures = [Collections.Generic.List[string]]::new()

function Assert-Rule([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $failures.Add($Message) }
}

$descriptorPath = Join-Path $PluginRoot 'GamePlatformVFX.uplugin'
Assert-Rule (Test-Path $descriptorPath -PathType Leaf) 'Missing GamePlatformVFX.uplugin'

if (Test-Path $descriptorPath) {
    $descriptor = [IO.File]::ReadAllText($descriptorPath) | ConvertFrom-Json
    $moduleNames = @($descriptor.Modules | ForEach-Object { $_.Name })
    Assert-Rule ($moduleNames -contains 'GamePlatformVFXClient') 'Missing GamePlatformVFXClient module'
    Assert-Rule ($moduleNames -contains 'GamePlatformVFXEditor') 'Missing GamePlatformVFXEditor module'
    Assert-Rule (-not ($moduleNames -contains 'GamePlatformVFXServer')) 'Pure VFX plugin must not add a server module'
}

$requiredFiles = @(
    'Source/GamePlatformVFXClient/Public/Interfaces/GamePlatformVFXService.h',
    'Source/GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h',
    'Source/GamePlatformVFXClient/Public/Catalogs/GamePlatformVFXCatalog.h',
    'Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp',
    'Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXResolver.cpp',
    'Source/GamePlatformVFXClient/Private/Execution/GamePlatformVFXNiagaraExecutor.cpp',
    'Source/GamePlatformVFXEditor/GamePlatformVFXEditor.Build.cs'
)
foreach ($relative in $requiredFiles) {
    Assert-Rule (Test-Path (Join-Path $PluginRoot $relative) -PathType Leaf) ('Missing required file: ' + $relative)
}

$sourceFiles = @(Get-ChildItem (Join-Path $PluginRoot 'Source') -Recurse -File -Include *.h,*.cpp,*.cs)
foreach ($file in $sourceFiles) {
    $text = [IO.File]::ReadAllText($file.FullName)
    Assert-Rule (-not ($text -match '\b(DBAHero|DivineBeastsHero|MobaCombat|MobaAbility)\b')) ('Upper-layer symbol leaked into platform VFX source: ' + $file.FullName)
}

$binaryAssets = @(Get-ChildItem $PluginRoot -Recurse -File -Include *.uasset,*.umap -ErrorAction SilentlyContinue)
$result = [ordered]@{
    plugin = 'GamePlatformVFX'
    passed = ($failures.Count -eq 0)
    failures = @($failures)
    binaryAssetsObserved = $binaryAssets.Count
    note = 'Source/layout validation only; not UE build, Cook or runtime proof.'
}
$result | ConvertTo-Json -Depth 5
if ($failures.Count -gt 0) { exit 1 }
exit 0
