Describe 'UE项目三角色目录与Shared契约保持一致' {
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $roleSchemaPath = Join-Path $workspaceRoot 'Shared/Contracts/Games/DivineBeasts/Schemas/server-role.schema.json'
    $experienceSchemaPath = Join-Path $workspaceRoot 'Shared/Contracts/Games/DivineBeasts/Schemas/experience.schema.json'
    $generatedCatalogPath = Join-Path $workspaceRoot 'Shared/Generated/Cpp/Games/DivineBeasts/DivineBeastsCatalog.generated.hpp'
    $runtimeTestPath = Join-Path $workspaceRoot 'Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsRuntime/Private/Tests/DivineBeastsRuntimeTests.cpp'
    $worldBootstrapPath = Join-Path $workspaceRoot 'Game/Source/DivineBeastsArena/Private/Bootstrap/World/DBAFoundationWorldBootstrap.cpp'

    It 'Shared真源只发布OpenWorld、Village、MainArena三角色并保留大厅体验别名' {
        $roles = @(Get-Content -LiteralPath $roleSchemaPath -Raw | ConvertFrom-Json).enum
        $experiences = @(Get-Content -LiteralPath $experienceSchemaPath -Raw | ConvertFrom-Json).enum
        $roles.Count | Should Be 3
        $experiences.Count | Should Be 7
        ($roles -join ',') | Should Be 'GameServer.Role.OpenWorld,GameServer.Role.Village,GameServer.Role.MainArena'
        (@($experiences) -contains 'Experience.OpenWorld.Hub') | Should Be $true
        (@($experiences) -contains 'Experience.Lobby.Main') | Should Be $true
    }

    It '生成目录将大厅体验及兼容标识映射到OpenWorld角色' {
        $generatedCatalog = Get-Content -LiteralPath $generatedCatalogPath -Raw
        $generatedCatalog | Should Not Match 'GameServerRoleLobby'
        $generatedCatalog | Should Match 'ExperienceServerRoleMapping\{"Experience\.OpenWorld\.Hub", "GameServer\.Role\.OpenWorld"\}'
        $generatedCatalog | Should Match 'ExperienceServerRoleMapping\{"Experience\.Lobby\.Main", "GameServer\.Role\.OpenWorld"\}'
        $generatedCatalog | Should Match 'ExperienceLobbyMain = "Experience\.Lobby\.Main"'
    }

    It '项目运行时自动化契约固定三角色并拒绝Lobby服务器角色' {
        $runtimeTest = Get-Content -LiteralPath $runtimeTestPath -Raw
        $runtimeTest | Should Match 'ServerRole count.*3'
        $runtimeTest | Should Match 'OpenWorld valid'
        $runtimeTest | Should Match 'Lobby invalid'
        $runtimeTest | Should Match 'Experience\.OpenWorld\.Hub'
    }

    It '开发专用服务器启动夹具只接受三种正式角色' {
        $worldBootstrap = Get-Content -LiteralPath $worldBootstrapPath -Raw
        $worldBootstrap | Should Not Match 'DevelopmentServerRole\s*!=\s*TEXT\("Lobby"\)'
        $worldBootstrap | Should Match 'DevelopmentServerRole\s*!=\s*TEXT\("Village"\)'
        $worldBootstrap | Should Match 'DevelopmentServerRole\s*!=\s*TEXT\("OpenWorld"\)'
        $worldBootstrap | Should Match 'DevelopmentServerRole\s*!=\s*TEXT\("MainArena"\)'
    }
}
