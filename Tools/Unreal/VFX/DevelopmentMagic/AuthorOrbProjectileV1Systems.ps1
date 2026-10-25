# SSLM OrbProjectile V1 实际Niagara审核系统作者：分阶段，只创建未占用的目标Review资产。
param([ValidateSet('FireForm','IceForm','FireFlight','IceFlight')][string]$Stage,[switch]$Resume)
$ErrorActionPreference='Stop'
$Base='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
$Systems="$Base/Niagara/Systems"
$Motion="$Base/Niagara/Modules/NM_DBA_OrbVisualMotion_Review"
$TrailFade="$Base/Niagara/Modules/NM_DBA_OrbTrailFade_Review"
$Template='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal'
function CallM([string]$tool,[hashtable]$arg){
  $json=@{jsonrpc='2.0';id=9;method='tools/call';params=@{name=$tool;arguments=$arg}}|ConvertTo-Json -Depth 55 -Compress
  $r=Invoke-RestMethod 'http://127.0.0.1:9317/mcp' -Method Post -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes($json)) -TimeoutSec 180
  if($r.error -or $r.result.isError){throw ('MCP调用失败: '+($r|ConvertTo-Json -Depth 12 -Compress))}
  $body=($r.result.content|Where-Object {$_.type -eq 'text'}|Select-Object -First 1).text
  if(-not $body){throw 'MCP没有返回正文'}
  $v=$body|ConvertFrom-Json -Depth 40
  if($v.error -or ($v.PSObject.Properties.Name -contains 'success' -and $v.success -eq $false) -or ($v.PSObject.Properties.Name -contains 'ok' -and $v.ok -eq $false)){throw ('MCP操作失败 '+$body)}
  return $v
}
function N([string]$op,[hashtable]$param){return CallM 'niagara_query' @{action=$op;params=$param}}
function E([string]$py){return CallM 'editor_query' @{action='run_python';params=@{command=$py;unattended=$true}}}
$stages=@{
 FireForm=@{name='NS_DBA_FireOrb_Form_Review';layers=@(
  @('OrbCore',0,1,1.1,'core','sprite'), @('OrbShellFlame',1,10,1.1,'tongue','sprite'),
  @('OuterFlameTongues',2,14,1.1,'tongue','sprite'), @('HeatFlow',3,7,1.1,'tongue','sprite'),
  @('EmberSparks',4,6,1.1,'ember','sprite')
 )};
 IceForm=@{name='NS_DBA_IceBlock_Form_Review';layers=@(
  @('IceCoreMesh',10,1,1.4,'crystal','mesh'), @('FacetGrowth',11,7,1.4,'crystal','meshshard'),
  @('FrostWrap',12,9,1.4,'frost','sprite'), @('ColdCondensation',13,12,1.4,'mist','sprite'),
  @('CrystalGlint',14,5,1.4,'frost','sprite')
 )};
 FireFlight=@{name='NS_DBA_FireOrb_FlightTrail_Review';layers=@(
  @('TravelCore',20,1,3.2,'core','sprite'), @('FlightFlames',21,14,3.2,'core','sprite'),
  @('PrimaryFireRibbon',22,1,.27,'ribbon','ribbon'), @('SecondaryWisps',23,7,3.2,'core','sprite'),
  @('TrailingEmbers',24,9,3.2,'ember','sprite')
 )};
 IceFlight=@{name='NS_DBA_IceBlock_FlightTrail_Review';layers=@(
  @('FlyingIceCore',30,1,3.2,'crystal','mesh'), @('FacetShimmer',31,5,3.2,'frost','sprite'),
  @('PrimaryFrostRibbon',32,1,.34,'ribbon','ribbon'), @('ColdMistWake',33,11,3.2,'mist','sprite'),
  @('CrystalShards',34,7,3.2,'crystal','meshshard')
 )}
}
$c=$stages[$Stage]
if(-not $c){throw '阶段配置无效'}
$target="$Systems/$($c.name)"
$guard=@"
import unreal,os
assert '-OrbProjectileV1Owned' in unreal.SystemLibrary.get_command_line(), '禁止操作其他编辑器'
assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.EditorAssetLibrary.does_asset_exist('$target') == ('$Resume'=='True'), '审核系统存在状态和恢复模式不符，拒绝操作'
assert isinstance(unreal.load_asset('$Motion'),unreal.NiagaraScript)
assert isinstance(unreal.load_asset('$TrailFade'),unreal.NiagaraScript)
print('ORB_SYSTEM_PATH_AVAILABLE',os.getpid(),'$target')
"@
$owner=E $guard
Write-Output ('AUTHOR_OWNER '+($owner.output|ConvertTo-Json -Depth 4 -Compress))
$m=@{}
if($Stage -eq 'FireForm'){
 $m=@{core="$Base/Materials/MI_DBA_FireOrb_Form_Core_Review";tongue="$Base/Materials/MI_DBA_FireOrb_Form_Tongues_Review";ember="$Base/Materials/MI_DBA_FireOrb_Form_Ember_Review"}
}
if($Stage -eq 'IceForm'){
 $m=@{crystal="$Base/Materials/MI_DBA_IceBlock_Form_Crystal_Review";frost="$Base/Materials/MI_DBA_IceBlock_Form_Frost_Review";mist="$Base/Materials/MI_DBA_IceBlock_Form_Mist_Review"}
}
if($Stage -eq 'FireFlight'){
 $m=@{core="$Base/Materials/MI_DBA_FireOrb_Flight_Core_Review";ember="$Base/Materials/MI_DBA_FireOrb_Flight_Ember_Review";ribbon="$Base/Materials/MI_DBA_FireOrb_Flight_Ribbon_Review"}
}
if($Stage -eq 'IceFlight'){
 $m=@{crystal="$Base/Materials/MI_DBA_IceBlock_Flight_Crystal_Review";frost="$Base/Materials/MI_DBA_IceBlock_Form_Frost_Review";mist="$Base/Materials/MI_DBA_IceBlock_Flight_Mist_Review";ribbon="$Base/Materials/MI_DBA_IceBlock_Flight_Ribbon_Review"}
}
$emitters=@()
foreach($layer in $c.layers){
 $name=[string]$layer[0];$mode=[int]$layer[1];$qty=[int]$layer[2];$life=[double]$layer[3]
 $key=[string]$layer[4];$rtype=[string]$layer[5]
 $isTrail=($rtype -eq 'ribbon')
 $modules=@()
 if($isTrail){
   $modules+=@{stage='emitter_update';script='/Niagara/Modules/Emitter/SpawnRate.SpawnRate'}
   $modules+=@{stage='particle_update';script=$TrailFade;inputs=@{Life=[string]$life;WidthCm=$(if($Stage -eq 'FireFlight'){'24'}else{'26'});Flavor=$(if($Stage -eq 'FireFlight'){'1'}else{'2'})};bindings=@{Age='Particles.Age'}}
 }else{
   $modules+=@{stage='emitter_update';script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous';inputs=@{'Spawn Count'=[string]$qty}}
   $modules+=@{stage='particle_update';script=$Motion;inputs=@{Mode=[string]$mode;Life=[string]$life};bindings=@{Age='Particles.Age';Index='Particles.UniqueID'}}
 }
 $rend=@()
 if($isTrail){$rend=@(@{class='ribbon';material=$m[$key]})}
 elseif($rtype -eq 'mesh' -or $rtype -eq 'meshshard'){$rend=@(@{class='mesh';material=$m[$key]})}
 $emitters+=@{name=$name;asset=$Template;properties=@{SimTarget='GPUComputeSim';bLocalSpace=$(if($isTrail){'False'}else{'True'})};modules=$modules;renderers=$rend}
}
if(-not $Resume){
 $res=N 'create_system_from_spec' @{save_path=$target;spec=@{emitters=$emitters}}
 Write-Output ('ORBP_CREATED '+$Stage+' '+($res|ConvertTo-Json -Depth 11 -Compress))
}else{Write-Output ('ORBP_RESUMING_OWN_REVIEW '+$target)}
foreach($layer in $c.layers){
 $name=[string]$layer[0];$life=[string]$layer[3];$key=[string]$layer[4];$rtype=[string]$layer[5];$isTrail=($rtype -eq 'ribbon')
 if($isTrail -or $rtype -like 'mesh*'){
   $orig=N 'list_renderers' @{asset_path=$target;emitter=$name}
   foreach($r in @($orig.renderers|Where-Object{$_.type -eq 'sprite'}|Sort-Object index -Descending)){
     N 'remove_renderer' @{asset_path=$target;emitter=$name;renderer_index=[int]$r.index}|Out-Null
   }
   $render=N 'list_renderers' @{asset_path=$target;emitter=$name}
   $kind=if($isTrail){'ribbon'}else{'mesh'}
   $found=@($render.renderers|Where-Object {$_.type -eq $kind})
   if($found.Count -ne 1){throw ('要求的原生渲染器不存在 '+$name+' '+($render|ConvertTo-Json -Depth 8 -Compress))}
   $idx=[int]$found[0].index
   if($isTrail){
      $x=N 'configure_ribbon' @{asset_path=$target;emitter=$name;renderer_index=$idx;preset='trail';tiling_length=90}
      Write-Output ('ORBP_TRUE_RIBBON '+$name+' '+($x|ConvertTo-Json -Depth 6 -Compress))
   }else{
      $mesh=if($rtype -eq 'mesh'){"$Base/Meshes/SM_DBA_IceBlock_FacetedCore_Review"}else{'/DBAPresentationPack_Core/VFX/IceArrow/Meshes/SM_DBA_IceArrow_CrystalShard'}
      $x=N 'set_renderer_mesh' @{asset_path=$target;emitter=$name;renderer_index=$idx;mesh=$mesh}
      Write-Output ('ORBP_TRUE_MESH '+$name+' '+$mesh)
   }
 }else{
   N 'set_renderer_material' @{asset_path=$target;emitter=$name;renderer_index=0;material=$m[$key]}|Out-Null
 }
 N 'set_module_input_value' @{asset_path=$target;emitter=$name;module_node='InitializeParticle';input='Lifetime';value=$life}|Out-Null
 if(-not $isTrail){
  N 'set_emitter_loop_profile' @{asset_path=$target;emitter=$name;loop_behavior='Once';loop_duration=.12}|Out-Null
 }
 N 'set_fixed_bounds' @{asset_path=$target;emitter=$name;min=@(-490,-230,-230);max=@(240,230,255);enabled=$true}|Out-Null
}
N 'set_fixed_bounds' @{asset_path=$target;min=@(-600,-260,-260);max=@(275,260,290);enabled=$true}|Out-Null
if($Stage -match 'Flight'){
 $x=N 'set_effect_type' @{asset_path=$target;effect_type='/DBAPresentationPack_Core/VFX/Library/00_Core/EffectTypes/ProjectileTrail/ET_VFX_ProjectileTrail'}
 Write-Output 'ORBP_REUSED_PROJECTILE_TRAIL_EFFECT_TYPE'
}
$diag=N 'get_system_diagnostics' @{asset_path=$target;compile_first=$true}
Write-Output ('ORBP_UE_NATIVE_COMPILE '+$Stage+' ERROR='+$diag.error_count+' WARNING='+$diag.warning_count)
if($diag.error_count -gt 0){
 foreach($err in ($diag.errors|Select-Object -First 7)){Write-Output ('COMPILE_ERROR '+$err.emitter+' '+$err.message)}
 throw '审核Niagara编译失败，拒绝宣称通过'
}
N 'save_system' @{asset_path=$target;only_if_dirty=$false}|Out-Null
Write-Output ('ORBP_NIAGARA_SAVED '+$target)
