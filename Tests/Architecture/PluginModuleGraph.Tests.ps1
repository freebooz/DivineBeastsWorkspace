Describe '插件模块依赖图完整性' {
    $workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $gameRoot = Join-Path $workspaceRoot 'Game'

    function Get-LocalModuleDependencies {
        param([Parameter(Mandatory)][string]$Path)

        $source = Get-Content -LiteralPath $Path -Raw
        $dependencies = New-Object 'System.Collections.Generic.List[string]'
        $blockPattern = '(?:Public|Private|DynamicallyLoaded)DependencyModuleNames\.Add(?:Range)?\s*\((?<body>.*?)\)\s*;'
        foreach ($block in [System.Text.RegularExpressions.Regex]::Matches(
            $source,
            $blockPattern,
            [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
            foreach ($dependency in [System.Text.RegularExpressions.Regex]::Matches(
                $block.Groups['body'].Value,
                '"([A-Za-z0-9_]+)"')) {
                $dependencies.Add($dependency.Groups[1].Value)
            }
        }
        return @($dependencies | Select-Object -Unique)
    }

    It '所有GamePlatform、Moba和DivineBeasts模块依赖均解析到真实构建规则' {
        $buildRules = @(Get-ChildItem -LiteralPath $gameRoot -Filter '*.Build.cs' -File -Recurse)
        $moduleNames = @($buildRules | ForEach-Object { $_.Name -replace '\.Build\.cs$', '' })
        $unresolved = New-Object 'System.Collections.Generic.List[string]'

        foreach ($buildRule in $buildRules) {
            foreach ($dependency in (Get-LocalModuleDependencies -Path $buildRule.FullName)) {
                if ($dependency -match '^(GamePlatform|Moba|DivineBeasts|DBA)' -and
                    $moduleNames -notcontains $dependency) {
                    $moduleName = $buildRule.Name -replace '\.Build\.cs$', ''
                    $unresolved.Add("$moduleName -> $dependency")
                }
            }
        }

        ($unresolved -join "`n") | Should Be ''
    }

    It '本地插件模块依赖图不存在循环' {
        $buildRules = @(Get-ChildItem -LiteralPath $gameRoot -Filter '*.Build.cs' -File -Recurse)
        $moduleNames = @($buildRules | ForEach-Object { $_.Name -replace '\.Build\.cs$', '' })
        $dependenciesByModule = @{}
        foreach ($buildRule in $buildRules) {
            $moduleName = $buildRule.Name -replace '\.Build\.cs$', ''
            $dependenciesByModule[$moduleName] = @(
                Get-LocalModuleDependencies -Path $buildRule.FullName |
                    Where-Object { $moduleNames -contains $_ } |
                    Select-Object -Unique
            )
        }

        $incomingCount = @{}
        foreach ($moduleName in $moduleNames) { $incomingCount[$moduleName] = 0 }
        foreach ($moduleName in $moduleNames) {
            foreach ($dependency in $dependenciesByModule[$moduleName]) {
                $incomingCount[$dependency]++
            }
        }

        $ready = New-Object 'System.Collections.Generic.Queue[string]'
        foreach ($moduleName in $moduleNames) {
            if ($incomingCount[$moduleName] -eq 0) { $ready.Enqueue($moduleName) }
        }
        $visited = 0
        while ($ready.Count -gt 0) {
            $moduleName = $ready.Dequeue()
            $visited++
            foreach ($dependency in $dependenciesByModule[$moduleName]) {
                $incomingCount[$dependency]--
                if ($incomingCount[$dependency] -eq 0) { $ready.Enqueue($dependency) }
            }
        }

        $visited | Should Be $moduleNames.Count
    }
}
