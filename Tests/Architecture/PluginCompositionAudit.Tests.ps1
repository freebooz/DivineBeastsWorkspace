Describe '插件按目标装配及非竞技闭包审计' {
    $auditPath = Join-Path $PSScriptRoot 'PluginCompositionAudit.psm1'
    if (Test-Path -LiteralPath $auditPath) { Import-Module $auditPath -Force }
    $workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

    # 只构造声明夹具，不伪造UE资产、不参与正式构建；检验审计器对输入图的行为。
    function New-CompositionPlugin {
        param([string]$Root, [string]$Name, [object[]]$Dependencies = @(), [string]$ModuleDependency = '')
        $layer = if ($Name -eq 'GamePlatformArena') { 'MobaCommon' } elseif ($Name.StartsWith('GamePlatform')) { 'GamePlatform' } else { 'DivineBeasts' }
        $directory = Join-Path $Root "Game/Plugins/$layer/$Name"
        $null = New-Item -ItemType Directory -Path "$directory/Source/$Name" -Force
        @{ FileVersion=3; Plugins=$Dependencies; Modules=@(@{Name=$Name;Type='Runtime';LoadingPhase='Default'}) } |
            ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$directory/$Name.uplugin" -Encoding UTF8
        $rule = if ($ModuleDependency) { 'PublicDependencyModuleNames.AddRange(new[] { "' + $ModuleDependency + '" });' } else { '' }
        Set-Content -LiteralPath "$directory/Source/$Name/$Name.Build.cs" -Value $rule -Encoding UTF8
    }

    It '公共项目在Client、Server、Editor目标均不要求竞技插件' {
        foreach ($target in @('Client','Server','Editor')) {
            $roots = if ($target -eq 'Server') { @('DBAGameplay','DBAWorlds','DBAServer') } else { @('DBAGameplay','DBAWorlds','DBAClient') }
            $result = Test-PluginComposition -WorkspaceRoot $workspaceRoot -RootPlugins $roots -Target $target -DisabledPlugins @('DBAArena','GamePlatformArena','MobaPresentation')
            ($result.Errors -join "`n") | Should BeNullOrEmpty
            $result.Passed | Should Be $true
        }
    }

    It '项目竞技服务器闭包不包含公共客户端或MOBA客户端表现插件' {
        $result = Test-PluginComposition -WorkspaceRoot $workspaceRoot -RootPlugins @('DBAArena') -Target Server -DisabledPlugins @('DBAClient','MobaPresentation')
        ($result.Errors -join "`n") | Should BeNullOrEmpty
        $result.Passed | Should Be $true
        ($result.ReachablePlugins -contains 'GamePlatformArena') | Should Be $true
        ($result.ReachableModules -contains 'DivineBeastsArenaClient') | Should Be $false
        ($result.ReachableModules -contains 'DivineBeastsArenaServer') | Should Be $true
    }

    It '实际主工程与Server目标装配排除纯客户端内容依赖' {
        # 从真实项目启用项和Server Target共同取根；避免只验最小样例闭包遗漏主工程全局启用的内容包。
        $project = Get-Content (Join-Path $workspaceRoot 'Game/DivineBeastsArena.uproject') -Raw | ConvertFrom-Json
        $roots = @($project.Plugins | Where-Object {
            $_.Enabled -and (-not $_.PSObject.Properties['TargetAllowList'] -or 'Server' -in $_.TargetAllowList)
        } | ForEach-Object { $_.Name })
        $target = Get-Content (Join-Path $workspaceRoot 'Game/Source/DivineBeastsArenaServer.Target.cs') -Raw
        $roots += @([regex]::Matches($target, 'EnablePlugins\.Add\("([^"]+)"\)') | ForEach-Object { $_.Groups[1].Value })
        $result = Test-PluginComposition -WorkspaceRoot $workspaceRoot -RootPlugins ($roots | Sort-Object -Unique) -Target Server
        ($result.Errors -join "`n") | Should BeNullOrEmpty
        ($result.ReachablePlugins -contains 'DBAClient') | Should Be $false
        ($result.ReachableModules -contains 'DivineBeastsPresentationRuntime') | Should Be $false
        ($result.ReachableModules -contains 'GamePlatformVFXClient') | Should Be $false
    }

    It '竞技客户端保留公开流程扩展所需的单向依赖' {
        $result = Test-PluginComposition -WorkspaceRoot $workspaceRoot -RootPlugins @('DBAArena') -Target Client
        ($result.Errors -join "`n") | Should BeNullOrEmpty
        $result.Passed | Should Be $true
        ($result.ReachablePlugins -contains 'DBAClient') | Should Be $true
        ($result.ReachableModules -contains 'DivineBeastsArenaServer') | Should Be $false
    }

    It '公共客户端Runtime在额外验证启用时仍被Server模块列表排除' {
        # 保护真实模块允许列表；只验证源码声明，不能替代插件资产挂载/Cook或最终包审计。
        $result = Test-PluginComposition -WorkspaceRoot $workspaceRoot -RootPlugins @('DBAClient') -Target Server
        ($result.Errors -join "`n") | Should BeNullOrEmpty
        ($result.ReachableModules -contains 'DivineBeastsPresentationRuntime') | Should Be $false
        ($result.ReachableModules -contains 'DivineBeastsApplicationFlowClient') | Should Be $false
    }

    It '间接依赖被禁用插件时指出完整责任链' {
        $root = Join-Path $TestDrive 'disabled'
        New-CompositionPlugin $root DBATest @(@{Name='DBAIntermediate';Enabled=$true})
        New-CompositionPlugin $root DBAIntermediate @(@{Name='GamePlatformArena';Enabled=$true})
        New-CompositionPlugin $root GamePlatformArena
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Client -DisabledPlugins @('GamePlatformArena')
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match 'DBATest.*DBAIntermediate.*GamePlatformArena'
    }

    It '插件循环依赖不能因遍历去重被遗漏' {
        $root = Join-Path $TestDrive 'cycle'
        New-CompositionPlugin $root DBATest @(@{Name='DBAOther';Enabled=$true})
        New-CompositionPlugin $root DBAOther @(@{Name='DBATest';Enabled=$true})
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '循环'
    }

    It '平台反向依赖项目必须失败' {
        $root = Join-Path $TestDrive 'reverse'
        New-CompositionPlugin $root GamePlatformCore @(@{Name='DBATest';Enabled=$true})
        New-CompositionPlugin $root DBATest
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('GamePlatformCore') -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '反向依赖'
    }

    It '模块偷偷跨插件依赖不能绕过插件声明' {
        $root = Join-Path $TestDrive 'module'
        New-CompositionPlugin $root DBATest @() 'GamePlatformArena'
        New-CompositionPlugin $root GamePlatformArena
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '未声明.*GamePlatformArena'
    }

    It '缺失的项目依赖不能按引擎插件忽略' {
        $root = Join-Path $TestDrive 'missing'
        New-CompositionPlugin $root DBATest @(@{Name='DBAMissing';Enabled=$true})
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '缺少.*DBAMissing'
    }

    It '同一插件内的模块循环同样拒绝装配' {
        $root = Join-Path $TestDrive 'moduleCycle'
        New-CompositionPlugin $root DBATest @() 'DBATest'
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Editor
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '模块循环.*DBATest'
    }

    It '服务器不能通过已声明插件引用使用客户端模块' {
        $root = Join-Path $TestDrive 'clientModule'
        New-CompositionPlugin $root DBATest @(@{Name='DBAOther';Enabled=$true}) 'DBAOther'
        New-CompositionPlugin $root DBAOther
        $path = Join-Path $root 'Game/Plugins/DivineBeasts/DBAOther/DBAOther.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'ClientOnly'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins @('DBATest') -Target Server
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '模块不可用.*DBAOther'
    }

    It 'Editor单目标条件依赖不误计入Client但Editor仍核查声明' {
        # 只有可确定求值的完整Target.Type条件才能排除；声明夹具不启动UBT或生成UE资产。
        $root = Join-Path $TestDrive 'editorCondition'
        New-CompositionPlugin $root DBATest @(@{Name='GamePlatformReview';Enabled=$true;TargetAllowList=@('Editor')})
        New-CompositionPlugin $root GamePlatformReview
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rulePath = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        Set-Content -LiteralPath $rulePath -Encoding UTF8 -Value 'if (Target.Type == TargetType.Editor) { PrivateDependencyModuleNames.AddRange(new[] { "GamePlatformReview" }); }'
        (Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client).Passed | Should Be $true
        (Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Editor).Passed | Should Be $true
        # Editor缺真实插件声明仍失败，不能把所有Editor引用一律忽略。
        $pluginPath = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/DBATest.uplugin'
        $definition = Get-Content -LiteralPath $pluginPath -Raw | ConvertFrom-Json
        $definition.Plugins = @()
        $definition | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $pluginPath -Encoding UTF8
        (Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Editor).Passed | Should Be $false
    }

    It '注释伪条件及未知复合条件不能遮盖Client的Editor泄漏' {
        # 注释、字符串不是条件；未实现的条件语法保持保守检查，不静默排除真实字面量依赖。
        $root = Join-Path $TestDrive 'unknownCondition'
        New-CompositionPlugin $root DBATest
        New-CompositionPlugin $root GamePlatformReview
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rulePath = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        foreach ($source in @(
            '// if (Target.Type == TargetType.Editor) {',
            'if (Target.Type == TargetType.Editor || Target.bBuildEditor) {',
            'if (Target.Type == TargetType.Editor) {} else {'
        )) {
            Set-Content -LiteralPath $rulePath -Encoding UTF8 -Value ($source + "`n" + 'PrivateDependencyModuleNames.Add("GamePlatformReview"); }')
            (Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client).Passed | Should Be $false
        }
    }

    It 'Target.bBuildEditor无花括号单语句只为Editor引入依赖' {
        $root = Join-Path $TestDrive 'buildEditorSingle'
        New-CompositionPlugin $root DBATest @(@{Name='GamePlatformReview';Enabled=$true;TargetAllowList=@('Editor')})
        New-CompositionPlugin $root GamePlatformReview
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rule = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        Set-Content -LiteralPath $rule -Encoding UTF8 -Value @'
if (Target.bBuildEditor)
    PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "GamePlatformReview" });
