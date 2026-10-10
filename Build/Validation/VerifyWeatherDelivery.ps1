# 天气系统集中验证入口；按用户要求最后统一执行，不在每次资产制作脚本中偷偷调用构建。
# 必须明确区分静态合同、C++源码、可加载UE模块、真实Niagara/蓝图资源与联机验收。
param([switch]$RequireEngineAssets)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $Root
try {
    Write-Output '=== WEATHER STATIC SOURCE ==='
    foreach($cmd in @('Tests/Architecture/ValidateWeatherSourceArt.py',
                      'Tests/Architecture/ValidateWeatherAuthoringIntegration.py')){
        & python $cmd
        if($LASTEXITCODE -ne 0){ throw "天气Python静态校验失败：$cmd，exit=$LASTEXITCODE" }
    }
    $Scripts = @(
        'Tools/Unreal/Weather/GenerateWeatherSourceTextures.py',
        'Tools/Unreal/Weather/GenerateWeatherAudio.py',
        'Tools/Unreal/Weather/ImportWeatherSourceArt.py',
        'Tools/Unreal/Weather/AuthorWeatherSurfaceAssets.py',
        'Tools/Unreal/Weather/AuthorWeatherVFXMaterials.py',
        'Tools/Unreal/Weather/AuthorWeatherBlueprintAssets.py',
        'Tools/Unreal/Weather/AuthorWeatherVFXDefinitions.py',
        'Tools/Unreal/Weather/AuthorWeatherAudioAssets.py',
        'Tools/Unreal/Weather/AuthorWeatherReviewMap.py',
        'Tools/Unreal/Weather/GenerateWeatherMonolithSpecs.py',
        'Tools/Unreal/Weather/AuthorWeatherProductionPipeline.py',
        'Tools/Unreal/Weather/VerifyWeatherUnrealAssets.py'
    )
    & python -m py_compile @Scripts
    if($LASTEXITCODE -ne 0){throw "天气Python源码语法有错误"}
    Write-Output 'WEATHER_PYTHON_SYNTAX=PASSED'

    Write-Output '=== WEATHER ARCHITECTURE ==='
    Import-Module '.\Tests\Architecture\DesignBaselineAudit.psm1' -Force
    $baseline=Test-DesignBaselineWorkspace -WorkspaceRoot $Root
    Write-Output ("WEATHER_BASELINE="+$baseline.Passed+", CODE="+$baseline.PluginCounts.Baseline)
    if(-not $baseline.Passed){ $baseline.Errors | Select-Object -First 12;throw '天气架构基线失败' }
    Import-Module Pester -ErrorAction Stop
    $tests = Invoke-Pester -Path @(
        '.\Tests\Architecture\DesignBaselineAudit.Tests.ps1',
        '.\Tests\Architecture\GamePlatformWeatherIsolation.Tests.ps1'
    ) -PassThru
    Write-Output ("WEATHER_PESTER_PASS="+$tests.PassedCount+", FAIL="+$tests.FailedCount)
    if($tests.FailedCount -gt 0){throw 'Pester静态架构回归失败'}

    Write-Output '=== WEATHER ENGINE GATE ==='
    # 本引擎资产门禁既要检查真实可加载模块，还要检查二进制内容资源存在，
    # 不能仅因GPU或SourceArt源图片存在就标记天气P5完成。
    $requiredPackages = @(
        'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/ParameterCollections/MPC_GP_SurfaceGlobal.uasset',
        'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Materials/M_GP_Surface_Master.uasset',
        'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Materials/M_GP_Surface_Lite.uasset',
        'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Weather/Niagara/NS_GP_Weather_Rain.uasset',
        'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Weather/Niagara/NS_GP_Weather_Snow.uasset',
        'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Weather/Definitions/DA_DBA_Weather_Clear.uasset',
        'Game/Content/Development/Weather/Blueprints/BP_DBA_WeatherReviewController.uasset',
        'Game/Content/Development/Weather/Blueprints/BP_DBA_WeatherReviewGameMode.uasset',
        'Game/Content/Development/Weather/Maps/L_DBA_WeatherReview.umap'
    )
    $missingPackages=@($requiredPackages | Where-Object {-not (Test-Path $_)})
    Write-Output ('WEATHER_P5_BINARY_PACKAGES_PRESENT='+($requiredPackages.Count-$missingPackages.Count)+'/'+$requiredPackages.Count)
    foreach($package in $missingPackages) {Write-Output ('MISSING_WEATHER_ASSET '+$package)}
    if($RequireEngineAssets -and $missingPackages.Count -gt 0) {throw '天气真实二进制资源尚未制作，不得声明P5完成'}

    $requiredModules = @(
        'Game/Plugins/GamePlatform/World/GamePlatformWeather/Binaries/Win64/UnrealEditor-GamePlatformWeatherRuntime.dll',
        'Game/Plugins/GamePlatform/World/GamePlatformWeather/Binaries/Win64/UnrealEditor-GamePlatformWeatherClient.dll',
        'Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Binaries/Win64/UnrealEditor-GamePlatformServer.dll'
    )
    $notFound=@($requiredModules | Where-Object {-not (Test-Path $_)})
    if($notFound.Count -gt 0){
        Write-Output 'WEATHER_ENGINE_GATE=BLOCKED_MISSING_MODULES'
        $notFound | ForEach-Object {Write-Output ('MISSING_MODULE '+$_)}
        if($RequireEngineAssets){exit 3}
        Write-Output 'WEATHER_STATIC_STAGE=PASSED ENGINE_STAGE=BLOCKED'
    } else {
        Write-Output 'WEATHER_ENGINE_MODULE_FILES_PRESENT=TRUE (尚需UE实际加载/资产测试)'
        Write-Output 'WEATHER_ENGINE_GATE=PENDING_REAL_EDITOR'
        if($RequireEngineAssets){exit 3}
    }
} finally {
    Pop-Location
}
