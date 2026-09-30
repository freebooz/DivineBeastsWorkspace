param(
    [string]$EngineRoot = "",
    [string]$SourceSearchRoot = "E:\work\Game\DivineBeastsArena",
    [string]$SourceProjectRoot = "",
    [string]$WorkspaceRoot = "",
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"

# 神兽联盟十二生肖原型角色资产生成包装脚本。
# 目标：
# 1. 公共内容包只保留一套 Manny/Quinn、Skeleton、PhysicsAsset 与必要材质/纹理；
# 2. 不导入 Control Rig、Mover 示例和旧动画蓝图；
# 3. 12 个生肖只生成轻量材质实例、Appearance Profile 与 Server-safe Hero Definition；
# 4. 已存在公共资源时不再依赖旧工程，可在未来真实角色替换后继续执行校验/更新。

if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineCandidates = @(
        "F:\UnrealEngine-5.8.0-release",
        "D:\UnrealEngine-5.8.0-release"
    )
    $EngineRoot = $EngineCandidates |
        Where-Object { Test-Path (Join-Path $_ "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") } |
        Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
        throw "未找到 Unreal Engine 5.8。请通过 -EngineRoot 指定实际安装目录。"
    }
}

$TargetProject = Join-Path $WorkspaceRoot "Game\DivineBeastsArena.uproject"
$TargetDbaRoot = Join-Path $WorkspaceRoot "Game\Content\DBA\Characters\Mannequins"
$TargetStandardRoot = Join-Path $WorkspaceRoot "Game\Content\Characters\Mannequins"
$PythonScript = Join-Path $WorkspaceRoot "Tools\Unreal\Characters\GenerateZodiacPrototypeCharacters.py"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

$CommonRoot = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\DBA\Meshes"
$CommonManny = Join-Path $CommonRoot "SKM_Manny_Simple.uasset"
$CommonQuinn = Join-Path $CommonRoot "SKM_Quinn_Simple.uasset"
$CommonSkeleton = Join-Path $CommonRoot "SK_Mannequin_Skeleton.uasset"
$CommonPhysics = Join-Path $CommonRoot "SK_Mannequin_PhysicsAsset.uasset"

if (-not (Test-Path $TargetProject)) { throw "目标项目不存在: $TargetProject" }
if (-not (Test-Path $EditorCmd)) { throw "UnrealEditor-Cmd 不存在: $EditorCmd" }
if (-not (Test-Path $PythonScript)) { throw "生肖生成器不存在: $PythonScript" }

$CommonAssetsReady =
    (Test-Path $CommonManny) -and
    (Test-Path $CommonQuinn) -and
    (Test-Path $CommonSkeleton) -and
    (Test-Path $CommonPhysics)

Write-Host "[DBA] EngineRoot=$EngineRoot"
Write-Host "[DBA] WorkspaceRoot=$WorkspaceRoot"
Write-Host "[DBA] CommonAssetsReady=$CommonAssetsReady"

if (-not $CommonAssetsReady) {
    if ([string]::IsNullOrWhiteSpace($SourceProjectRoot)) {
        if (-not (Test-Path $SourceSearchRoot)) {
            throw "公共 Manny/Quinn 尚未落盘，且源搜索根目录不存在: $SourceSearchRoot"
        }

        $CandidateRoots = @($SourceSearchRoot)
        $CandidateRoots += Get-ChildItem $SourceSearchRoot -Directory -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName

        $SourceProjectRoot = $CandidateRoots |
            Where-Object {
                (Test-Path (Join-Path $_ "Content\DBA\Characters\Mannequins")) -and
                (Test-Path (Join-Path $_ "Content\Characters\Mannequins"))
            } |
            Sort-Object {
                if ((Split-Path $_ -Leaf) -eq "DBA_GameClient") { 0 } else { 1 }
            }, { $_ } |
            Select-Object -First 1
    }

    if ([string]::IsNullOrWhiteSpace($SourceProjectRoot)) {
        throw "未找到可用 Manny/Quinn 源项目。可通过 -SourceProjectRoot 显式指定。"
    }

    $SourceDbaRoot = Join-Path $SourceProjectRoot "Content\DBA\Characters\Mannequins"
    $SourceStandardRoot = Join-Path $SourceProjectRoot "Content\Characters\Mannequins"
    if (-not (Test-Path $SourceDbaRoot)) { throw "DBA Manny/Quinn 源目录不存在: $SourceDbaRoot" }
    if (-not (Test-Path $SourceStandardRoot)) { throw "标准 Mannequin 依赖目录不存在: $SourceStandardRoot" }

    Write-Host "[DBA] SourceProjectRoot=$SourceProjectRoot"

    if (Test-Path $TargetDbaRoot) { Remove-Item $TargetDbaRoot -Recurse -Force }
    if (Test-Path $TargetStandardRoot) { Remove-Item $TargetStandardRoot -Recurse -Force }
    New-Item -ItemType Directory -Path $TargetDbaRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $TargetStandardRoot -Force | Out-Null

    Write-Host "[DBA] Copying Manny/Quinn meshes, skeleton, physics asset and source materials..."
    Copy-Item (Join-Path $SourceDbaRoot "Meshes") $TargetDbaRoot -Recurse -Force
    Copy-Item (Join-Path $SourceDbaRoot "Materials") $TargetDbaRoot -Recurse -Force

    # 只复制占位显示所需的标准材质与纹理。
    # 不复制 Rigs/Animations，避免把旧 Mover/ControlRig 依赖带入正式架构。
    Write-Host "[DBA] Copying standard Mannequin material/texture dependencies..."
    Copy-Item (Join-Path $SourceStandardRoot "Materials") $TargetStandardRoot -Recurse -Force
    Copy-Item (Join-Path $SourceStandardRoot "Textures") $TargetStandardRoot -Recurse -Force
}

