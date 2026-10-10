# 天气系统三层与端侧门禁：纯静态契约测试，不等价于UE编译或运行中的网络Actor验证。
Describe 'GamePlatformWeather isolation gate' {
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $pluginPath = Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformWeather/GamePlatformWeather.uplugin'
    $runtimeBuild = Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformWeather/Source/GamePlatformWeatherRuntime/GamePlatformWeatherRuntime.Build.cs'
    $clientBuild = Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformWeather/Source/GamePlatformWeatherClient/GamePlatformWeatherClient.Build.cs'

    It 'declares Runtime and ClientOnly modules' {
        $descriptor = Get-Content -LiteralPath $pluginPath -Raw -Encoding UTF8 | ConvertFrom-Json
        @($descriptor.Modules).Count | Should Be 2
        @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformWeatherRuntime' -and $_.Type -eq 'Runtime' }).Count | Should Be 1
        $client = @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformWeatherClient' -and $_.Type -eq 'ClientOnly' })
        $client.Count | Should Be 1
        @($client[0].TargetAllowList) -join ',' | Should Be 'Client,Editor'
    }

    It 'runtime does not link client presentation modules' {
        $source = Get-Content -LiteralPath $runtimeBuild -Raw -Encoding UTF8
        $source | Should Not Match 'GamePlatformSurfaceClient|GamePlatformVFXClient|GamePlatformSFXClient|DivineBeasts'
        $source | Should Match 'GamePlatformData'
    }

    It 'client reuses Surface and Presentation' {
        $source = Get-Content -LiteralPath $clientBuild -Raw -Encoding UTF8
        $source | Should Match 'GamePlatformSurfaceClient'
        $source | Should Match 'GamePlatformPresentationClient'
        $source | Should Match 'GamePlatformWeatherRuntime'
    }

    It 'all targets include Weather and Server excludes Surface' {
        foreach($targetName in @('DivineBeastsArenaClient','DivineBeastsArenaServer','DivineBeastsArenaEditor')){
            $source = Get-Content -LiteralPath (Join-Path $root "Game/Source/$targetName.Target.cs") -Raw -Encoding UTF8
            $source | Should Match 'EnablePlugins.Add\("GamePlatformWeather"\)'
        }
        $server = Get-Content -LiteralPath (Join-Path $root 'Game/Source/DivineBeastsArenaServer.Target.cs') -Raw -Encoding UTF8
        $server | Should Not Match 'EnablePlugins.Add\("GamePlatformSurface"\)'
    }

    It 'DBAWorlds only consumes WeatherRuntime' {
        $source = Get-Content -LiteralPath (Join-Path $root 'Game/Plugins/DivineBeasts/DBAWorlds/Source/DBAWorldsRuntime/DBAWorldsRuntime.Build.cs') -Raw -Encoding UTF8
        $source | Should Match 'GamePlatformWeatherRuntime'
        $source | Should Not Match 'GamePlatformWeatherClient|GamePlatformSurfaceClient'
    }
}
