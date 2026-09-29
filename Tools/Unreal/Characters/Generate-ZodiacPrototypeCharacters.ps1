param(
    [string]$EngineRoot = "",
    [string]$SourceSearchRoot = "E:\\work\\Game\\DivineBeastsArena",
    [string]$SourceProjectRoot = "",
    [string]$WorkspaceRoot = "E:\\work\\2026\\DivineBeastsWorkspace"
)

$ErrorActionPreference = "Stop"

# 神兽联盟十二生肖原型角色资产生成包装脚本。
# 只导入 Manny/Quinn 共用网格、骨架及其必要原始材质依赖，不为十二生肖复制十二套基础资源。
# 所有跨挂载点移动都交给 Unreal Editor 执行，让引擎负责重写二进制资产引用。

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineCandidates = @(
        "F:\\UnrealEngine-5.8.0-release",
        "D:\\UnrealEngine-5.8.0-release"
    )
    $EngineRoot = $EngineCandidates |
        Where-Object { Test-Path (Join-Path $_ "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") } |
        Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
        throw "未找到 Unreal Engine 5.8。请通过 -EngineRoot 指定实际安装目录。"
    }
}

if ([string]::IsNullOrWhiteSpace($SourceProjectRoot)) {
    if (-not (Test-Path $SourceSearchRoot)) {
        throw "Manny/Quinn 搜索根目录不存在: $SourceSearchRoot"
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

    if ([string]::IsNullOrWhiteSpace($SourceProjectRoot)) {
        throw "在 $SourceSearchRoot 下未找到同时包含 DBA/Characters/Mannequins 与 Characters/Mannequins 的源项目。"
    }
}

Write-Host "[DBA] EngineRoot=$EngineRoot"
Write-Host "[DBA] SourceProjectRoot=$SourceProjectRoot"
Write-Host "[DBA] WorkspaceRoot=$WorkspaceRoot"

$SourceDbaRoot = Join-Path $SourceProjectRoot "Content\DBA\Characters\Mannequins"
$SourceStandardRoot = Join-Path $SourceProjectRoot "Content\Characters\Mannequins"

$TargetProject = Join-Path $WorkspaceRoot "Game\DivineBeastsArena.uproject"
$TargetDbaRoot = Join-Path $WorkspaceRoot "Game\Content\DBA\Characters\Mannequins"
$TargetStandardRoot = Join-Path $WorkspaceRoot "Game\Content\Characters\Mannequins"
$PythonScript = Join-Path $WorkspaceRoot "Tools\Unreal\Characters\GenerateZodiacPrototypeCharacters.py"
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$CommonMesh = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\DBA\Meshes\SKM_Manny_Simple.uasset"
$CommonSkeleton = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\DBA\Meshes\SK_Mannequin_Skeleton.uasset"

if (-not (Test-Path $SourceDbaRoot)) { throw "DBA Manny/Quinn source directory not found: $SourceDbaRoot" }
if (-not (Test-Path $SourceStandardRoot)) { throw "Standard Mannequin dependency directory not found: $SourceStandardRoot" }
if (-not (Test-Path $TargetProject)) { throw "Target project not found: $TargetProject" }
if (-not (Test-Path $EditorCmd)) { throw "UnrealEditor-Cmd not found: $EditorCmd" }
if (-not (Test-Path $PythonScript)) { throw "Generator script not found: $PythonScript" }

if ((-not (Test-Path $CommonMesh)) -or (-not (Test-Path $CommonSkeleton))) {
    if (Test-Path $TargetDbaRoot) { Remove-Item $TargetDbaRoot -Recurse -Force }
    if (Test-Path $TargetStandardRoot) { Remove-Item $TargetStandardRoot -Recurse -Force }

    New-Item -ItemType Directory -Path $TargetDbaRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $TargetStandardRoot -Force | Out-Null

    Write-Host "[DBA] Copying Manny/Quinn meshes, skeleton, physics asset and source materials..."
    Copy-Item (Join-Path $SourceDbaRoot "Meshes") $TargetDbaRoot -Recurse -Force
    Copy-Item (Join-Path $SourceDbaRoot "Materials") $TargetDbaRoot -Recurse -Force

    # 只复制占位显示所需的标准材质与纹理，不导入Control Rig、Mover示例或动画蓝图。
    Write-Host "[DBA] Copying standard Mannequin material/texture dependencies..."
    Copy-Item (Join-Path $SourceStandardRoot "Materials") $TargetStandardRoot -Recurse -Force
    Copy-Item (Join-Path $SourceStandardRoot "Textures") $TargetStandardRoot -Recurse -Force
}

# Editor/Client Target已经显式启用公共角色包和12个生肖英雄包。
# 生成过程不修改.uproject，避免把临时插件状态写回正式工程。
Write-Host "[DBA] Running Unreal Python asset generator..."
$EditorArgs = @(
    $TargetProject,
    "-ExecutePythonScript=$PythonScript",
    "-unattended",
    "-nop4",
    "-nosplash",
    "-NoSound",
    "-nullrhi",
    "-NoCompile",
    "-NoCompileEditor",
    # UE5.8 TargetPlatformManager在Multiprocess模式下不启动ValidatePlatforms -AllPlatforms；
    # 本次仅生成Win64编辑器资产，不检查iOS/VisionOS等未安装SDK。
    "-Multiprocess"
)
& $EditorCmd @EditorArgs
if ($LASTEXITCODE -ne 0) {
    throw "Unreal asset generation failed. ExitCode=$LASTEXITCODE"
}

if ((Test-Path $TargetDbaRoot) -or (Test-Path $TargetStandardRoot)) {
    Write-Warning "Temporary Mannequin directories still exist. Check Unreal migration logs."
}

Write-Host "[DBA] Zodiac prototype character assets generated."