PrivateDependencyModuleNames.Add("Core");
'@
        foreach ($target in @('Client','Server','Editor')) {
            $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target $target
            ($result.Errors -join "`n") | Should BeNullOrEmpty
            $result.Passed | Should Be $true
        }
        New-CompositionPlugin $root GamePlatformUnexpected
        Add-Content -LiteralPath $rule -Encoding UTF8 -Value 'PrivateDependencyModuleNames.Add("GamePlatformUnexpected");'
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '未声明.*GamePlatformUnexpected'
    }

    It 'Target.bBuildEditor花括号与否定条件在三目标分别解析' {
        $root = Join-Path $TestDrive 'buildEditorBraced'
        New-CompositionPlugin $root DBATest @(
            @{Name='GamePlatformReview';Enabled=$true;TargetAllowList=@('Editor')},
            @{Name='GamePlatformClient';Enabled=$true;TargetAllowList=@('Client','Server')}
        )
        New-CompositionPlugin $root GamePlatformReview
        New-CompositionPlugin $root GamePlatformClient
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rule = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        Set-Content -LiteralPath $rule -Encoding UTF8 -Value @'
if (Target.bBuildEditor) {
    PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "GamePlatformReview" });
}
if (!Target.bBuildEditor)
    PrivateDependencyModuleNames.Add("GamePlatformClient");
