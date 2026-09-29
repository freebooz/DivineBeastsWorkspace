Set-StrictMode -Version Latest

# 结构基线是实施门槛而非源码能力推断；这里只检查路径、描述、模块规则和配置，不声称完成编译或运行验收。
$script:ExpectedGamePlatformPlugins = [ordered]@{
    Foundation = @('GamePlatformCore', 'GamePlatformData')
    Application = @('GamePlatformApplicationFlow', 'GamePlatformInput', 'GamePlatformLoading', 'GamePlatformSettings', 'GamePlatformSave', 'GamePlatformLocalization')
    OnlineServices = @('GamePlatformOnline', 'GamePlatformSession', 'GamePlatformServer')
    World = @('GamePlatformWorld', 'GamePlatformPCG', 'GamePlatformInteraction', 'GamePlatformNavigation')
    Gameplay = @('GamePlatformGameplay', 'GamePlatformCharacter', 'GamePlatformAbilitySystem', 'GamePlatformCombat', 'GamePlatformAI', 'GamePlatformQuest', 'GamePlatformAnimation')
    Presentation = @('GamePlatformUI', 'GamePlatformPresentation', 'GamePlatformVFX', 'GamePlatformSFX', 'GamePlatformCamera', 'GamePlatformSurface')
    GameModes = @('GamePlatformLobby', 'GamePlatformVillage')
    Moba = @('GamePlatformArena')
    PlayerServices = @('GamePlatformInventory', 'GamePlatformEntitlement', 'GamePlatformEquipment', 'GamePlatformProgression', 'GamePlatformLiveOps', 'GamePlatformCommerceUI')
    Diagnostics = @('GamePlatformTelemetry', 'GamePlatformDebug', 'GamePlatformDeveloperTools')
}
$script:ExpectedProjectPlugins = @('DBAGameplay', 'DBAWorlds', 'DBAClient', 'DBAServer', 'DBAArena')
$script:ExpectedMobaPresentationPlugin = 'MobaPresentation'
$script:ExpectedDefaultConfigs = @(
    'DefaultEngine.ini', 'DefaultGame.ini', 'DefaultInput.ini', 'DefaultGameplayTags.ini',
    'DefaultGameUserSettings.ini', 'DefaultDeviceProfiles.ini', 'DefaultScalability.ini', 'DefaultEditor.ini'
)
$script:ExpectedTargets = @('DivineBeastsArenaClient', 'DivineBeastsArenaServer', 'DivineBeastsArenaEditor')

