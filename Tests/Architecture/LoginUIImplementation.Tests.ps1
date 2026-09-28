Describe '神兽联盟登录用户界面交付门禁' {
    # 本门禁只验证源码、策略、内容包登记和真实二进制资产是否同时存在。
    # Widget树、父类、编译结果和运行时事件仍须由Monolith与UE自动化独立复核。
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $agentsPath = Join-Path $workspaceRoot 'AGENTS.md'
    $registryPath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json'
    $packRoot = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core'
    $descriptorPath = Join-Path $packRoot 'DBAUIPack_Core.uplugin'
    $manifestPath = Join-Path $packRoot 'Docs/MonolithGenerationManifest.json'
    $rootLayoutAsset = Join-Path $packRoot 'Content/UI/Root/WBP_DBA_UI_RootLayout.uasset'
    $loginAsset = Join-Path $packRoot 'Content/UI/Screens/WBP_DBA_UI_Login.uasset'
    $loginHeaderPath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Login/DivineBeastsLoginScreen.h'
    $loginSourcePath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Screens/Login/DivineBeastsLoginScreen.cpp'
    $uiSubsystemSourcePath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/DivineBeastsUIClientSubsystem.cpp'

    It '总控文档要求神兽联盟UI资产必须通过Monolith MCP创建和修改' {
        $agents = [IO.File]::ReadAllText($agentsPath, [Text.Encoding]::UTF8)
        $agents | Should Match '神兽联盟.*用户界面.*Monolith MCP'
        $agents | Should Match 'Widget Blueprint.*文本占位'
    }

    It '核心UI内容包已经登记且保持纯内容插件边界' {
        $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
        $entry = @($registry.ContentPacks | Where-Object { $_.Name -eq 'DBAUIPack_Core' })
        $entry.Count | Should Be 1
        $entry[0].RelativePath | Should Be 'Presentation/DBAUIPack_Core'

        Test-Path -LiteralPath $descriptorPath -PathType Leaf | Should Be $true
        if (Test-Path -LiteralPath $descriptorPath -PathType Leaf) {
            $descriptor = [IO.File]::ReadAllText($descriptorPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
            $descriptor.CanContainContent | Should Be $true
            # 纯内容插件应直接省略Modules字段，避免空数组被不同JSON读取器解释出歧义。
            ($descriptor.PSObject.Properties.Name -contains 'Modules') | Should Be $false
        }
    }

    It '登录所需RootLayout与Login Widget均为引擎生成资产并有Monolith证据清单' {
        Test-Path -LiteralPath $rootLayoutAsset -PathType Leaf | Should Be $true
        Test-Path -LiteralPath $loginAsset -PathType Leaf | Should Be $true
        Test-Path -LiteralPath $manifestPath -PathType Leaf | Should Be $true
        if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $manifest.CreationTool | Should Be 'Monolith MCP'
            @($manifest.Assets | Where-Object { $_.AssetPath -eq '/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout' }).Count | Should Be 1
            @($manifest.Assets | Where-Object { $_.AssetPath -eq '/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Login' }).Count | Should Be 1
        }
    }

    It '登录页面通过命名控件和事件更新连接ViewModel且不使用Tick轮询' {
        $header = Get-Content -LiteralPath $loginHeaderPath -Raw
        $source = Get-Content -LiteralPath $loginSourcePath -Raw
        foreach ($widgetName in @('AccountInput', 'PasswordInput', 'LoginButton', 'ErrorText', 'BusyIndicator', 'MaintenanceText')) {
            $header | Should Match ([regex]::Escape($widgetName))
        }
        $source | Should Match 'OnClicked\.Add(?:Unique)?Dynamic'
        $source | Should Match 'OnViewStateChanged\.Add(?:Unique)?Dynamic'
        $source | Should Match 'PasswordInput->SetText\(FText::GetEmpty\(\)\)'
        $source | Should Not Match 'NativeTick|Tick\('
        $source | Should Not Match 'FHttpModule|/v1/auth/login'
    }

    It '项目UI子系统从稳定软路径安装默认RootLayout并继续由状态事件驱动页面路由' {
        $source = Get-Content -LiteralPath $uiSubsystemSourcePath -Raw
        $source | Should Match '/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout\.WBP_DBA_UI_RootLayout_C'
        $source | Should Match 'InstallRootLayoutClass'
        $source | Should Match 'SyncPrimaryScreen\(\)'
        $source | Should Not Match 'NativeTick|Tick\('
    }
}
