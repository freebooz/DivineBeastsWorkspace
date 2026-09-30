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
    'Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXCommonParameters.h',
    'Shaders/Private/GamePlatformVFXCommonMotion.ush',
    'SourceArt/VFXLib/Common/SourceManifest.json',
    'Docs/VFXLibMigration.md',
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

$commonParameterPath = Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXCommonParameters.h'
$commonParameterText = [IO.File]::ReadAllText($commonParameterPath)
foreach ($parameter in @('User.PrimaryColor','User.SecondaryColor','User.CoreColor','User.Intensity','User.Duration','User.Radius','User.Length','User.Width','User.Speed','User.Seed','User.SourcePosition','User.TargetPosition','User.Direction','User.Scale')) {
    $cppPath = Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Private/Types/GamePlatformVFXCommonParameters.cpp'
    $cppText = [IO.File]::ReadAllText($cppPath)
    Assert-Rule ($cppText.Contains($parameter)) ('Missing common Niagara parameter: ' + $parameter)
}

$shaderPath = Join-Path $PluginRoot 'Shaders/Private/GamePlatformVFXCommonMotion.ush'
$shaderText = [IO.File]::ReadAllText($shaderPath)
foreach ($function in @('GPVFX_Hash01','GPVFX_OrbitOffset','GPVFX_ExpandingRadius','GPVFX_BallisticOffset','GPVFX_TrailWidth','GPVFX_SoftFloat','GPVFX_GoldenAngleDirection')) {
    Assert-Rule ($shaderText.Contains($function)) ('Missing common VFX shader function: ' + $function)
}
Assert-Rule (-not ($shaderText -match '(?i)Frost|PetalBloom|Frostbolt|Zodiac|DivineBeasts|DBA\.')) 'Platform common shader leaked project/theme semantics'

$clientModulePath = Join-Path $PluginRoot 'Source/GamePlatformVFXClient/Private/GamePlatformVFXClientModule.cpp'
$clientModuleText = [IO.File]::ReadAllText($clientModulePath)
Assert-Rule ($clientModuleText.Contains('/Plugin/GamePlatformVFX')) 'Client module must register stable GamePlatformVFX shader virtual path'
Assert-Rule ($clientModuleText.Contains('AddShaderSourceDirectoryMapping')) 'Client module must register plugin shader source directory'

$sourceManifest = [IO.File]::ReadAllText((Join-Path $PluginRoot 'SourceArt/VFXLib/Common/SourceManifest.json')) | ConvertFrom-Json
Assert-Rule (@($sourceManifest).Count -eq 6) 'VFX Lib first migration batch must contain exactly six neutral SourceArt files'
Assert-Rule (-not (Test-Path (Join-Path $PluginRoot 'Content/SourceArt'))) 'SourceArt must not be placed under runtime Content'

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
