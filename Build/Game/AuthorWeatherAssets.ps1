# 《神兽联盟》天气真实资源制作唯一工程入口（Windows + UE5.8）
# 不额外建立虚假项目，不生成伪.uasset，不改变正式Village地图。
# 默认 -Apply:false 只预检。带 -Apply 时强制校验Editor二进制与真实主工程，
# 再按MPC命令、Nine Texture、Surface Material、VFX Material、天气定义/审核蓝图及地图顺序执行。
# NiagaraSystem仍需运行Monolith创作+诊断；本脚本不把JSON当作Niagara，也不冒充已完成P5。
param(
    [switch]$Apply,
    [ValidateSet('prepare','texture','surface','vfx-material','blueprint','review-map','vfx-definition','audio')]
    [string]$Phase='prepare',
    [switch]$VerifyAfter
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Project=Join-Path $Root 'Game\DivineBeastsArena.uproject'
$Engine='F:\UnrealEngine-5.8.0-release'
$Editor=Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Scripts=Join-Path $Root 'Tools\Unreal\Weather'
$MpcPath=Join-Path $Root 'Game\Plugins\GamePlatform\Presentation\GamePlatformSurface\Content\ParameterCollections\MPC_GP_SurfaceGlobal.uasset'
$EssentialModules=@(
    (Join-Path $Root 'Game\Plugins\GamePlatform\World\GamePlatformWeather\Binaries\Win64\UnrealEditor-GamePlatformWeatherRuntime.dll'),
    (Join-Path $Root 'Game\Plugins\GamePlatform\World\GamePlatformWeather\Binaries\Win64\UnrealEditor-GamePlatformWeatherClient.dll'),
    (Join-Path $Root 'Game\Plugins\GamePlatform\Presentation\GamePlatformSurface\Binaries\Win64\UnrealEditor-GamePlatformSurfaceEditor.dll'),
    (Join-Path $Root 'Game\Plugins\GamePlatform\OnlineServices\GamePlatformServer\Binaries\Win64\UnrealEditor-GamePlatformServer.dll')
)
if(-not (Test-Path $Project) -or -not (Test-Path $Editor)) {
    throw '正式主工程或锁定UE5.8编辑器程序不存在；不能生成资产。'
}
Write-Output ('WEATHER_PROJECT='+$Project)
Write-Output ('WEATHER_PHASE='+$Phase)
foreach($module in $EssentialModules){
    if(-not (Test-Path $module)){
        Write-Output ('WEATHER_MISSING_EDITOR_MODULE='+$module)
    }
}
if(-not $Apply){
    Write-Output 'WEATHER_AUTHOR_DRY_RUN=YES：未执行Editor，未创建材质/蓝图/Niagara。'
    Write-Output '按GamePlatformSurfaceCoreAssets Commandlet、UE天气作者脚本和Monolith Niagara分阶段真实制作。'
    return
}
foreach($module in $EssentialModules){
    if(-not (Test-Path $module)){throw "编辑器模块不完整，禁止执行或伪造资产：$module"}
}
$Task=Join-Path $Scripts 'AuthorWeatherProductionPipeline.py'
$Verify=Join-Path $Scripts 'VerifyWeatherUnrealAssets.py'
if(-not (Test-Path $Task) -or -not (Test-Path $Verify)){
    throw '天气真实作者脚本或验证脚本缺失'
}
function Invoke-WeatherUE([string[]]$CallArgs,[string]$Title) {
    Write-Output ('START_WEATHER_UE '+$Title)
    & $Editor $Project @CallArgs -unattended -nop4 -nosplash
    $e=$LASTEXITCODE
    Write-Output ('WEATHER_UE_EXIT '+$Title+'='+$e)
    if($e -ne 0){throw "UE真实制作失败：$Title（$e）；请检查Saved/Logs后增量修复，禁止覆盖现有资产"}
}
# 首次生成MPC，若已存在则只做ValidateOnly；避免命令将同名资源当作新资产覆盖。
if(-not (Test-Path $MpcPath)){
    Invoke-WeatherUE @('-run=GamePlatformSurfaceCoreAssets') 'MPC_CREATE'
}
Invoke-WeatherUE @('-run=GamePlatformSurfaceCoreAssets','-ValidateOnly') 'MPC_VALIDATE'
$prev=$env:WEATHER_AUTHOR_PHASE
try{
    $env:WEATHER_AUTHOR_PHASE=$Phase
    $normalized=$Task.Replace('\\','/')
    Invoke-WeatherUE @('-ExecutePythonScript='+$normalized) ('ASSET_'+$Phase)
    if($VerifyAfter){
        $old=$env:WEATHER_ASSET_VERIFY_MODE
        try{
            $env:WEATHER_ASSET_VERIFY_MODE='verify'
            Invoke-WeatherUE @('-ExecutePythonScript='+$Verify.Replace('\\','/')) 'ASSET_VERIFY'
        }finally{
            $env:WEATHER_ASSET_VERIFY_MODE=$old
        }
    }
}finally{
    $env:WEATHER_AUTHOR_PHASE=$prev
}
Write-Output 'WEATHER_AUTHOR_UE_COMMANDS_COMPLETED；Niagara仍需Monolith单独制作和编译验收。'
