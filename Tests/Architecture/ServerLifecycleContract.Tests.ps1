Describe '服务端生命周期实现与Shared控制面契约' {
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $serverPluginRoot = Join-Path $workspaceRoot 'Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer'
    $dbaServerRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAServer'
    $openApiPath = Join-Path $workspaceRoot 'Shared/Contracts/GamePlatform/OpenAPI/game-server-control.openapi.yaml'

    It 'HTTP提供者实现的四个端点都存在于唯一Shared OpenAPI契约' {
        $provider = Get-Content -LiteralPath (Join-Path $serverPluginRoot 'Source/GamePlatformServer/Private/Server/GamePlatformHttpControlProvider.cpp') -Raw
        $contract = Get-Content -LiteralPath $openApiPath -Raw
        $expectedPaths = @(
            '/internal/v1/gameservers/register',
            '/internal/v1/gameservers/heartbeat',
            '/internal/v1/gameservers/ready',
            '/internal/v1/gameservers/drain'
        )
        foreach ($path in $expectedPaths) {
            ($provider -match [regex]::Escape($path)) | Should Be $true
            ($contract -match [regex]::Escape($path)) | Should Be $true
        }
        foreach ($field in @('clusterId', 'nodeId', 'protocolVersion')) {
            ($provider -match [regex]::Escape('"' + $field + '"')) | Should Be $true
            ($contract -match [regex]::Escape($field + ':')) | Should Be $true
        }
        ([regex]::Matches($provider, 'SendAcceptedPost\(').Count) | Should BeGreaterThan 4
    }

    It '模块启动仅注册提供者，不读取凭据或发起网络请求' {
        $module = Get-Content -LiteralPath (Join-Path $serverPluginRoot 'Source/GamePlatformServer/Private/GamePlatformServer.cpp') -Raw
        $module | Should Match 'RegisterModularFeature'
        $module | Should Match 'UnregisterModularFeature'
        $module | Should Not Match 'ProcessRequest|GAMESERVERCONTROL_INTERNAL_TOKEN|GetEnvironmentVariable'
    }

    It 'Shared契约声明四个生命周期路由的Bearer凭据与实例头' {
        $contract = Get-Content -LiteralPath $openApiPath -Raw
        $contract | Should Match 'securitySchemes:[\s\S]*?GameServerControlBearer:[\s\S]*?scheme: bearer'
        $paths = @(
            '/internal/v1/gameservers/register',
            '/internal/v1/gameservers/heartbeat',
            '/internal/v1/gameservers/ready',
            '/internal/v1/gameservers/drain'
        )
        foreach ($path in $paths) {
            $escapedPath = [regex]::Escape($path)
            $operation = [regex]::Match($contract, "(?ms)^  ${escapedPath}:\r?\n(?<body>.*?)(?=^  /|^components:)")
            $operation.Success | Should Be $true
            $operation.Groups['body'].Value | Should Match 'security:[\s\S]*?GameServerControlBearer'
            $operation.Groups['body'].Value | Should Match 'GameServerIdentityHeader'
            $operation.Groups['body'].Value | Should Match "'401':"
            $operation.Groups['body'].Value | Should Match "'403':"
        }
    }

    It 'DBAServer在服务端已加载世界和必需资源校验后才注册并发布Ready' {
        $bootstrap = Get-Content -LiteralPath (Join-Path $dbaServerRoot 'Source/DBAServer/Private/Server/DivineBeastsServerBootstrapSubsystem.cpp') -Raw
        $profile = Get-Content -LiteralPath (Join-Path $dbaServerRoot 'Source/DBAServer/Private/Server/DivineBeastsServerRoleProfile.cpp') -Raw
        $bootstrap | Should Match 'IsConfiguredWorldValid\(World, Reason\)[\s\S]*?RegisterInstance\(Instance\)'
        $bootstrap | Should Match 'FindMissingRequiredAssets\(MissingAssets\)'
        $bootstrap | Should Match 'IsConfiguredWorldValid\(\*Self->ValidatedWorld\.Get\(\), Reason\)[\s\S]*?MarkReady\(\)'
        $profile | Should Match 'DoesPackageExist\(PackageName\)[\s\S]*?AssetPath\.ResolveObject\(\) == nullptr'
    }

    It 'HTTP与JSON仅是服务器实现私有依赖' {
        $buildRule = Get-Content -LiteralPath (Join-Path $serverPluginRoot 'Source/GamePlatformServer/GamePlatformServer.Build.cs') -Raw
        $buildRule | Should Match 'PrivateDependencyModuleNames[\s\S]*?"HTTP"[\s\S]*?"Json"'
        $publicBlock = [regex]::Match(
            $buildRule,
            'PublicDependencyModuleNames\.AddRange\s*\(new string\[\]\s*\{(?<body>[\s\S]*?)\}\s*\)')
        $publicBlock.Success | Should Be $true
        ($publicBlock.Groups['body'].Value -match '"HTTP"|"Json"') | Should Be $false
    }
}
