[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [string]$Project = '',
    [string]$Platform = 'Win64',
    [string]$ArchiveDirectory = ''
)

$ErrorActionPreference = 'Stop'
$PluginRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
$WorkspaceRoot = [IO.Path]::GetFullPath((Join-Path $PluginRoot '../../../../../../'))
if ([string]::IsNullOrWhiteSpace($Project)) { $Project = Join-Path $WorkspaceRoot 'Game/DivineBeastsArena.uproject' }
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) { $ArchiveDirectory = Join-Path $WorkspaceRoot 'Artifacts/Client/VFXCook' }

$uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
if (-not (Test-Path $uat -PathType Leaf)) { throw ('RunUAT.bat not found: ' + $uat) }
if (-not (Test-Path $Project -PathType Leaf)) { throw ('Project not found: ' + $Project) }

& $uat BuildCookRun `
    ('-project=' + $Project) `
    -noP4 `
    -utf8output `
    ('-platform=' + $Platform) `
    -clientconfig=Development `
    -target=DivineBeastsArenaClient `
    -build `
    -cook `
    -stage `
    -pak `
    -archive `
    ('-archivedirectory=' + $ArchiveDirectory)

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
exit 0
