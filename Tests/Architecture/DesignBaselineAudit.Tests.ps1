Describe '设计基线只读结构审计' {
    $auditModulePath = Join-Path $PSScriptRoot 'DesignBaselineAudit.psm1'
    if (Test-Path -LiteralPath $auditModulePath) {
        Import-Module $auditModulePath -Force
    }

    # 该手工核对清单构造审计夹具，避免用生产函数的期望值生成它自身的测试输入。
    $script:ExpectedPluginGroups = [ordered]@{
        Foundation = @('GamePlatformCore', 'GamePlatformData')
        Application = @('GamePlatformApplicationFlow', 'GamePlatformInput', 'GamePlatformLoading', 'GamePlatformSettings', 'GamePlatformSave', 'GamePlatformLocalization')
        OnlineServices = @('GamePlatformOnline', 'GamePlatformSession', 'GamePlatformServer')
        World = @('GamePlatformWorld', 'GamePlatformOpenWorld', 'GamePlatformPCG', 'GamePlatformInteraction', 'GamePlatformNavigation')
        Gameplay = @('GamePlatformGameplay', 'GamePlatformCharacter', 'GamePlatformAbilitySystem', 'GamePlatformCombat', 'GamePlatformAI', 'GamePlatformQuest', 'GamePlatformAnimation')
        Presentation = @('GamePlatformUI', 'GamePlatformPresentation', 'GamePlatformVFX', 'GamePlatformSFX', 'GamePlatformCamera')
        GameModes = @('GamePlatformLobby', 'GamePlatformVillage')
        PlayerServices = @('GamePlatformInventory', 'GamePlatformEntitlement', 'GamePlatformEquipment', 'GamePlatformProgression', 'GamePlatformLiveOps', 'GamePlatformCommerceUI')
        Diagnostics = @('GamePlatformTelemetry', 'GamePlatformDebug', 'GamePlatformDeveloperTools')
    }
    $script:ExpectedProjectPlugins = @('DBAGameplay', 'DBAWorlds', 'DBAClient', 'DBAServer', 'DBAArena')
    $script:ExpectedConfigs = @(
        'DefaultEngine.ini', 'DefaultGame.ini', 'DefaultInput.ini',
        'DefaultGameplayTags.ini', 'DefaultGameUserSettings.ini',
        'DefaultDeviceProfiles.ini', 'DefaultScalability.ini', 'DefaultEditor.ini'
    )

    function New-ValidDesignBaselineFixture {
        param([Parameter(Mandatory)][string]$WorkspaceRoot)

        foreach ($category in $script:ExpectedPluginGroups.Keys) {
            foreach ($pluginName in $script:ExpectedPluginGroups[$category]) {
                $pluginDirectory = Join-Path $WorkspaceRoot "Game/Plugins/GamePlatform/$category/$pluginName"
                $null = New-Item -ItemType Directory -Path $pluginDirectory -Force
                $descriptor = [ordered]@{ FileVersion = 3; Modules = @() } | ConvertTo-Json -Depth 5
                Set-Content -LiteralPath (Join-Path $pluginDirectory "$pluginName.uplugin") -Value $descriptor
            }
        }

        foreach ($pluginName in $script:ExpectedProjectPlugins) {
            $pluginDirectory = Join-Path $WorkspaceRoot "Game/Plugins/DivineBeasts/$pluginName"
            $null = New-Item -ItemType Directory -Path $pluginDirectory -Force
            $descriptor = [ordered]@{ FileVersion = 3; Modules = @() } | ConvertTo-Json -Depth 5
            Set-Content -LiteralPath (Join-Path $pluginDirectory "$pluginName.uplugin") -Value $descriptor
        }

        $arenaDirectory = Join-Path $WorkspaceRoot 'Game/Plugins/MobaCommon/GamePlatformArena'
        $null = New-Item -ItemType Directory -Path $arenaDirectory -Force
        Set-Content -LiteralPath (Join-Path $arenaDirectory 'GamePlatformArena.uplugin') -Value '{"FileVersion":3,"Modules":[]}'

        $mobaPresentationDirectory = Join-Path $WorkspaceRoot 'Game/Plugins/MobaCommon/Presentation/MobaPresentation'
        $null = New-Item -ItemType Directory -Path $mobaPresentationDirectory -Force
        $mobaPresentationDescriptor = [ordered]@{ FileVersion = 3; Modules = @() } | ConvertTo-Json -Depth 5
        Set-Content -LiteralPath (Join-Path $mobaPresentationDirectory 'MobaPresentation.uplugin') -Value $mobaPresentationDescriptor

        $gameDirectory = Join-Path $WorkspaceRoot 'Game'
        $null = New-Item -ItemType Directory -Path (Join-Path $gameDirectory 'Config') -Force
        Set-Content -LiteralPath (Join-Path $gameDirectory 'DivineBeastsArena.uproject') -Value '{"FileVersion":3}'
        foreach ($configName in $script:ExpectedConfigs) {
            Set-Content -LiteralPath (Join-Path $gameDirectory "Config/$configName") -Value "[Baseline]`nFixtureSetting=True`n; 结构审计夹具，不作为正式游戏配置。"
        }

        $sourceDirectory = Join-Path $gameDirectory 'Source'
        $null = New-Item -ItemType Directory -Path $sourceDirectory -Force
        foreach ($targetName in @('DivineBeastsArenaClient', 'DivineBeastsArenaServer', 'DivineBeastsArenaEditor')) {
            Set-Content -LiteralPath (Join-Path $sourceDirectory "$targetName.Target.cs") -Value '// 结构审计夹具，不参与UE编译。'
        }
    }

    function New-DesignBaselineFixtureRoot {
        $workspaceRoot = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $workspaceRoot
        New-ValidDesignBaselineFixture -WorkspaceRoot $workspaceRoot
        return $workspaceRoot
    }

    It '接受跨三层的40个GamePlatform身份、5个项目插件及独立MOBA表现插件、一个工程、三个Target和八项默认配置' {
        $workspaceRoot = New-DesignBaselineFixtureRoot

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $true
        $result.PluginCounts.Actual | Should Be 46
        $result.PluginCounts.GamePlatform | Should Be 40
        $result.PluginCounts.Project | Should Be 5
        $result.PluginCounts.MobaPresentation | Should Be 1
        $result.ProjectCount | Should Be 1
        $result.TargetCount | Should Be 3
        $result.ConfigCount | Should Be 8
    }

    It '缺少目标插件时报告插件身份而不是误报通过' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        Remove-Item -LiteralPath (Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAWorlds/DBAWorlds.uplugin')

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'DBAWorlds'
    }

    It '内容登记路径越界时失败而不是读取工程外部描述' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $registryRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks'
        $null = New-Item -ItemType Directory -Path $registryRoot -Force
        '{"SchemaVersion":1,"ContentPacks":[{"Name":"DBAHeroPack_Rat","RelativePath":"../Outside"}]}' |
            Set-Content -LiteralPath (Join-Path $registryRoot 'ContentPackRegistry.json')
        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '内容.*路径'
    }

    It '已登记内容包计数单列但无真实资产的空包不能通过交付门禁' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $registryRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks'
        $packRoot = Join-Path $registryRoot 'Heroes/DBAHeroPack_Rat'
        $null = New-Item -ItemType Directory -Path $packRoot -Force
        '{"SchemaVersion":1,"ContentPacks":[{"Name":"DBAHeroPack_Rat","RelativePath":"Heroes/DBAHeroPack_Rat"}]}' |
            Set-Content -LiteralPath (Join-Path $registryRoot 'ContentPackRegistry.json')
        '{"FileVersion":3,"CanContainContent":true}' | Set-Content -LiteralPath (Join-Path $packRoot 'DBAHeroPack_Rat.uplugin')
        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot
        $result.PluginCounts.Baseline | Should Be 46
        $result.PluginCounts.ContentPacks | Should Be 1
        $result.PluginCounts.GamePlatform | Should Be 40
        $result.PluginCounts.Project | Should Be 5
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '内容包.*缺少真实UE资产'
    }

    It '内容登记不能借已有插件身份覆盖正式路径' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $registryRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks'
        $null = New-Item -ItemType Directory -Path $registryRoot -Force
        '{"SchemaVersion":1,"ContentPacks":[{"Name":"DBAClient","RelativePath":"Common/DBAClient"}]}' |
            Set-Content -LiteralPath (Join-Path $registryRoot 'ContentPackRegistry.json')
        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '内容.*重复.*DBAClient'
    }

    It '发现非基线插件时返回失败并指出多余身份' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $extraDirectory = Join-Path $workspaceRoot 'Game/Plugins/GamePlatform/Gameplay/UnapprovedPlugin'
        $null = New-Item -ItemType Directory -Path $extraDirectory -Force
        Set-Content -LiteralPath (Join-Path $extraDirectory 'UnapprovedPlugin.uplugin') -Value '{"FileVersion":3,"Modules":[]}'

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'UnapprovedPlugin'
    }

    It '重复插件描述即使总数相近也必须失败' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $duplicateDirectory = Join-Path $workspaceRoot 'Game/Plugins/LegacyCopy'
        $null = New-Item -ItemType Directory -Path $duplicateDirectory -Force
        Set-Content -LiteralPath (Join-Path $duplicateDirectory 'GamePlatformCore.uplugin') -Value '{"FileVersion":3,"Modules":[]}'

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '重复.*GamePlatformCore'
    }

    It '模块声明缺少对应Source模块目录和Build规则时失败' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $pluginDirectory = Join-Path $workspaceRoot 'Game/Plugins/GamePlatform/Foundation/GamePlatformCore'
        $descriptorPath = Join-Path $pluginDirectory 'GamePlatformCore.uplugin'
        $descriptor = [ordered]@{
            FileVersion = 3
            Modules = @([ordered]@{ Name = 'GamePlatformCore'; Type = 'Runtime'; LoadingPhase = 'Default' })
        } | ConvertTo-Json -Depth 5
        Set-Content -LiteralPath $descriptorPath -Value $descriptor
        $moduleDirectory = Join-Path $pluginDirectory 'Source/GamePlatformCore'
        $null = New-Item -ItemType Directory -Path $moduleDirectory -Force

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'GamePlatformCore.Build.cs'
    }

    It '审计不会在被审查工程中创建或修改文件' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        $before = @(Get-ChildItem -LiteralPath $workspaceRoot -Recurse -File | ForEach-Object { $_.FullName.Substring($workspaceRoot.Length) } | Sort-Object)

        $null = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $after = @(Get-ChildItem -LiteralPath $workspaceRoot -Recurse -File | ForEach-Object { $_.FullName.Substring($workspaceRoot.Length) } | Sort-Object)
        Compare-Object -ReferenceObject $before -DifferenceObject $after | Should BeNullOrEmpty
    }

    It '缺少任一规定默认配置时失败' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        Remove-Item -LiteralPath (Join-Path $workspaceRoot 'Game/Config/DefaultGameplayTags.ini')

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'DefaultGameplayTags.ini'
    }

    It '默认配置文件只有空白和注释时失败' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        Set-Content -LiteralPath (Join-Path $workspaceRoot 'Game/Config/DefaultDeviceProfiles.ini') -Value "; 尚无经验证的设备专属覆盖。`n"

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'DefaultDeviceProfiles.ini.*没有有效配置段'
    }

    It '正式工程不得把项目地图设为游戏或服务器默认地图' {
        $workspaceRoot = New-DesignBaselineFixtureRoot
        Set-Content -LiteralPath (Join-Path $workspaceRoot 'Game/Config/DefaultEngine.ini') -Value "[/Script/EngineSettings.GameMapsSettings]`nGameDefaultMap=/Game/Development/Foundation/Maps/L_FoundationBootstrap"

        $result = Test-DesignBaselineWorkspace -WorkspaceRoot $workspaceRoot

        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '不得设置生产默认地图'
    }
}