'@
        foreach ($target in @('Client','Server','Editor')) {
            $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target $target
            ($result.Errors -join "`n") | Should BeNullOrEmpty
            $result.Passed | Should Be $true
        }
    }

    It '复合bBuildEditor条件及else仍保守报告风险' {
        $root = Join-Path $TestDrive 'buildEditorUnknown'
        New-CompositionPlugin $root DBATest
        New-CompositionPlugin $root GamePlatformReview
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rule = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        foreach ($source in @(
            'if (Target.bBuildEditor || Target.Type == TargetType.Server) { PrivateDependencyModuleNames.Add("GamePlatformReview"); }',
            'if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("GamePlatformReview"); else PrivateDependencyModuleNames.Add("Core");',
            ('// if (Target.bBuildEditor)' + "`n" + 'PrivateDependencyModuleNames.Add("GamePlatformReview");')
        )) {
            Set-Content -LiteralPath $rule -Encoding UTF8 -Value $source
            $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client
            $result.Passed | Should Be $false
            ($result.Errors -join "`n") | Should Match '未声明.*GamePlatformReview'
        }
    }

    It '同一插件身份重复声明即使按Target拆分也必须失败' {
        # 目标过滤前检查重复声明，避免Editor/Client间出现相同插件不同启用规则。
        $root = Join-Path $TestDrive 'duplicatePlugin'
        New-CompositionPlugin $root DBATest @(
            @{Name='GamePlatformCore';Enabled=$true;TargetAllowList=@('Editor')},
            @{Name='GamePlatformCore';Enabled=$true;TargetAllowList=@('Client')}
        )
        New-CompositionPlugin $root GamePlatformCore
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '重复插件依赖.*GamePlatformCore'
    }

    It '同一目标下公开和私有重复模块依赖必须失败' {
        # 核查Public/Private边界，重复声明既包括内部项目模块也包括引擎模块。
        $root = Join-Path $TestDrive 'duplicateModule'
        New-CompositionPlugin $root DBATest
        $rulePath = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        Set-Content -LiteralPath $rulePath -Encoding UTF8 -Value 'PublicDependencyModuleNames.Add("Core"); PrivateDependencyModuleNames.Add("Core");'
        $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client
        $result.Passed | Should Be $false
        ($result.Errors -join "`n") | Should Match '重复模块依赖.*Core'
    }

    It '插值及原始字符串不能让条件扫描吞掉块外客户端依赖' {
        # 合法C#插值表达式内的嵌套字符串包含花括号；有限正则不能当完整词法器使用。
        $root = Join-Path $TestDrive 'interpolatedString'
        New-CompositionPlugin $root DBATest
        New-CompositionPlugin $root GamePlatformReview
        $path = Join-Path $root 'Game/Plugins/GamePlatform/GamePlatformReview/GamePlatformReview.uplugin'
        $descriptor = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        $descriptor.Modules[0].Type = 'Editor'
        $descriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
        $rulePath = Join-Path $root 'Game/Plugins/DivineBeasts/DBATest/Source/DBATest/DBATest.Build.cs'
        foreach ($literal in @('$"{System.String.Concat("{", "x")}"', '"""{raw}"""', '$@"{System.String.Concat("{", "x")}"', '@$"{System.String.Concat("{", "x")}"')) {
            $source = 'class Rules { void Setup() { if (Target.Type == TargetType.Editor) { var text = ' + $literal + '; } PrivateDependencyModuleNames.Add("GamePlatformReview"); } }'
            Set-Content -LiteralPath $rulePath -Encoding UTF8 -Value $source
            $result = Test-PluginComposition -WorkspaceRoot $root -RootPlugins DBATest -Target Client
            $result.Passed | Should Be $false
            ($result.Errors -join "`n") | Should Match '模块不可用.*GamePlatformReview'
        }
    }
}
