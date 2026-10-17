#requires -Version 5.1
<#
.SYNOPSIS
SSLM三个世界的PCG资产与注册覆盖率检查（仅静态，不代替UE生成验收）。
.DESCRIPTION
验证真实UE文件头、PCG模板/蓝图/地图文件和内容包注册。只写入Saved中的JSON证据。
不修改或创建任何.uasset/.umap，不打开正式地图，不改变服务器运行时生成策略。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot,[switch]$RequireProductionReady,[string]$ReportPath)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {$WorkspaceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path}
$root=(Resolve-Path -LiteralPath $WorkspaceRoot).Path
$registryPath=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json'
$uprojectPath=Join-Path $root 'Game/DivineBeastsArena.uproject'
if (!(Test-Path -LiteralPath $registryPath -PathType Leaf) -or !(Test-Path -LiteralPath $uprojectPath -PathType Leaf)) {throw 'SSLM唯一正式工程或内容包注册表不存在。'}
$registry=(Get-Content -LiteralPath $registryPath -Raw -Encoding UTF8|ConvertFrom-Json).ContentPacks
$plugins=(Get-Content -LiteralPath $uprojectPath -Raw -Encoding UTF8|ConvertFrom-Json).Plugins
$names=@($registry|ForEach-Object{$_.Name})
$packages=[System.Collections.Generic.List[object]]::new()
$gaps=[System.Collections.Generic.List[string]]::new()
function Add-Package {
  param([string]$Role,[string]$Path)
  $full=Join-Path $root $Path
  $status='Missing';$bytes=0L
  if (Test-Path -LiteralPath $full -PathType Leaf) {
    $f=[IO.File]::OpenRead($full)
    try {
      $bytes=$f.Length;$magic=[byte[]]::new(4);$n=$f.Read($magic,0,4)
      # UE PACKAGE_FILE_TAG：磁盘小端C1-83-2A-9E；包头有效不证明UE资产已加载。
      $status=if($n -eq 4 -and [BitConverter]::ToString($magic) -eq 'C1-83-2A-9E' -and $bytes -gt 64) {'UEPackageHeaderPresent'} else {'InvalidUEPackageHeader'}
    } finally {$f.Dispose()}
  }
  [void]$script:packages.Add([pscustomobject]@{Role=$Role;Path=$Path;State=$status;Bytes=$bytes})
  if ($status -ne 'UEPackageHeaderPresent') {[void]$script:gaps.Add("UE包缺失或损坏：$Path")}
}
function Add-Set {
  param([string]$Role,[string]$Dir,[string[]]$Names)
  foreach($n in $Names){Add-Package -Role $Role -Path "$Dir/$n.uasset"}
}
$f='Game/Content/Development/Foundation/PCG'
Add-Set 'Foundation.Template' "$f/Templates" @('TPL_Base','TPL_ScatterSurface','TPL_BiomeGenerator','TPL_LinearDresser','TPL_Enclosure','TPL_EnclosureClosed','TPL_Connector','TPL_GateInsert','TPL_ParcelFill','TPL_CropField','TPL_AssemblySpawn','TPL_InterfaceBand')
Add-Set 'Foundation.Subgraph' "$f/Subgraphs" @('SG_ProjectOnLandscape','SG_PriorityCarve','SG_ApplySpawnPolicy','SG_AssignMeshSet','SG_FitPostsToSpline','SG_BreakByIntersection','SG_WriteClosedExclude')
Add-Set 'Foundation.Definition' "$f/Definitions" @('DA_PCGGold_Biome','DA_PCGGold_Connector','DA_PCGGold_Crop','DA_PCGGold_Enclosure','DA_PCGGold_Exclusion','DA_PCGGold_Exec','DA_PCGGold_LayerCanopy','DA_PCGGold_LayerCrop','DA_PCGGold_LayerFloor','DA_PCGGold_MeshCanopy','DA_PCGGold_MeshCrop','DA_PCGGold_MeshFence','DA_PCGGold_Parcel','DA_PCGGold_PolicyCrop','DA_PCGGold_PolicyForest','DA_PCGGold_Priority','DA_PCGGold_Road')
Add-Set 'Foundation.Realized' "$f/Realized" @('PCG_Gold_Canopy','PCG_Gold_Crop','PCG_Gold_Rock')
Add-Set 'Foundation.Profile' "$f/Profiles" @('DA_PCGGold_Canopy','DA_PCGGold_Crop','DA_PCGGold_Rock')
Add-Package 'Foundation.GoldMap' "$f/Validation/PCG_GoldLevel_M1.umap"
$v='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content'
Add-Package 'Village.Map' "$v/Maps/L_Village_Start.umap"
Add-Set 'Village.PlacementBP' "$v/PCG/Blueprints" @('BP_PCG_Bridge','BP_PCG_Building','BP_PCG_Crops','BP_PCG_Exclusion','BP_PCG_Field','BP_PCG_Forest','BP_PCG_Resource','BP_PCG_Road','BP_PCG_Rock','BP_PCG_Spawn','BP_PCG_WaterBank')
Add-Set 'Village.TerrainBP' "$v/Terrain/PCG/Blueprints" @('BP_DBA_Terrain_ForestBiome','BP_DBA_Terrain_RoadBiome','BP_DBA_Terrain_RockBiome','BP_DBA_Terrain_ShoreBiome')
Add-Set 'Village.TerrainGraph' "$v/Terrain/PCG/Graphs" @('PCG_DBA_Terrain_ForestWeightFiltered','PCG_DBA_Terrain_LandscapeSemanticSampling','PCG_DBA_Terrain_RoadWeightFiltered','PCG_DBA_Terrain_RockWeightFiltered','PCG_DBA_Terrain_ShoreWeightFiltered')
Add-Set 'Village.ForestSourceGraph' "$v/PCG/Graphs" @('PCG_DBA_Village_OuterTropicalCanopy_Source','PCG_DBA_Village_OuterTropicalUnderstory_Source','PCG_DBA_Village_OuterTropicalFloor_Source')
$a='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Maps/L_DBA_MainArena.umap'
Add-Package 'MainArena.Map' $a
$pcgEnabled=@($plugins|Where-Object{$_.Name -eq 'GamePlatformPCG' -and $_.Enabled -eq $true}).Count -eq 1
$hasVillage=$names -contains 'DBAWorldPack_Village'
$hasArena=$names -contains 'DBAWorldPack_MainArena'
$hasOpenWorld=$names -contains 'DBAWorldPack_OpenWorld'
if(!$pcgEnabled){[void]$gaps.Add('正式工程未启用平台PCG插件')}
if(!$hasVillage){[void]$gaps.Add('Village内容包未注册')}
if(!$hasArena){[void]$gaps.Add('MainArena内容包未注册')}
if(!$hasOpenWorld){[void]$gaps.Add('OpenWorld正式内容包缺失，不能证明整个游戏的PCG覆盖')}
# 外围森林三张源点图不等于正式网格Spawner图，也不代表完成材质、树木和碰撞审查。
$forestRealized=@(Get-ChildItem -LiteralPath (Join-Path $root "$v/PCG/Graphs") -File -Filter '*Realized*.uasset' -ErrorAction SilentlyContinue)
if($forestRealized.Count -eq 0){[void]$gaps.Add('Village外围森林未发现正式Realized/Spawner图，源点图尚未形成完整森林')}
$pending=@(
  'UE5.8 AssetRegistry：蓝图父类/图节点/软硬引用及真实地图实例',
  'Village正式MeshSet与森林农田路网水岸建筑放置器绑定',
  'Village正式地图静态生成、保存和独立冷重载',
  'OpenWorld真实内容包/地图及按设计完成生态图',
  'MainArena静态场景与可选PCG装饰的性能边界',
  'Gold Level G01-G16、确定性与空间冲突实测',
  'WorldPartition/HLOD/NavMesh/碰撞审查',
  'Client/Server独立Cook及专服装饰资源隔离',
  'CPU/GPU耗时、实例数、DrawCall和人工视觉验收'
)
$report=[ordered]@{
  SchemaVersion=1;GeneratedAt=(Get-Date).ToString('o')
  InspectionType='FilesystemAndRegistryOnly';WorkspaceRoot=$root;PCGPluginEnabled=$pcgEnabled
  WorldRoles=@(
    [ordered]@{Name='Village';Registered=$hasVillage;PCGStatus='SourceGraphsAndBlueprintsPresent_BakeUnverified'},
    [ordered]@{Name='OpenWorld';Registered=$hasOpenWorld;PCGStatus='FormalWorldContentNotFound'},
    [ordered]@{Name='MainArena';Registered=$hasArena;PCGStatus='MapPresent_PCGOptional'}
  )
  AssetFacts=$packages.ToArray()
  BlockingGaps=$gaps.ToArray()
  PendingUEAcceptance=$pending
  ProductionReady=$false
}
$savedRoot=Join-Path $root 'Saved/Validation/GamePlatformPCG'
if([string]::IsNullOrWhiteSpace($ReportPath)){$ReportPath=Join-Path $savedRoot 'PCGWholeGameCoverage.json'}
$full=[IO.Path]::GetFullPath($ReportPath)
$prefix=[IO.Path]::GetFullPath($savedRoot).TrimEnd('\')+'\'
if(!$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'JSON报告路径只能位于工作空间Saved/Validation/GamePlatformPCG目录'}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($full))|Out-Null
[IO.File]::WriteAllText($full,($report|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
$good=@($packages|Where-Object{$_.State -eq 'UEPackageHeaderPresent'}).Count
Write-Output "PCG_GAME_COVERAGE_INVENTORY_DONE packages=$good/$($packages.Count) gaps=$($gaps.Count)"
Write-Output "PCG_COVERAGE_REPORT=$full"
$gaps|ForEach-Object{Write-Output "PCG_GAP: $_"}
Write-Output '注意：仅验证磁盘文件头/注册，不代替UE编辑器AssetRegistry、Generate、Cook或性能实测。'
if($RequireProductionReady){Write-Output 'PCG_PRODUCTION_GATE_BLOCKED: 真实UE验收证据缺失';exit 2}
exit 0