$DefinitionRoot = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\DBAGameplay\Content\Definitions"
$HeroPacksRoot = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Heroes"

function Get-ZodiacAssetCounts {
    $DefinitionCount = @(
        Get-ChildItem $DefinitionRoot -Recurse -File -Filter "DA_Hero_Zodiac_*.uasset" -ErrorAction SilentlyContinue
    ).Count
    $ProfileCount = @(
        Get-ChildItem $HeroPacksRoot -Recurse -File -Filter "DA_Appearance_Zodiac_*.uasset" -ErrorAction SilentlyContinue
    ).Count
    $MaterialCount = @(
        Get-ChildItem $HeroPacksRoot -Recurse -File -Filter "MI_Zodiac_*_Prototype.uasset" -ErrorAction SilentlyContinue
    ).Count

    return [PSCustomObject]@{
        Definitions = $DefinitionCount
        Profiles = $ProfileCount
        Materials = $MaterialCount
    }
}

if ($ValidateOnly) {
    $Counts = Get-ZodiacAssetCounts
    Write-Host "[DBA] ValidateOnly Definitions=$($Counts.Definitions) Profiles=$($Counts.Profiles) Materials=$($Counts.Materials)"
    if ($CommonAssetsReady -and $Counts.Definitions -eq 12 -and $Counts.Profiles -eq 12 -and $Counts.Materials -eq 12) {
        Write-Host "[DBA] Zodiac prototype character validation passed."
        exit 0
    }
    throw "生肖原型资产验证失败。要求公共 Manny/Quinn 完整且 Definition/Profile/Material 均为 12 个。"
}

# Editor/Client Target 已显式启用公共角色包和 12 个生肖英雄包。
# 不修改 .uproject；-Multiprocess 用于跳过与本次 Win64 资产生成无关的全平台 AutoSDK 检查。
Write-Host "[DBA] Running Unreal Python asset generator..."
$EditorArgs = @(
    $TargetProject,
    "-ExecutePythonScript=$PythonScript",
    "-Multiprocess",
    "-unattended",
    "-nop4",
    "-nosplash",
    "-NoSound",
    "-nullrhi",
    "-NoCompile",
    "-NoCompileEditor"
)
& $EditorCmd @EditorArgs
if ($LASTEXITCODE -ne 0) {
    throw "Unreal 资产生成失败。ExitCode=$LASTEXITCODE"
}

$Counts = Get-ZodiacAssetCounts
if ($Counts.Definitions -ne 12 -or $Counts.Profiles -ne 12 -or $Counts.Materials -ne 12) {
    throw "生肖生成数量不完整：Definitions=$($Counts.Definitions), Profiles=$($Counts.Profiles), Materials=$($Counts.Materials)"
}

# /Game 下的目录只用于跨挂载点迁移。成功后清理，正式资源只保留在插件内容包中。
if (Test-Path $TargetDbaRoot) { Remove-Item $TargetDbaRoot -Recurse -Force }
if (Test-Path $TargetStandardRoot) { Remove-Item $TargetStandardRoot -Recurse -Force }

Write-Host "[DBA] Zodiac prototype character assets generated and validated: 12/12/12."

