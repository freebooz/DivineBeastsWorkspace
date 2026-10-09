# 项目插件静态身份/声明回归：只读源码和描述，验证五个代码插件及真实直接依赖。
# CommonUI是锁定UE5.8的Engine/Plugins/Runtime/CommonUI/CommonUI.uplugin，不能误当缺失项目插件。
# GameplayAbilities同样已核锁定UE5.8的Runtime/GameplayAbilities/GameplayAbilities.uplugin；服务器ASC桥直接消费其模块。
# 此已核对的内置身份不证明本机引擎/二进制可用；UBT链接、Cook及运行仍须独立验收。
Describe '神兽联盟项目插件按DBA边界收敛' {
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $projectPluginRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts'
    $expectedPluginNames = @('DBAArena', 'DBAClient', 'DBAGameplay', 'DBAServer', 'DBAWorlds')

    function Get-ModuleDependenciesFromBuildRule {
        param([Parameter(Mandatory)][string]$Path)

        $source = Get-Content -LiteralPath $Path -Raw
        $dependencies = New-Object 'System.Collections.Generic.List[string]'
        $pattern = '(?:Public|Private|DynamicallyLoaded)DependencyModuleNames\.AddRange\s*\([^\{]*\{(?<items>.*?)\}\s*\)\s*;'
        foreach ($block in [System.Text.RegularExpressions.Regex]::Matches(
            $source,
            $pattern,
            [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
            foreach ($dependency in [System.Text.RegularExpressions.Regex]::Matches(
                $block.Groups['items'].Value,
                '"([A-Za-z0-9_]+)"')) {
                $dependencies.Add($dependency.Groups[1].Value)
            }
        }
        return @($dependencies | Select-Object -Unique)
    }

    It '项目代码插件恰为五个，内容插件不混入代码清单' {
        $actualNames = @(
            Get-ChildItem -LiteralPath $projectPluginRoot -Directory | Where-Object { $_.Name -ne 'ContentPacks' } | Get-ChildItem -Filter '*.uplugin' -File |
                ForEach-Object { $_.BaseName } |
                Sort-Object
        )
        ($actualNames -join ',') | Should Be 'DBAArena,DBAClient,DBAGameplay,DBAServer,DBAWorlds'
    }

    It '平台插件的编辑器分类不再显示旧GameFoundation层名' {
        $descriptors = @(Get-ChildItem -LiteralPath (Join-Path $workspaceRoot 'Game/Plugins/GamePlatform') -Filter '*.uplugin' -File -Recurse)
        foreach ($file in $descriptors) {
            $descriptor = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
            $descriptor.Category | Should Not Match '^GameFoundation(?:\.|$)'
        }
    }

    It '当前项目模块按共享、客户端、服务器和世界职责归属且身份唯一' {
        $expectedModules = @{
            DBAArena = @('DivineBeastsArenaRuntime', 'DivineBeastsArenaClient', 'DivineBeastsArenaServer')
            # DivineBeastsInputClient 是项目输入语义与平台输入/GAS之间的客户端组合边界，
            # 必须作为 DBAClient 的独立 ClientOnly 模块纳入正式身份清单。
            DBAClient = @('DivineBeastsApplicationFlowClient', 'DivineBeastsInputClient', 'DivineBeastsPresentationClient', 'DivineBeastsPresentationRuntime', 'DivineBeastsUIClient')
            # 主线真实技能授权模块归既有DBAGameplay，保持五插件身份；它不是新竞技或内容播放器。
            DBAGameplay = @('DivineBeastsAbilitiesRuntime', 'DivineBeastsCharactersRuntime', 'DivineBeastsRuntime')
            DBAServer = @('DBAServer')
            DBAWorlds = @('DBAWorldsRuntime')
        }
        foreach ($pluginName in $expectedPluginNames) {
            $descriptorPath = Join-Path $projectPluginRoot "$pluginName/$pluginName.uplugin"
            if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
                $false | Should Be $true
                continue
            }
            $descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
            $actual = @($descriptor.Modules | ForEach-Object { $_.Name } | Sort-Object)
            $expected = @($expectedModules[$pluginName] | Sort-Object)
            ($actual -join ',') | Should Be ($expected -join ',')
        }
    }

    It '插件依赖声明覆盖模块直接依赖且只指向真实插件' {
        $expectedDependencies = @{
            DBAArena = @('DBAGameplay', 'DBAClient', 'GamePlatformArena', 'GamePlatformCore', 'GamePlatformData', 'CommonUI')
            DBAClient = @('DBAGameplay', 'GamePlatformApplicationFlow', 'GamePlatformCharacter', 'GamePlatformLoading', 'GamePlatformOnline', 'GamePlatformPresentation', 'GamePlatformSession', 'GamePlatformUI')
            DBAGameplay = @('GamePlatformCharacter', 'GamePlatformCore')
            DBAServer = @('DBAGameplay', 'GamePlatformServer')
            DBAWorlds = @('DBAGameplay', 'GamePlatformWorld')
        }
        $installedPluginNames = @(Get-ChildItem -LiteralPath (Join-Path $workspaceRoot 'Game/Plugins') -Filter '*.uplugin' -File -Recurse | ForEach-Object { $_.BaseName })
        # 限定已读取真实UE描述的内置身份，其他未知名字仍失败，不无条件放行仓库外依赖。
        $knownEnginePluginNames = @('CommonUI', 'GameplayAbilities')
        foreach ($pluginName in $expectedPluginNames) {
            $descriptorPath = Join-Path $projectPluginRoot "$pluginName/$pluginName.uplugin"
            if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
                $false | Should Be $true
                continue
            }
            $descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
            $actual = @($descriptor.Plugins | ForEach-Object { $_.Name } | Sort-Object)
            foreach ($dependency in $expectedDependencies[$pluginName]) {
                ($actual -contains $dependency) | Should Be $true
            }
            foreach ($dependency in $actual) {
                (($installedPluginNames -contains $dependency) -or ($knownEnginePluginNames -contains $dependency)) | Should Be $true
            }
        }
    }

    It '竞技客户端直接链接真实UI基类且CommonUI不进入服务器装配' {
        # 新链接回归：项目Widget虚表直接使用UMG/CommonUI；描述限定端侧，不能靠传递头可见性。
        $arenaDescriptor = Get-Content -LiteralPath (Join-Path $projectPluginRoot 'DBAArena/DBAArena.uplugin') -Raw | ConvertFrom-Json
        $commonUI = @($arenaDescriptor.Plugins | Where-Object Name -eq 'CommonUI')
        $commonUI.Count | Should Be 1
        (@($commonUI[0].TargetAllowList | Sort-Object) -join ',') | Should Be 'Client,Editor'
        $clientDependencies = @(Get-ModuleDependenciesFromBuildRule -Path (Join-Path $projectPluginRoot 'DBAArena/Source/DivineBeastsArenaClient/DivineBeastsArenaClient.Build.cs'))
        ($clientDependencies -contains 'UMG') | Should Be $true
        ($clientDependencies -contains 'CommonUI') | Should Be $true
    }

    It '每个声明模块都有同名构建规则、注册入口或真实外部模块类型' {
        foreach ($pluginName in $expectedPluginNames) {
            $descriptorPath = Join-Path $projectPluginRoot "$pluginName/$pluginName.uplugin"
            if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
                $false | Should Be $true
                continue
            }
            $descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
            foreach ($module in @($descriptor.Modules)) {
                $moduleRoot = Join-Path (Join-Path (Split-Path -Parent $descriptorPath) 'Source') $module.Name
                $buildRulePath = Join-Path $moduleRoot "$($module.Name).Build.cs"
                if (-not (Test-Path -LiteralPath $buildRulePath -PathType Leaf)) {
                    $false | Should Be $true
                    continue
                }
                $buildSource = Get-Content -LiteralPath $buildRulePath -Raw
                $buildSource | Should Match "public class\s+$([regex]::Escape($module.Name))\s*:\s*ModuleRules"
                $registrationPattern = "IMPLEMENT_(?:MODULE|PRIMARY_GAME_MODULE)\(\s*[^,]+,\s*$([regex]::Escape($module.Name))\s*\)"
                $registrations = @(Get-ChildItem -LiteralPath $moduleRoot -Filter '*.cpp' -File -Recurse -ErrorAction SilentlyContinue | Where-Object {
                    [regex]::IsMatch((Get-Content -LiteralPath $_.FullName -Raw), $registrationPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
                })
                $isExternal = $buildSource -match 'Type\s*=\s*ModuleType\.External'
                if (-not $isExternal) { $registrations.Count | Should BeGreaterThan 0 }
            }
        }
    }

    It '五个项目插件的描述不再依赖旧项目插件身份' {
        $legacyPluginNames = @('DivineBeastsRuntime', 'DivineBeastsApplicationFlow', 'DivineBeastsCharacters', 'DivineBeastsArena', 'DivineBeastsPresentation', 'DivineBeastsUI')
        foreach ($pluginName in $expectedPluginNames) {
            $descriptorPath = Join-Path $projectPluginRoot "$pluginName/$pluginName.uplugin"
            if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
                $false | Should Be $true
                continue
            }
            $descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
            $dependencyNames = @($descriptor.Plugins | ForEach-Object { $_.Name })
            foreach ($legacyName in $legacyPluginNames) {
                ($dependencyNames -contains $legacyName) | Should Be $false
            }
        }
    }

    It 'DBAClient与DBAServer专有模块不互相依赖' {
        $clientDescriptorPath = Join-Path $projectPluginRoot 'DBAClient/DBAClient.uplugin'
        $serverDescriptorPath = Join-Path $projectPluginRoot 'DBAServer/DBAServer.uplugin'
        if (-not (Test-Path -LiteralPath $clientDescriptorPath -PathType Leaf) -or -not (Test-Path -LiteralPath $serverDescriptorPath -PathType Leaf)) {
            $false | Should Be $true
            return
        }
        $clientDescriptor = Get-Content -LiteralPath $clientDescriptorPath -Raw | ConvertFrom-Json
        $serverDescriptor = Get-Content -LiteralPath $serverDescriptorPath -Raw | ConvertFrom-Json
        $clientModuleNames = @($clientDescriptor.Modules | Where-Object { $_.Type -eq 'ClientOnly' } | ForEach-Object { $_.Name })
        $serverModuleNames = @($serverDescriptor.Modules | Where-Object { $_.Type -eq 'ServerOnly' } | ForEach-Object { $_.Name })
        $clientModuleNames.Count | Should BeGreaterThan 0
        $serverModuleNames.Count | Should BeGreaterThan 0
        foreach ($module in @($clientDescriptor.Modules | Where-Object { $_.Type -eq 'ClientOnly' })) {
            $buildRule = Join-Path (Join-Path (Join-Path $projectPluginRoot 'DBAClient/Source') $module.Name) "$($module.Name).Build.cs"
            if (-not (Test-Path -LiteralPath $buildRule -PathType Leaf)) { $false | Should Be $true; continue }
            foreach ($dependency in Get-ModuleDependenciesFromBuildRule -Path $buildRule) {
                $serverModuleNames -contains $dependency | Should Be $false
            }
        }
        foreach ($module in @($serverDescriptor.Modules | Where-Object { $_.Type -eq 'ServerOnly' })) {
            $buildRule = Join-Path (Join-Path (Join-Path $projectPluginRoot 'DBAServer/Source') $module.Name) "$($module.Name).Build.cs"
            if (-not (Test-Path -LiteralPath $buildRule -PathType Leaf)) { $false | Should Be $true; continue }
            foreach ($dependency in Get-ModuleDependenciesFromBuildRule -Path $buildRule) {
                $clientModuleNames -contains $dependency | Should Be $false
            }
        }
    }

    It '共享契约适配只依赖Shared生成头，不链接缺失的旧静态库或包含不存在的生成头' {
        $applicationModuleRoot = Join-Path $projectPluginRoot 'DBAClient/Source/DivineBeastsApplicationFlowClient'
        $buildRule = Get-Content -LiteralPath (Join-Path $applicationModuleRoot 'DivineBeastsApplicationFlowClient.Build.cs') -Raw
        $backendSource = Get-Content -LiteralPath (Join-Path $applicationModuleRoot 'Private/Backend/DivineBeastsApplicationBackend.cpp') -Raw
        $buildRule | Should Not Match 'DivineBeastsContracts'
        $backendSource | Should Not Match 'DivineBeastsApplicationContracts\.generated\.hpp'
        $runtimeBuild = Get-Content -LiteralPath (Join-Path $projectPluginRoot 'DBAGameplay/Source/DivineBeastsRuntime/DivineBeastsRuntime.Build.cs') -Raw
        $runtimeBuild | Should Match 'Backend/internal/tools/contractcodegen'
        $runtimeBuild | Should Match '\.\./\.\./\.\./\.\./\.\./'
    }

    It 'DBAWorlds不能是空占位：有真实模块实现或引擎生成内容资产' {
        $descriptorPath = Join-Path $projectPluginRoot 'DBAWorlds/DBAWorlds.uplugin'
        if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
            $false | Should Be $true
            return
        }
        $descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
        $worldPluginRoot = Split-Path -Parent $descriptorPath
        if (@($descriptor.Modules).Count -gt 0) {
            $sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $worldPluginRoot 'Source') -Filter '*.cpp' -File -Recurse -ErrorAction SilentlyContinue)
            $sourceFiles.Count | Should BeGreaterThan 0
        }
        else {
            $contentAssets = @(Get-ChildItem -LiteralPath $worldPluginRoot -Recurse -File -Include '*.uasset', '*.umap' -ErrorAction SilentlyContinue)
            $contentAssets.Count | Should BeGreaterThan 0
        }
    }

    It 'DBAWorlds真实定义校验项目角色、体验映射和竞技模式上下文' {
        $moduleRoot = Join-Path $projectPluginRoot 'DBAWorlds/Source/DBAWorldsRuntime'
        $headerPath = Join-Path $moduleRoot 'Public/Definitions/DivineBeastsWorldDefinition.h'
        $sourcePath = Join-Path $moduleRoot 'Private/Definitions/DivineBeastsWorldDefinition.cpp'
        foreach ($path in @($headerPath, $sourcePath)) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $false | Should Be $true }
        }
        if ((Test-Path -LiteralPath $headerPath -PathType Leaf) -and (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            $header = Get-Content -LiteralPath $headerPath -Raw
            $source = Get-Content -LiteralPath $sourcePath -Raw
            $header | Should Match 'class DBAWORLDSRUNTIME_API UDivineBeastsWorldDefinition'
            $source | Should Match 'FDivineBeastsProjectCatalog::TryGetServerRoleForExperience'
            $source | Should Match 'FDivineBeastsProjectCatalog::TryGetArenaContextForMode'
            $source | Should Match 'Super::ValidateDefinition\(\)'
        }
    }
}
