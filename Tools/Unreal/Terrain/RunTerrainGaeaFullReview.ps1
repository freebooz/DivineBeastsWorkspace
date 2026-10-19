#requires -Version 5.1
<#
.SYNOPSIS
  UE5.8真正地形图层→PCG权重过滤Graph→独立Landscape审核图的受控编辑器执行入口。
.DESCRIPTION
  不停止正在运行的UE Editor/UBT，不覆盖Village、PCG Gold、未批准Gaea源。
  默认DryRun只打印；-Apply先完成Editor真实构建、再创建和回读资产；
  -ValidateOnly仅回读已保存目标，不更新源地图。不会冒充Cook或人工视觉验收。
#>
[CmdletBinding(DefaultParameterSetName='Plan')]
param(
    [Parameter(ParameterSetName='Apply')][switch]$Apply,
    [Parameter(ParameterSetName='Verify')][switch]$ValidateOnly,
    [switch]$ProductionPBR, # 仅独立开发审核地图启用正式高精客户端材质，不覆盖共享Village。
    [string]$EngineRoot # 显式引擎根路径优先；否则读取UE_ROOT，不隐式搜索其他版本。
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if(-not $EngineRoot){$EngineRoot=$env:UE_ROOT}
if(-not $EngineRoot){throw '缺少锁定UE5.8引擎路径：必须提供-EngineRoot或UE_ROOT'}
$versionPath=Join-Path $EngineRoot 'Engine\Build\Build.version'
if(!(Test-Path -LiteralPath $versionPath -PathType Leaf)){throw "UE引擎Build.version不存在：$versionPath"}
$version=Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8 | ConvertFrom-Json
if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '地形原生生产只允许锁定UE5.8，不静默切换引擎'}
$uproject=Join-Path $root 'Game\DivineBeastsArena.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$build=Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logs=Join-Path $root 'Game\Saved\Validation\TerrainGaea\Authoring'
$rawRoot=Join-Path $root 'Game\Plugins\DivineBeasts\ContentPacks\Worlds\DBAWorldPack_Village\SourceArt\Terrain\CalibrationDerived\Raw'
$layers='Grass,ForestSoil,Dirt,Rock,Scree,Sand,Gravel,RoadDirt,FarmSoil,ShoreMud'
$layerRoot='/DBAWorldPack_Village/Terrain/LayerInfos'
$graphRoot='/DBAWorldPack_Village/Terrain/PCG/Graphs'
$map='/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview'
$material='/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10'
if($ProductionPBR){
    $rawRoot=Join-Path $root 'Game\Plugins\DivineBeasts\ContentPacks\Worlds\DBAWorldPack_Village\SourceArt\Terrain\CalibrationDerivedV2\Raw'
    $material='/DBAPresentationPack_Core/Materials/Terrain/MI_DBA_Terrain_Production10_Review'
    $scannedManifest=Join-Path $PSScriptRoot 'TerrainProductionPBRManifest_v1.json'
    if(!(Test-Path -LiteralPath $scannedManifest -PathType Leaf)){
        throw '正式扫描PBR素材尚未提交/校验，禁止用技术校准源伪造正式审核地图'
    }
}
$manifest=Join-Path $rawRoot 'TerrainUEImportRaw_v1.json'
foreach($path in @($uproject,$editor,$build,$manifest)){
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){
        throw "真实工程/UE5.8或R16/R8清单缺失：$path"
    }
}
$contract=(Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 |ConvertFrom-Json)
if($contract.schemaVersion -ne 1 -or $contract.sourceKind -ne 'calibration_only_not_gaea' -or
   $contract.layers.Count -ne 10 -or $contract.mapPath -ne $map -or
   ($ProductionPBR -and $contract.projectionVersion -ne 2)){
    throw 'Terrain导入清单版本/层数/来源/地图身份不一致'
}
Write-Host 'TERRAIN_GAEA_REVIEW_PLAN'
Write-Host (" UE5.8："+$editor)
Write-Host (" 项目："+$uproject)
Write-Host (" 技术样本："+$rawRoot)
Write-Host (" LayerInfo："+$layerRoot)
Write-Host (" PCG Graph："+$graphRoot)
Write-Host (" 独立地图："+$map)
Write-Host (" 材质："+$material+" 生产PBR="+$ProductionPBR)
Write-Host ' 正式Gaea源：尚未提供；本入口仅技术校准，不会迁入正式Village'
if(!$Apply -and !$ValidateOnly){
    Write-Host 'TERRAIN_GAEA_REVIEW_INSPECT_ONLY：没有创建地图、图层或PCGGraph'
    exit 0
}
# 独占Editor窗口的显式制作门禁；不主动结束任何会话。
$running=@(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {($_.Name -in @('UnrealEditor.exe','UnrealEditor-Cmd.exe','dotnet.exe')) -and
                  ($_.CommandLine -match 'UnrealBuildTool|DivineBeastsArena')})
