param(
    [string]$EngineRoot = "D:\\UnrealEngine-5.8.0-release",
    [string]$SourceProjectRoot = "E:\\work\\Game\\DivineBeastsArena\\DBA_GameClient",
    [string]$WorkspaceRoot = "E:\\work\\2026\\DivineBeastsWorkspace"
)

$ErrorActionPreference = "Stop"

# DBA zodiac prototype character asset generation wrapper.
# Only the prototype meshes/skeleton and their minimum original material dependencies are copied.
# Unreal Editor performs all cross-mount moves so binary references are repaired by the engine.

$SourceDbaRoot = Join-Path $SourceProjectRoot "Content\DBA\Characters\Mannequins"
$SourceStandardRoot = Join-Path $SourceProjectRoot "Content\Characters\Mannequins"

$TargetProject = Join-Path $WorkspaceRoot "Game\DivineBeastsArena.uproject"
$TargetDbaRoot = Join-Path $WorkspaceRoot "Game\Content\DBA\Characters\Mannequins"
$TargetStandardRoot = Join-Path $WorkspaceRoot "Game\Content\Characters\Mannequins"
$PythonScript = Join-Path $WorkspaceRoot "Tools\Unreal\Characters\GenerateZodiacPrototypeCharacters.py"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$CommonMesh = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\DBA\Meshes\SKM_Manny_Simple.uasset"

if (-not (Test-Path $SourceDbaRoot)) { throw "DBA Manny/Quinn source directory not found: $SourceDbaRoot" }
if (-not (Test-Path $SourceStandardRoot)) { throw "Standard Mannequin dependency directory not found: $SourceStandardRoot" }
if (-not (Test-Path $TargetProject)) { throw "Target project not found: $TargetProject" }
if (-not (Test-Path $EditorCmd)) { throw "UnrealEditor-Cmd not found: $EditorCmd" }
if (-not (Test-Path $PythonScript)) { throw "Generator script not found: $PythonScript" }

$OriginalProjectJson = [System.IO.File]::ReadAllText($TargetProject)
$DevelopersDir = Join-Path $WorkspaceRoot "Game\Content\Developers"
$DevelopersBackup = Join-Path $WorkspaceRoot "Game\Saved\ZodiacPrototypeBackup\Developers"
$DevelopersMoved = $false

try {
    if (-not (Test-Path $CommonMesh)) {
        if (Test-Path $TargetDbaRoot) { Remove-Item $TargetDbaRoot -Recurse -Force }
        if (Test-Path $TargetStandardRoot) { Remove-Item $TargetStandardRoot -Recurse -Force }

        New-Item -ItemType Directory -Path $TargetDbaRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $TargetStandardRoot -Force | Out-Null

        Write-Host "[DBA] Copying Manny/Quinn meshes, skeleton and source materials..."
        Copy-Item (Join-Path $SourceDbaRoot "Meshes") $TargetDbaRoot -Recurse -Force
        Copy-Item (Join-Path $SourceDbaRoot "Materials") $TargetDbaRoot -Recurse -Force

        Write-Host "[DBA] Copying standard Mannequin material/texture dependencies..."
        Copy-Item (Join-Path $SourceStandardRoot "Materials") $TargetStandardRoot -Recurse -Force
        Copy-Item (Join-Path $SourceStandardRoot "Textures") $TargetStandardRoot -Recurse -Force
    }

    # Content-only hero plugins are target-specific in normal builds. The Python commandlet does not
    # consume the Editor Target receipt consistently, so enable them only for this generation run
    # and restore the original .uproject byte-for-byte in finally.
    $ProjectObject = $OriginalProjectJson | ConvertFrom-Json
    $RequiredPlugins = @(
        "DBAContentPack_Common",
        "DBAHeroPack_Rat", "DBAHeroPack_Ox", "DBAHeroPack_Tiger", "DBAHeroPack_Rabbit",
        "DBAHeroPack_Dragon", "DBAHeroPack_Snake", "DBAHeroPack_Horse", "DBAHeroPack_Goat",
        "DBAHeroPack_Monkey", "DBAHeroPack_Rooster", "DBAHeroPack_Dog", "DBAHeroPack_Boar"
    )
    foreach ($PluginName in $RequiredPlugins) {
        $Existing = @($ProjectObject.Plugins | Where-Object { $_.Name -eq $PluginName })
        if ($Existing.Count -eq 0) {
            $ProjectObject.Plugins += [PSCustomObject]@{ Name = $PluginName; Enabled = $true }
        } else {
            $Existing[0].Enabled = $true
        }
    }
    [System.IO.File]::WriteAllText(
        $TargetProject,
        ($ProjectObject | ConvertTo-Json -Depth 32),
        (New-Object System.Text.UTF8Encoding($false))
    )

    # UE5.8 source build currently asserts in PythonScript commandlet initialization if the physical
    # Developers content directory is present. Move this editor-only directory aside only for the
    # generation process and restore it immediately afterwards.
    if (Test-Path $DevelopersDir) {
        if (Test-Path $DevelopersBackup) { Remove-Item $DevelopersBackup -Recurse -Force }
        New-Item -ItemType Directory -Path (Split-Path $DevelopersBackup -Parent) -Force | Out-Null
        Move-Item $DevelopersDir $DevelopersBackup
        $DevelopersMoved = $true
    }

    Write-Host "[DBA] Running Unreal Python commandlet..."
    $EditorArgs = @(
        $TargetProject,
        "-run=pythonscript",
        "-script=$PythonScript",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-NoSound"
    )
    & $EditorCmd @EditorArgs

    if ($LASTEXITCODE -ne 0) {
        throw "Unreal asset generation failed. ExitCode=$LASTEXITCODE"
    }
}
finally {
    [System.IO.File]::WriteAllText(
        $TargetProject,
        $OriginalProjectJson,
        (New-Object System.Text.UTF8Encoding($false))
    )

    if ($DevelopersMoved -and (Test-Path $DevelopersBackup)) {
        if (Test-Path $DevelopersDir) { Remove-Item $DevelopersDir -Recurse -Force }
        Move-Item $DevelopersBackup $DevelopersDir
    }
}

if ((Test-Path $TargetDbaRoot) -or (Test-Path $TargetStandardRoot)) {
    Write-Warning "Temporary Mannequin directories still exist. Check Unreal migration logs."
}

Write-Host "[DBA] Zodiac prototype character assets generated."

