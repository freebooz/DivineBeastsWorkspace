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
}
