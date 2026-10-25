# 《神兽联盟》火球V3 P1：单独的双手前摇审核资源，不覆盖正式Niagara、GAS或地图。
# 默认只读；仅-Apply显式写新审核资产。原生Niagara接口失败时立即停止。
param([switch]$Apply)
$ErrorActionPreference='Stop'
$root='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara'
$mod="$root/NM_DBA_FireballCast_V3Review"
$sys="$root/NS_DBA_FireballCast_V3Review"
function MCP([string]$tool,[hashtable]$arg){
 $q=@{jsonrpc='2.0';id=1;method='tools/call';params=@{name=$tool;arguments=$arg}}|ConvertTo-Json -Depth 40 -Compress
 $r=Invoke-RestMethod -Uri 'http://127.0.0.1:9316/mcp' -Method Post -Body ([Text.Encoding]::UTF8.GetBytes($q)) -ContentType 'application/json' -TimeoutSec 120
 if($r.error -or $r.result.isError){throw ('MCP错误: '+($r|ConvertTo-Json -Depth 16 -Compress))}
 $c=$r.result.content|Where-Object {$_.type -eq 'text'}|Select-Object -First 1
 if(-not $c){throw 'MCP没有返回文本'}
 $v=$c.text|ConvertFrom-Json -Depth 40
 if($v.error -or ($null -ne $v.success -and $v.success -eq $false) -or ($null -ne $v.ok -and $v.ok -eq $false)){throw ('MCP接口失败: '+$c.text)}
 return $v
}
function Ed([string]$c){MCP 'editor_query' @{action='run_python';params=@{command=$c;unattended=$true}}}
function Ng([string]$a,[hashtable]$p){MCP 'niagara_query' @{action=$a;params=$p}}
$check=@"
import unreal
assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
assert 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
for p in ['$mod','$sys']: print('REVIEW_EXISTS',p,unreal.EditorAssetLibrary.does_asset_exist(p))
"@
Ed $check|Out-Null
if(-not $Apply){Write-Output 'P1_INSPECT_ONLY';exit 0}
$guard=@"
import unreal
for p in ['$mod','$sys']:
 if unreal.EditorAssetLibrary.does_asset_exist(p): raise RuntimeError('不能覆盖已有审核资产：'+p)
assert isinstance(unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Niagara/NS_DBA_DevFireballCast'),unreal.NiagaraSystem)
assert isinstance(unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow'),unreal.Material)
print('ORIGINALS_REMAIN_UNTOUCHED')
"@
Ed $guard|Out-Null
$hlsl=@'
#if GPU_SIMULATION
// 使用已绑定的自定义输入Index；UE5.8的CustomHLSL解析器要求Index后有空格再接%运算符。
int uid = Index % 1024;
// 单一系统的双掌代理位置、环绕火焰、掌心汇聚、白金高温核心。客户端纯外观，不执行伤害。
float t=saturate(Age/0.70);
float s=t*t*(3-2*t);
float angle=uid*2.399963+Age*8.5;
float3 L=float3(23,-34,10);
float3 R=float3(23,34,10);
float3 C=float3(56,0,18);
float3 p=C;
float w=16;
float a=0;
float3 tint=float3(2.3,0.6,0.07);
if(Mode==0){p=(uid % 2)==0?L:R;w=18+18*s;a=(0.55+0.15*s)*(1-0.75*s);}
if(Mode==1){
 float3 palm=(uid % 2)==0?L:R;float r=15+9*s;
 p=palm+float3(cos(angle)*r,sin(angle)*r,sin(angle*0.73)*r*0.46);
 w=11+6*s;a=0.4*(1-0.6*s);
}
if(Mode==2){
 float3 palm=(uid % 2)==0?L:R;
 float u=saturate((t-0.15)/0.62);float r=11*(1-u);
 p=lerp(palm,C,u)+float3(0,cos(angle+7*u)*r,sin(angle+7*u)*r);
 w=12-5*u;a=saturate((t-0.08)*3.5)*(1-u)*0.5;
}
if(Mode==3){
 float u=saturate((t-0.3)/0.7);p=C;w=13+73*u*u*(3-2*u);a=0.84*u;
 tint=lerp(float3(2.1,0.46,0.05),float3(2.7,1.12,0.22),u);
}
Particles.Position=p;
Particles.Color=float4(tint,a);
Particles.SpriteSize=float2(w,w);
Particles.SpriteRotation=angle;
#endif
'@
Ng 'create_module_from_hlsl' @{name='DBAFireballCastV3Review';save_path=$mod;hlsl=$hlsl;inputs=@(@{name='Age';type='float'},@{name='Index';type='int'},@{name='Mode';type='int'});outputs=@();description='SSLM火球P1双掌火焰、旋流与中心汇聚'}|Out-Null
# 与冰箭作者入口一致：参数映射必须由调用栈传入，隐藏的可选InputMap会导致输出引脚编译失败。
Ed "import unreal`na=unreal.load_asset('$mod')`nnodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith(a.get_path_name()+':')]`nassert len(nodes)==1`no=nodes[0].get_editor_property('exposure_options')`no.set_editor_property('exposed',True)`no.set_editor_property('required',True)`no.set_editor_property('hidden',False)`nnodes[0].set_editor_property('exposure_options',o)`nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)"|Out-Null
$spec=@(@{name='Palms';mode=0;count=2},@{name='PalmOrbits';mode=1;count=24},@{name='ConvergeStreams';mode=2;count=18},@{name='ChargeCore';mode=3;count=1})
$emitters=@()
foreach($e in $spec){
 $emitters+=@{name=$e.name;asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal';properties=@{SimTarget='GPUComputeSim';bLocalSpace='True'};modules=@(
   @{stage='emitter_update';script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous';inputs=@{'Spawn Count'=[string]$e.count}},
   @{stage='particle_spawn';script=$mod;bindings=@{Index='Particles.UniqueID'};inputs=@{Mode=[string]$e.mode;Age='0'}},
   @{stage='particle_update';script=$mod;bindings=@{Age='Particles.Age';Index='Particles.UniqueID'};inputs=@{Mode=[string]$e.mode}}
 )}
}
Ng 'create_system_from_spec' @{save_path=$sys;spec=@{emitters=$emitters}}|Out-Null
foreach($e in $spec){
 Ng 'set_renderer_material' @{asset_path=$sys;emitter=$e.name;renderer_index=0;material='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow'}|Out-Null
 Ng 'set_module_input_value' @{asset_path=$sys;emitter=$e.name;module_node='InitializeParticle';input='Lifetime';value='0.78'}|Out-Null
 Ng 'set_emitter_loop_profile' @{asset_path=$sys;emitter=$e.name;loop_behavior='Once';loop_duration=0.12}|Out-Null
 Ng 'set_fixed_bounds' @{asset_path=$sys;emitter=$e.name;min=@(-190,-190,-160);max=@(190,190,170);enabled=$true}|Out-Null
}
Ng 'set_fixed_bounds' @{asset_path=$sys;min=@(-190,-190,-160);max=@(190,190,170);enabled=$true}|Out-Null
$d=Ng 'get_system_diagnostics' @{asset_path=$sys;compile_first=$true}
Write-Output ('P1_DIAGNOSTICS '+($d|ConvertTo-Json -Depth 15 -Compress))
if($d.error_count -or $d.warning_count){throw 'Niagara编译不干净'}
Ng 'save_system' @{asset_path=$sys;only_if_dirty=$false}|Out-Null
Write-Output ('P1_REVIEW_SYSTEM_SAVED '+$sys)
