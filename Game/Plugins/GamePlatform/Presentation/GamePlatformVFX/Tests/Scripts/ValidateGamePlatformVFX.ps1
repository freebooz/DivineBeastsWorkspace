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

    $clientModule = @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformVFXClient' }) | Select-Object -First 1
    $editorModule = @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformVFXEditor' }) | Select-Object -First 1
    Assert-Rule ($clientModule.Type -eq 'ClientOnly') 'GamePlatformVFXClient must remain ClientOnly'
    Assert-Rule ((@($clientModule.TargetAllowList) -join ',') -eq 'Client,Editor') 'GamePlatformVFXClient TargetAllowList must be exactly Client,Editor'
    Assert-Rule ($editorModule.Type -eq 'Editor') 'GamePlatformVFXEditor must remain Editor-only'
    Assert-Rule ((@($editorModule.TargetAllowList) -join ',') -eq 'Editor') 'GamePlatformVFXEditor TargetAllowList must be exactly Editor'
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

$definitionText = [IO.File]::ReadAllText((Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h'))
Assert-Rule ($definitionText -match 'UGamePlatformDefinitionBase') 'VFX Definition must inherit unified UGamePlatformDefinitionBase'
Assert-Rule (-not ($definitionText -match 'public\s+UPrimaryDataAsset')) 'VFX Definition must not restore an independent UPrimaryDataAsset identity system'
Assert-Rule ($definitionText -match 'AssetBundles="VFXRuntime"') 'VFX runtime assets must be declared in the VFXRuntime bundle'
Assert-Rule ($definitionText -match 'GetPlatformVariants') 'Editor validation must be able to inspect all platform Niagara variants'
Assert-Rule ($definitionText -match 'GetQualityVariants') 'Editor validation must be able to inspect all quality Niagara variants'

$definitionValidatorPath = Join-Path $PluginRoot 'Source/GamePlatformVFXEditor/Private/Validation/GamePlatformVFXDefinitionValidator.cpp'
$definitionValidatorText = [IO.File]::ReadAllText($definitionValidatorPath)
Assert-Rule ($definitionValidatorText -match 'GetPlatformVariants') 'Editor validator must inspect platform variants'
Assert-Rule ($definitionValidatorText -match 'GetQualityVariants') 'Editor validator must inspect quality variants'
Assert-Rule ($definitionValidatorText -match 'LoadSynchronous') 'Editor validator must load soft Niagara variants before LWC/Bounds validation'

$worldText = [IO.File]::ReadAllText((Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp'))
Assert-Rule ($worldText -match 'AcquireDefinition') 'Standard VFX Definition loading must use IGamePlatformDataService::AcquireDefinition'
Assert-Rule ($worldText -match 'OnSystemFinished') 'VFX instances must release through Niagara OnSystemFinished lifecycle events'
Assert-Rule ($worldText -match 'EGamePlatformVFXPredictionState::Corrected') 'VFX must implement Corrected prediction semantics'
Assert-Rule ($worldText -match 'IsSupportedWorldType') 'VFX WorldSubsystem must restrict creation to supported game world types'
Assert-Rule (-not ($worldText -match 'FGamePlatformVFXPreloadCoordinator')) 'WorldSubsystem must not restore the retired private Definition preloader'

$legacyPreloader = Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Private/Preloading/GamePlatformVFXPreloadCoordinator.cpp'
Assert-Rule (-not (Test-Path $legacyPreloader)) 'Retired VFX private Definition preloader must stay removed'

$dbaClientDescriptor = [IO.Path]::GetFullPath((Join-Path $PluginRoot '../../../DivineBeasts/DBAClient/DBAClient.uplugin'))
Assert-Rule (Test-Path $dbaClientDescriptor -PathType Leaf) 'Missing DBAClient composition descriptor'
if (Test-Path $dbaClientDescriptor) {
    $dba = [IO.File]::ReadAllText($dbaClientDescriptor) | ConvertFrom-Json
    $dbaPlugins = @($dba.Plugins | ForEach-Object { $_.Name })
    Assert-Rule ($dbaPlugins -contains 'GamePlatformVFX') 'DBAClient must explicitly enable GamePlatformVFX'
}

$serverTarget = [IO.Path]::GetFullPath((Join-Path $PluginRoot '../../../../Source/DivineBeastsArenaServer.Target.cs'))
Assert-Rule (Test-Path $serverTarget -PathType Leaf) 'Missing DivineBeastsArenaServer.Target.cs'
if (Test-Path $serverTarget) {
    $serverTargetText = [IO.File]::ReadAllText($serverTarget)
    Assert-Rule (-not ($serverTargetText -match 'EnablePlugins\.Add\("DBAClient"\)')) 'Dedicated Server target must not enable DBAClient'
    Assert-Rule (-not ($serverTargetText -match 'EnablePlugins\.Add\("GamePlatformVFX"\)')) 'Dedicated Server target must not enable GamePlatformVFX'
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