if($running.Count){
    $ids=($running|ForEach-Object {$_.ProcessId}) -join ','
    throw "项目UE Editor或UBT仍在运行（PID=$ids）；本工具没有写入地图/图层。"
}
New-Item -Path $logs -ItemType Directory -Force |Out-Null
function Run-UeCommandlet([string]$name,[string[]]$commandArguments,[string]$marker){
    $mode=if($ValidateOnly){'Verify'}else{'Apply'}
    $log=Join-Path $logs ($name+'_'+$mode+'.log')
    $cmd=@($uproject,('-run='+$name),'-unattended','-nop4','-nosplash','-NoSound',
           ('-abslog='+$log)) + $commandArguments
    if($ValidateOnly){$cmd+= '-ValidateOnly'}
    Write-Host ('TERRAIN_EXECUTE_COMMANDLET='+$name)
    & $editor @cmd
    $exitCode=$LASTEXITCODE
    if($exitCode -ne 0 -or !(Test-Path -LiteralPath $log)){
        throw "Commandlet执行失败（退出码$exitCode）：$log"
    }
    if(!(Select-String -Path $log -Pattern $marker -SimpleMatch -Quiet)){
        throw "UE日志不含成功标记[$marker]，禁止宣称通过：$log"
    }
    Write-Host ("TERRAIN_COMMANDLET_VERIFIED "+$name+" "+$log)
}
function Run-UePython([string]$name,[string]$scriptName,[string]$marker){
    $log=Join-Path $logs ($name+'.log')
    $path=Join-Path $PSScriptRoot $scriptName
    $cmd=@($uproject,('-ExecutePythonScript='+$path.Replace('\','/')),
           '-unattended','-nop4','-nosplash','-NoSound',('-abslog='+$log))
    & $editor @cmd
    $code=$LASTEXITCODE
    if($code -ne 0 -or !(Test-Path -LiteralPath $log) -or
       !(Select-String -Path $log -SimpleMatch -Pattern $marker -Quiet)){
        throw "UE独立回读失败[$marker]：$log"
    }
}
if($Apply){
    # 正式PBR仅编译本轮更改的Terrain作者模块；全量Editor会触发源码版UE的上千引擎
    # Action并重链正在被其它开发者Editor使用的UnrealEditor-Core.dll（LNK1104）。
    # 已有插件演示客户端模块由其独立开发会话负责，本站不改写他人模块或二进制。
    $buildArgs=@('DivineBeastsArenaEditor','Win64','Development',('-Project='+$uproject),
                 '-MaxParallelActions=3','-WaitMutex','-NoHotReloadFromIDE','-NoUBA')
    if($ProductionPBR){
        $buildArgs+='-Module=GamePlatformSurfaceEditor+GamePlatformPCGEditor'
        Write-Host 'TERRAIN_NATIVE_BUILD_SCOPED_TO_SURFACE_AND_PCG_EDITOR_MODULES'
    }
    & $build @buildArgs
    if($LASTEXITCODE -ne 0){throw '地形本轮UE编辑器模块构建失败；无原生资产创建，查看UBT真实日志'}
}
if($ProductionPBR){
    # 仅在独占编辑器窗口按官方清单替换10层原Texture2D；必须保留回退Zip。
    $oldMode=$env:TERRAIN_PBR_UE_MODE
    $oldMaterial=$env:TERRAIN_PRODUCTION_MATERIAL_MODE
    try {
        $env:TERRAIN_PBR_UE_MODE=if($Apply){'apply'}else{'verify'}
        Run-UePython 'TerrainProductionPBRTextures' 'ReimportTerrainProductionPBR.py' 'TERRAIN_PBR_UE_40_TEXTURES_VERIFIED'
        $env:TERRAIN_PRODUCTION_MATERIAL_MODE=if($Apply){'apply'}else{'verify'}
        Run-UePython 'TerrainProductionMaterial' 'AuthorTerrainProductionMaterial.py' 'TERRAIN_PRODUCTION_MATERIAL_UE_VERIFIED'
    }
    finally {
        $env:TERRAIN_PBR_UE_MODE=$oldMode
        $env:TERRAIN_PRODUCTION_MATERIAL_MODE=$oldMaterial
    }
}
Run-UeCommandlet 'GamePlatformSurfaceTerrainLayers' @(
    ('-LayerRoot='+$layerRoot),('-LayerIds='+$layers),
    '-AssetPrefix=LI_DBA_Terrain_','-LayerNamePrefix=LS_'
) 'TERRAIN_LAYER_INFO_CONTRACT_PASS'
Run-UeCommandlet 'GamePlatformPCGTerrainBiomeGraphs' @(
    ('-GraphRoot='+$graphRoot),'-GraphPrefix=PCG_DBA_Terrain_',
    '-BiomeMap=Forest:LS_ForestSoil,Rock:LS_Rock,Shore:LS_ShoreMud,Road:LS_RoadDirt',
    '-WeightThreshold=0.18'
) 'TERRAIN_BIOME_FILTER_GRAPHS_PASS'
$mapMarker=if($ValidateOnly){'TERRAIN_LANDSCAPE_REVIEW_UE_VERIFY_PASS'}else{'TERRAIN_LANDSCAPE_REVIEW_UE_SAVED'}
Run-UeCommandlet 'GamePlatformSurfaceTerrainReviewMap' @(
    ('-RawRoot='+$rawRoot),('-LayerRoot='+$layerRoot),
    ('-LayerIds='+$layers),'-LayerAssetPrefix=LI_DBA_Terrain_',
    '-LayerNamePrefix=LS_',('-MaterialPath='+$material),('-MapPackage='+$map)
) $mapMarker
Run-UePython 'TerrainUEAssets' 'VerifyTerrainMultiLayerUEAssets.py' 'TERRAIN_UE_ASSET_INVENTORY_PASS'
Run-UePython 'TerrainPCGAssets' 'VerifyTerrainPCGReviewAssets.py' 'TERRAIN_PCG_LANDSCAPE_GRAPH_RELOAD_PASS'
if($ProductionPBR){
    # 只在独立审核关卡绑定四张真实Landscape层权重PCG图，正式Village根地图不受影响。
    $previousSceneMode=$env:TERRAIN_PCG_SCENE_MODE
    try {
        $env:TERRAIN_PCG_SCENE_MODE=if($Apply){'apply'}else{'verify'}
        Run-UePython 'TerrainProductionPCGScene' 'AuthorTerrainProductionPCGReview.py' 'TERRAIN_PBR_LANDSCAPE_PCG_FOUR_GRAPHS_BOUND'
    }
    finally {
        $env:TERRAIN_PCG_SCENE_MODE=$previousSceneMode
    }
}
Write-Host ('TERRAIN_GAEA_REVIEW_EDITOR_TARGETS_PASS productionPBR='+$ProductionPBR+' (NOT_COOK_OR_MANUAL_REVIEW)')
