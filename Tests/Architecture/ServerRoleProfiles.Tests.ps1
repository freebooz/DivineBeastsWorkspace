Describe '三角色服务器启动Profile契约' {
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $profileRoot = Join-Path $workspaceRoot 'Deploy/Server'
    $catalogPath = Join-Path $workspaceRoot 'Shared/Contracts/Games/DivineBeasts/Schemas/server-catalog.schema.json'
    $expectedRoles = @('MainArena', 'OpenWorld', 'Village')

    function Get-WorldContextFixturePairs([string]$Source) {
        $contexts = [regex]::Match($Source, '(?s)ValidContexts\[\]\s*=\s*\{(?<body>.*?)\};')
        if (-not $contexts.Success) { throw '没有找到世界正向测试夹具' }
        # 先完整提取字符串对，再校验版本／身份；不能先过滤@1而漏掉非法条目。
        [regex]::Matches($contexts.Groups['body'].Value, 'TEXT\("(?<role>[^"]+)"\)\s*,\s*TEXT\("(?<experience>[^"@]+)(?:@(?<version>[^"]*))?"\)')
    }

    It '每种正式服务器角色恰有一个Profile，且不建立角色专属Target' {
        $actualRoles = @(
            Get-ChildItem -LiteralPath $profileRoot -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { $_.Name } |
                Sort-Object
        )
        ($actualRoles -join ',') | Should Be ($expectedRoles -join ',')

        $roleTargets = @(Get-ChildItem -LiteralPath (Join-Path $workspaceRoot 'Game') -Filter '*Target.cs' -File -Recurse |
            Where-Object { $_.Name -match '(Lobby|Village|OpenWorld|MainArena)' })
        $roleTargets.Count | Should Be 0
    }

    It '每个Profile只声明已知角色、允许体验、地图与非秘密运行策略' {
        $catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
        $roleExperienceMap = @{}
        foreach ($entry in $catalog.'x-role-experience-map') {
            $roleExperienceMap[$entry.serverRole] = @($entry.experiences)
        }

        foreach ($roleName in $expectedRoles) {
            $profilePath = Join-Path (Join-Path $profileRoot $roleName) 'server-profile.json'
            if (-not (Test-Path -LiteralPath $profilePath -PathType Leaf)) {
                $false | Should Be $true
                continue
            }
            $profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
            $profile.serverRoleId | Should Be "GameServer.Role.$roleName"
            @($profile.allowedExperienceIds).Count | Should BeGreaterThan 0
            (@($profile.allowedExperienceIds) -contains $profile.defaultExperienceId) | Should Be $true
            (@($roleExperienceMap[$profile.serverRoleId]) -contains $profile.defaultExperienceId) | Should Be $true
            $profile.worldPackage | Should Match '^/Game/'
            @($profile.requiredAssets).Count | Should BeGreaterThan 0
            $profile.instancePolicy | Should Not BeNullOrEmpty
            $profile.readiness.requireRequiredAssets | Should Be $true

            ($profile | ConvertTo-Json -Depth 20) | Should Not Match '(?i)(password|secret|private.?key|bearer.?token)'
        }
    }

    It 'MainArena Profile明确列出全部五种模式，其他角色不冒充竞技模式' {
        $expectedModes = @(
            'Arena.Mode.Duel1v1',
            'Arena.Mode.Team2v2',
            'Arena.Mode.Team3v3',
            'Arena.Mode.Team4v4',
            'Arena.Mode.Team5v5'
        )
        foreach ($roleName in $expectedRoles) {
            $profilePath = Join-Path (Join-Path $profileRoot $roleName) 'server-profile.json'
            if (-not (Test-Path -LiteralPath $profilePath -PathType Leaf)) { continue }
            $profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
            $actualModes = @($profile.arenaModeIds | Sort-Object)
            if ($roleName -eq 'MainArena') {
                ($actualModes -join ',') | Should Be (($expectedModes | Sort-Object) -join ',')
            }
            else {
                $actualModes.Count | Should Be 0
            }
        }
    }

    It '大厅新体验属于OpenWorld，旧Lobby体验标识只兼容映射而不启用Lobby角色' {
        $catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
        $roleSchemaPath = Join-Path (Split-Path $catalogPath) 'server-role.schema.json'
        $roleIds = @(Get-Content -LiteralPath $roleSchemaPath -Raw | ConvertFrom-Json).enum
        $roleMappings = @($catalog.'x-role-experience-map')
        $openWorld = @($roleMappings | Where-Object { $_.serverRole -eq 'GameServer.Role.OpenWorld' })[0]
        $profile = Get-Content -LiteralPath (Join-Path (Join-Path $profileRoot 'OpenWorld') 'server-profile.json') -Raw | ConvertFrom-Json

        ($roleIds -contains 'GameServer.Role.Lobby') | Should Be $false
        (@($openWorld.experiences) -contains 'Experience.OpenWorld.Hub') | Should Be $true
        (@($openWorld.experiences) -contains 'Experience.Lobby.Main') | Should Be $true
        $profile.defaultExperienceId | Should Be 'Experience.OpenWorld.Hub'
        (@($profile.allowedExperienceIds) -contains 'Experience.Lobby.Main') | Should Be $false
    }

    It 'UE世界定义正向测试夹具只使用Shared允许的三角色与体验组合' {
        # 只核验测试输入与真源的一致性；并不冒充UE运行ValidateDefinition。
        $catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
        $testPath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAWorlds/Source/DBAWorldsRuntime/Private/Tests/DivineBeastsWorldDefinitionTests.cpp'
        $testSource = Get-Content -LiteralPath $testPath -Raw
        $pairs = @(Get-WorldContextFixturePairs -Source $testSource)
        $pairs.Count | Should BeGreaterThan 0
        foreach ($pair in $pairs) {
            $pair.Groups['version'].Value | Should Be '1'
            $mapping = @($catalog.'x-role-experience-map' | Where-Object { $_.serverRole -eq $pair.Groups['role'].Value })
            $mapping.Count | Should Be 1
            if ($mapping.Count -eq 1) {
                (@($mapping[0].experiences) -contains $pair.Groups['experience'].Value) | Should Be $true
            }
        }
    }

    It '正向夹具提取不能静默跳过未知体验版本' {
        # 变异只在内存发生，不改UE测试源码；解析器必须将未知版本交给后续断言。
        $fixture = 'const FValidWorldContext ValidContexts[] = { {TEXT("GameServer.Role.OpenWorld"), TEXT("experience.openworld.hub@2")}, };'
        $pairs = @(Get-WorldContextFixturePairs -Source $fixture)
        $pairs.Count | Should Be 1
        $pairs[0].Groups['version'].Value | Should Be '2'
    }
}
