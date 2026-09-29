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
$EditorExe = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$CommonMesh = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\DBA\Meshes\SKM_Manny_Simple.uasset"
$CommonBodyRig = Join-Path $WorkspaceRoot "Game\Plugins\DivineBeasts\ContentPacks\Common\DBAContentPack_Common\Content\Mannequins\Standard\Rigs\CR_Mannequin_Body.uasset"

if (-not (Test-Path $SourceDbaRoot)) { throw "DBA Manny/Quinn source directory not found: $SourceDbaRoot" }
if (-not (Test-Path $SourceStandardRoot)) { throw "Standard Mannequin dependency directory not found: $SourceStandardRoot" }
if (-not (Test-Path $TargetProject)) { throw "Target project not found: $TargetProject" }
if (-not (Test-Path $EditorExe)) { throw "UnrealEditor not found: $EditorExe" }
if (-not (Test-Path $PythonScript)) { throw "Generator script not found: $PythonScript" }

$OriginalProjectJson = [System.IO.File]::ReadAllText($TargetProject)
$DevelopersDir = Join-Path $WorkspaceRoot "Game\Content\Developers"
$DevelopersBackup = Join-Path $WorkspaceRoot "Game\Saved\ZodiacPrototypeBackup\Developers"
$DevelopersMoved = $false

try {
    if ((-not (Test-Path $CommonMesh)) -or (-not (Test-Path $CommonBodyRig))) {
        if (Test-Path $TargetDbaRoot) { Remove-Item $TargetDbaRoot -Recurse -Force }
        if (Test-Path $TargetStandardRoot) { Remove-Item $TargetStandardRoot -Recurse -Force }

        New-Item -ItemType Directory -Path $TargetDbaRoot -Force | Out-Null
        New-Item -ItemType Directory -Path $TargetStandardRoot -Force | Out-Null

        Write-Host "[DBA] Copying Manny/Quinn meshes, skeleton and source materials..."
        Copy-Item (Join-Path $SourceDbaRoot "Meshes") $TargetDbaRoot -Recurse -Force
        Copy-Item (Join-Path $SourceDbaRoot "Materials") $TargetDbaRoot -Recurse -Force
        Write-Host "[DBA] Copying standard Mannequin material/texture/rig dependencies..."
        Copy-Item (Join-Path $SourceStandardRoot "Materials") $TargetStandardRoot -Recurse -Force
        Copy-Item (Join-Path $SourceStandardRoot "Textures") $TargetStandardRoot -Recurse -Force
        # 旧工程Rigs文件物理上位于DBA目录，但uasset内部包名仍是/Game/Characters/Mannequins/Rigs；
        # 必须先还原到标准Mannequin临时根，再由AssetTools跨挂载点迁移并重写网格引用。
        Copy-Item (Join-Path $SourceDbaRoot "Rigs") $TargetStandardRoot -Recurse -Force
    }

    # 纯内容生肖插件在正常构建中按 Target 选择启用。Python Commandlet 对 Editor Target 收据的消费不稳定，
    # 因此仅在本次生成期间临时启用这些插件，并在 finally 中逐字节恢复原 .uproject。
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

    # UE5.8 当前源码版的 ContentBrowser 数据源会错误地把物理 Developers 绝对路径当成长包名解析。
    # 资产生成不依赖个人 Developers 内容，因此仅在本次无人值守生成期间临时移出，结束后原样恢复。
    if (Test-Path $DevelopersDir) {
        if (Test-Path $DevelopersBackup) { Remove-Item $DevelopersBackup -Recurse -Force }
        New-Item -ItemType Directory -Path (Split-Path $DevelopersBackup -Parent) -Force | Out-Null
        Move-Item $DevelopersDir $DevelopersBackup
        $DevelopersMoved = $true
    }

    # UE5.8 的 PythonScript Commandlet 在当前源码版会错误处理 Developers 绝对路径；
    # 改用编辑器启动参数 ExecutePythonScript，仍保持无界面、无渲染执行，并在脚本完成后自动退出。
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
        "-NoCompileEditor"
    )
    $EditorProcess = Start-Process -FilePath $EditorExe -ArgumentList $EditorArgs -Wait -PassThru
    if ($EditorProcess.ExitCode -ne 0) {
        throw "Unreal asset generation failed. ExitCode=$($EditorProcess.ExitCode)"
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