function Test-DesignBaselineWorkspace {
    <#
    .SYNOPSIS
    只读检查工作空间的插件、工程、Target 和默认配置结构。
    .DESCRIPTION
    不加载 Unreal Engine，不解析蓝图／资产引用，也不推断模块端侧隔离或运行时正确性；调用者须将这些验证另行记录。
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ValidateNotNullOrEmpty()]
        [string]$WorkspaceRoot
    )

    $errors = New-Object 'System.Collections.Generic.List[string]'
    $warnings = New-Object 'System.Collections.Generic.List[string]'
    try {
        $root = [System.IO.Path]::GetFullPath($WorkspaceRoot)
    }
    catch {
        throw "工作空间路径无效：$WorkspaceRoot。$($_.Exception.Message)"
    }
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "工作空间目录不存在：$root"
    }

    $pluginRoot = Join-Path $root 'Game/Plugins'
    $gamePlatformRoot = Join-Path $pluginRoot 'GamePlatform'
    $projectPluginRoot = Join-Path $pluginRoot 'DivineBeasts'
    $allDescriptors = @()
    if (Test-Path -LiteralPath $pluginRoot -PathType Container) {
        $allDescriptors = @(Get-ChildItem -LiteralPath $pluginRoot -Filter '*.uplugin' -File -Recurse -ErrorAction SilentlyContinue)
    }
    else {
        $errors.Add("缺少唯一插件源码根目录：Game/Plugins")
    }

    $gamePlatformNames = @()
    if (Test-Path -LiteralPath $gamePlatformRoot -PathType Container) {
        $gamePlatformNames = @(
            Get-ChildItem -LiteralPath $gamePlatformRoot -Filter '*.uplugin' -File -Recurse -ErrorAction SilentlyContinue |
                ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) }
        )
    }
    $projectPluginNames = @()
    if (Test-Path -LiteralPath $projectPluginRoot -PathType Container) {
        $projectPluginNames = @(
            Get-ChildItem -LiteralPath $projectPluginRoot -Filter '*.uplugin' -File -Recurse -ErrorAction SilentlyContinue |
                ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) }
        )
    }

    $expectedPluginPaths = @{}
    foreach ($category in $script:ExpectedGamePlatformPlugins.Keys) {
        foreach ($pluginName in $script:ExpectedGamePlatformPlugins[$category]) {
            $expectedPluginPaths[$pluginName] = if ($category -eq 'Moba') {
                "Game/Plugins/MobaCommon/$pluginName/$pluginName.uplugin"
            } else { "Game/Plugins/GamePlatform/$category/$pluginName/$pluginName.uplugin" }
        }
    }
    foreach ($pluginName in $script:ExpectedProjectPlugins) {
        $expectedPluginPaths[$pluginName] = "Game/Plugins/DivineBeasts/$pluginName/$pluginName.uplugin"
    }
    $expectedPluginPaths[$script:ExpectedMobaPresentationPlugin] = 'Game/Plugins/MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin'

    # 内容登记只允许已审核的纯内容插件扩展45个代码／机制插件基线，不能借目录存在绕过清单。
    $baselineCount = $expectedPluginPaths.Count
    $contentNames = @()
    $contentRoot = Join-Path $projectPluginRoot 'ContentPacks'
    $registryPath = Join-Path $contentRoot 'ContentPackRegistry.json'
    if (Test-Path -LiteralPath $registryPath -PathType Leaf) {
        try {
            $registry = Get-Content -LiteralPath $registryPath -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
            if ($registry.SchemaVersion -ne 1 -or -not $registry.PSObject.Properties['ContentPacks']) { throw '需要SchemaVersion=1及ContentPacks数组' }
            foreach ($pack in @($registry.ContentPacks)) {
                $name = [string]$pack.Name
                $relative = [string]$pack.RelativePath
                if ($name -notmatch '^DBA[A-Za-z0-9_]+$') { $errors.Add("内容包身份不合法：$name"); continue }
                if ($expectedPluginPaths.ContainsKey($name)) { $errors.Add("内容登记重复插件身份：$name"); continue }
                if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.?([\\/]|$)|:' -or
                    $relative -notmatch '^(Common|Heroes|Worlds|Presentation|Optional)/' -or
                    ($relative -split '/')[-1] -ne $name) { $errors.Add("内容包路径不合法：$name / $relative"); continue }
                $fullPackPath = [IO.Path]::GetFullPath((Join-Path $contentRoot $relative))
                if (-not $fullPackPath.StartsWith([IO.Path]::GetFullPath($contentRoot) + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { $errors.Add("内容包路径越界：$name"); continue }
                $expectedPluginPaths[$name] = "Game/Plugins/DivineBeasts/ContentPacks/$relative/$name.uplugin"
                $contentNames += $name
            }
        } catch { $errors.Add("内容登记无法解析：$($_.Exception.Message)") }
    }

    $moduleNames = @{}

    $descriptorGroups = @($allDescriptors | Group-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) })
    foreach ($group in $descriptorGroups) {
        if ($group.Count -gt 1) {
            $duplicatePaths = ($group.Group | ForEach-Object { $_.FullName }) -join '；'
            $errors.Add("重复插件描述：$($group.Name)；位置：$duplicatePaths")
        }
    }

    $actualDescriptorNames = @($allDescriptors | ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) })
    foreach ($pluginName in $expectedPluginPaths.Keys) {
        $expectedFullPath = [System.IO.Path]::GetFullPath((Join-Path $root $expectedPluginPaths[$pluginName]))
        if (-not (Test-Path -LiteralPath $expectedFullPath -PathType Leaf) -and $actualDescriptorNames -notcontains $pluginName) {
            $errors.Add("缺少基线插件描述：$pluginName（$($expectedPluginPaths[$pluginName])）")
        }
    }

    foreach ($descriptorFile in $allDescriptors) {
        $pluginName = [System.IO.Path]::GetFileNameWithoutExtension($descriptorFile.Name)
        if (-not $expectedPluginPaths.ContainsKey($pluginName)) {
            $relativePluginPath = $descriptorFile.FullName.Substring($pluginRoot.Length).TrimStart([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
            $errors.Add("存在非基线插件描述：$pluginName（Game/Plugins/$relativePluginPath）")
            continue
        }

        $expectedFullPath = [System.IO.Path]::GetFullPath((Join-Path $root $expectedPluginPaths[$pluginName]))
        if (-not [string]::Equals($descriptorFile.FullName, $expectedFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $errors.Add("插件目录不符合基线分类：$pluginName；应为 $($expectedPluginPaths[$pluginName])，实际为 $($descriptorFile.FullName)")
        }

        try {
            $descriptor = Get-Content -LiteralPath $descriptorFile.FullName -Raw -Encoding UTF8 -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        }
        catch {
            $errors.Add("插件描述无法解析：$($descriptorFile.FullName)；$($_.Exception.Message)")
            continue
        }
        $descriptorModules = @(if ($descriptor.PSObject.Properties['Modules']) { $descriptor.Modules })
        if ($pluginName -in $contentNames) {
            if (-not $descriptor.PSObject.Properties['CanContainContent'] -or -not $descriptor.CanContainContent -or $descriptorModules.Count -gt 0) {
                $errors.Add("登记内容包必须为无源码模块的纯内容插件：$pluginName")
            }
            $assets = @(Get-ChildItem -LiteralPath (Join-Path $descriptorFile.DirectoryName 'Content') -Recurse -File -Include '*.uasset','*.umap' -ErrorAction SilentlyContinue)
            if ($assets.Count -eq 0) { $errors.Add("内容包$pluginName 缺少真实UE资产；不得以空包交付") }
        }
        foreach ($otherDescriptor in $allDescriptors) {
            if ($otherDescriptor.FullName -ne $descriptorFile.FullName -and $descriptorFile.FullName.StartsWith($otherDescriptor.DirectoryName + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
                $errors.Add("禁止嵌套插件：$($otherDescriptor.BaseName) / $pluginName")
            }
        }
        foreach ($module in $descriptorModules) {
            if ($null -eq $module -or [string]::IsNullOrWhiteSpace([string]$module.Name)) {
                $errors.Add("插件模块声明缺少模块名：$($descriptorFile.FullName)")
                continue
            }
            $moduleName = [string]$module.Name
            if ($moduleNames.ContainsKey($moduleName)) { $errors.Add("重复模块身份：$moduleName（$pluginName / $($moduleNames[$moduleName])）") }
            else { $moduleNames[$moduleName] = $pluginName }
            $moduleDirectory = Join-Path (Join-Path $descriptorFile.DirectoryName 'Source') $moduleName
            $buildRulePath = Join-Path $moduleDirectory "$moduleName.Build.cs"
            if (-not (Test-Path -LiteralPath $moduleDirectory -PathType Container)) {
                $errors.Add("插件模块缺少源码目录：$pluginName / $moduleName（Source/$moduleName）")
            }
            if (-not (Test-Path -LiteralPath $buildRulePath -PathType Leaf)) {
                $errors.Add("插件模块缺少构建规则：$pluginName / $moduleName（$moduleName.Build.cs）")
            }
        }
    }

    $gameRoot = Join-Path $root 'Game'
    $projectFiles = @()
    if (Test-Path -LiteralPath $gameRoot -PathType Container) {
        $projectFiles = @(Get-ChildItem -LiteralPath $gameRoot -Filter '*.uproject' -File -Recurse -ErrorAction SilentlyContinue)
    }
    if ($projectFiles.Count -ne 1) {
        $errors.Add("正式游戏工程数量应为 1，实际为 $($projectFiles.Count)")
    }
    if ($projectFiles.Count -eq 1 -and $projectFiles[0].Name -ne 'DivineBeastsArena.uproject') {
        $errors.Add("唯一正式工程应为 DivineBeastsArena.uproject，实际为 $($projectFiles[0].Name)")
    }

    $sourceRoot = Join-Path $gameRoot 'Source'
    $targetFiles = @()
    if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        $targetFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Filter '*.Target.cs' -File -ErrorAction SilentlyContinue)
    }
    $targetNames = @($targetFiles | ForEach-Object { $_.Name -replace '\.Target\.cs$', '' } | Sort-Object)
    $expectedTargetNames = @($script:ExpectedTargets | Sort-Object)
    if (($targetNames -join '|') -ne ($expectedTargetNames -join '|')) {
        $errors.Add("正式 UE Target 应恰为 Client、Server、Editor 三个；实际为：$($targetNames -join ', ')")
    }

    $configRoot = Join-Path $gameRoot 'Config'
    $configFiles = @()
    if (Test-Path -LiteralPath $configRoot -PathType Container) {
        $configFiles = @(Get-ChildItem -LiteralPath $configRoot -Filter '*.ini' -File -ErrorAction SilentlyContinue)
    }
    $configNames = @($configFiles | ForEach-Object { $_.Name })
    foreach ($configName in $script:ExpectedDefaultConfigs) {
        if ($configNames -notcontains $configName) {
            $errors.Add("缺少规定的默认配置：Game/Config/$configName")
        }
    }

    foreach ($configFile in $configFiles) {
        $hasSection = $false
        $hasSetting = $false
        $insideSection = $false
        foreach ($line in (Get-Content -LiteralPath $configFile.FullName -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            $trimmedLine = $line.Trim()
            if ([string]::IsNullOrWhiteSpace($trimmedLine) -or
                $trimmedLine.StartsWith(';') -or
                $trimmedLine.StartsWith('#')) {
                continue
            }
            if ($trimmedLine -match '^\[[^\]]+\]$') {
                $hasSection = $true
                $insideSection = $true
                continue
            }
            if ($insideSection -and $trimmedLine -match '^[^=]+=') {
                $hasSetting = $true
            }
        }
        if (-not $hasSection -or -not $hasSetting) {
            $errors.Add("默认配置文件$($configFile.Name)没有有效配置段或配置项")
        }
    }

    $defaultEnginePath = Join-Path $configRoot 'DefaultEngine.ini'
    if (Test-Path -LiteralPath $defaultEnginePath -PathType Leaf) {
        $insideGameMapsSettings = $false
        foreach ($line in (Get-Content -LiteralPath $defaultEnginePath -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            $trimmedLine = $line.Trim()
            if ([string]::IsNullOrWhiteSpace($trimmedLine) -or
                $trimmedLine.StartsWith(';') -or
                $trimmedLine.StartsWith('#')) {
                continue
            }
            if ($trimmedLine -match '^\[(?<section>[^\]]+)\]$') {
                $insideGameMapsSettings = $Matches.section -ieq '/Script/EngineSettings.GameMapsSettings'
                continue
            }
            if ($insideGameMapsSettings -and
                $trimmedLine -match '^(?<key>GameDefaultMap|ServerDefaultMap)\s*=\s*(?<value>[^;#]+)') {
                $mapPath = $Matches.value.Trim().Trim('"')
                if ($mapPath.StartsWith('/Game/', [System.StringComparison]::OrdinalIgnoreCase)) {
                    $errors.Add("DefaultEngine.ini不得设置生产默认地图：$($Matches.key)=$mapPath")
                }
            }
        }
    }

    $categoryCounts = [ordered]@{}
    $actualGamePlatformCount = 0
    foreach ($category in $script:ExpectedGamePlatformPlugins.Keys) {
        $categoryCounts[$category] = @($script:ExpectedGamePlatformPlugins[$category] | Where-Object { $actualDescriptorNames -contains $_ }).Count
        $actualGamePlatformCount += $categoryCounts[$category]
    }
    $actualProjectPluginCount = @($script:ExpectedProjectPlugins | Where-Object { $actualDescriptorNames -contains $_ }).Count
    $actualMobaPresentationCount = @($script:ExpectedMobaPresentationPlugin | Where-Object { $actualDescriptorNames -contains $_ }).Count
    $expectedGamePlatformCount = 0
    foreach ($category in $script:ExpectedGamePlatformPlugins.Keys) {
        $expectedGamePlatformCount += $script:ExpectedGamePlatformPlugins[$category].Count
    }

    [pscustomobject]@{
        Passed = ($errors.Count -eq 0)
        WorkspaceRoot = $root
        Errors = @($errors.ToArray())
        Warnings = @($warnings.ToArray())
        PluginCounts = [pscustomobject]@{
            Actual = $allDescriptors.Count
            Expected = $expectedPluginPaths.Count
            Baseline = $baselineCount
            ContentPacks = @($contentNames | Where-Object { $actualDescriptorNames -contains $_ }).Count
            ExpectedContentPacks = $contentNames.Count
            GamePlatform = $actualGamePlatformCount
            ExpectedGamePlatform = $expectedGamePlatformCount
            Project = $actualProjectPluginCount
            ExpectedProject = $script:ExpectedProjectPlugins.Count
            MobaPresentation = $actualMobaPresentationCount
            ExpectedMobaPresentation = 1
        }
        CategoryCounts = $categoryCounts
        ProjectCount = $projectFiles.Count
        TargetCount = $targetFiles.Count
        ConfigCount = $configFiles.Count
        RequiredConfigCount = @($script:ExpectedDefaultConfigs | Where-Object { $configNames -contains $_ }).Count
        ExpectedConfigCount = $script:ExpectedDefaultConfigs.Count
    }
}

Export-ModuleMember -Function Test-DesignBaselineWorkspace
