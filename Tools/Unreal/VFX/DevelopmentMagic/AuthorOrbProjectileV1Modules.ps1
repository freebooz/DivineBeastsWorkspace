# 《神兽联盟》OrbProjectile V1：仅创建纯客户端审核Niagara模块；禁止覆盖同名，执行前验证自有UE5.8编辑器。
$ErrorActionPreference='Stop'
$base='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
function CallM([string]$tool,[hashtable]$arg){
 $q=@{jsonrpc='2.0';id=1;method='tools/call';params=@{name=$tool;arguments=$arg}}|ConvertTo-Json -Depth 36 -Compress
 $r=Invoke-RestMethod 'http://127.0.0.1:9317/mcp' -Method Post -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes($q)) -TimeoutSec 130
 if($r.error -or $r.result.isError){throw ('Monolith failed: '+($r|ConvertTo-Json -Depth 10 -Compress))}
 $txt=($r.result.content|Where-Object {$_.type -eq 'text'}|Select-Object -First 1).text
 if(-not $txt){throw 'Monolith empty response'}
 $v=$txt|ConvertFrom-Json -Depth 38
 if($v.error -or ($v.PSObject.Properties.Name -contains 'success' -and $v.success -eq $false) -or ($v.PSObject.Properties.Name -contains 'ok' -and $v.ok -eq $false)){throw ('Monolith returned error: '+$txt)}
 return $v
}
$guard=@'
import unreal,os
assert '-OrbProjectileV1Owned' in unreal.SystemLibrary.get_command_line()
assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
print('ORBP_UE58_OWNER',os.getpid())
'@
$r=CallM 'editor_query' @{action='run_python';params=@{command=$guard;unattended=$true}}
Write-Output ('EDITOR_OWNED '+($r.output|ConvertTo-Json -Depth 6 -Compress))
$motion=@'
#if GPU_SIMULATION
// UI/施法视觉审核层；弹体实际移动由复制/权威Actor驱动。所有坐标为局部cm。
float a=max(0.0,Age);
int i=Index % 2048;
float t=saturate(a/max(Life,0.12));
float grow=saturate(a/.42);
float fade=saturate(a/.065)*saturate((Life-a)/.16);
float angle=i*2.39996323+a*1.65;
float3 p=float3(0,0,0);
float2 size=float2(30,30);
float3 scale=float3(1,1,1);
float3 col=float3(1.88,.47,.07);
float alpha=fade;
float4 meshrot=float4(0,0,0,1);
// F1: 立体火核及环绕球面翻卷火舌；由视觉锚点定位，非第二条弹道。
if(Mode==0){size=float2(26,26)*(.30+.70*grow);col=float3(2.3,1.22,.30);alpha=.85*fade;}
else if(Mode==1){
  float z=2*frac(i*.618033988)-1.0;
  float rad=sqrt(max(0.0,1-z*z));
  p=float3(rad*cos(angle),rad*sin(angle),z)*(16+18*grow);
  size=float2(30+12*grow,39+9*grow);alpha=.74*fade;
}
else if(Mode==2){
  float r=27+6*sin(angle);p=float3(cos(angle)*r,sin(angle)*r,11+20*grow+8*sin(angle*.63));
  size=float2(21,45);alpha=.64*fade;
}
else if(Mode==3){
  float r=17+13*(1-grow);p=float3(cos(angle)*r,sin(angle)*r,5+7*sin(angle*.5));
  size=float2(18,23);alpha=.29*fade;
}
else if(Mode==4){
  p=float3(cos(angle)*(18+25*t),sin(angle)*(18+24*t),15+a*25);
  size=float2(2.8,4);alpha=.35*fade;
}
// I1: 新建厚实多面体冰块主Mesh，辅以晶面凝结、寒雾和少量棱光。
else if(Mode==10){
  scale=float3(.13+.87*grow,.13+.87*grow,.13+.87*grow);
  col=float3(.84,.93,1.0);alpha=1;
  meshrot=float4(0,0,sin(a*.12),cos(a*.12));
}
else if(Mode==11){
  float r=12+21*grow;p=float3(cos(angle)*r,sin(angle)*r,6+9*sin(angle));
  scale=float3(.13,.13,.24)*(.20+.80*grow);col=float3(.67,.87,1);alpha=.78*fade;
}
else if(Mode==12){
  p=float3(cos(angle)*27,sin(angle)*27,5+12*sin(angle));size=float2(29,38);
  col=float3(.72,.86,1);alpha=.30*fade;
}
else if(Mode==13){
  float r=53*(1-grow)+18;p=float3(cos(angle)*r,sin(angle)*r,9+8*cos(angle*1.6));
  size=float2(42,56);col=float3(.35,.73,1);alpha=.22*fade;
}
else if(Mode==14){
  p=float3(cos(angle)*31,sin(angle)*31,8+cos(angle)*17);size=float2(5,6);
  col=float3(1,1.3,1.8);alpha=.50*fade;
}
// F2: 飞行只处理本地绕球火焰，世界轨迹由弹体Component驱动。
else if(Mode==20){size=float2(40,40);col=float3(2.3,1.12,.24);alpha=.85*fade;}
else if(Mode==21){
  float r=23+4*sin(angle);p=float3(-12,cos(angle)*r,sin(angle)*r);
  size=float2(31,45);alpha=.65*fade;
}
else if(Mode==23){
  p=float3(-30-26*t,cos(angle)*19,sin(angle)*19);
  size=float2(20,38);alpha=.28*fade;
}
else if(Mode==24){
  p=float3(-23-33*t,cos(angle)*21,sin(angle)*17-10*a);
  size=float2(3,4);alpha=.48*fade;
}
// I2: 飞行实体块仍是Mesh，冰片/寒雾只用于体积和层次。
else if(Mode==30){
  scale=float3(.76,.76,.76);col=float3(.84,.94,1);alpha=1;
  meshrot=float4(0,0,sin(a*.16),cos(a*.16));
}
else if(Mode==31){
  p=float3(cos(angle)*24,sin(angle)*24,cos(angle*.6)*20);
  size=float2(5,6);col=float3(.85,1.2,1.8);alpha=.46*fade;
}
else if(Mode==33){
  p=float3(-18-28*t,cos(angle)*22,sin(angle)*22);
  size=float2(37,49);col=float3(.38,.76,1);alpha=.22*fade;
}
else if(Mode==34){
  p=float3(-26-36*t,cos(angle)*19,sin(angle)*18);
  scale=float3(.075,.08,.14);col=float3(.63,.86,1);alpha=.75*fade;
}
Particles.Position=p;
Particles.SpriteSize=size;
Particles.SpriteRotation=angle*57.2957795;
Particles.Color=float4(col,alpha);
Particles.Scale=scale;
Particles.MeshOrientation=meshrot;
#endif
'@
$trail=@'
#if GPU_SIMULATION
// Ribbon粒子坐标由World Space的发射器逐次Spawn创建。本Update绝不覆盖历史Particles.Position。
float a=max(0.0,Age);
float t=saturate((Life-a)/max(0.05,Life));
float fade=smoothstep(0.0,.22,t);
float w=max(.05,WidthCm)*pow(t,.74);
float3 col=Flavor==1?float3(1.7,.46,.065):float3(.36,.76,1.0);
Particles.RibbonWidth=w;
Particles.Color=float4(col,.78*fade);
#endif
'@
$cfgs=@(
 @{name='OrbVisualMotion';path="$base/Niagara/Modules/NM_DBA_OrbVisualMotion_Review";hlsl=$motion;inputs=@(@{name='Age';type='float'},@{name='Index';type='int'},@{name='Mode';type='int'},@{name='Life';type='float'})},
 @{name='OrbTrailFade';path="$base/Niagara/Modules/NM_DBA_OrbTrailFade_Review";hlsl=$trail;inputs=@(@{name='Age';type='float'},@{name='Life';type='float'},@{name='WidthCm';type='float'},@{name='Flavor';type='int'})}
)
foreach($c in $cfgs){
 $path=$c.path
 $disk='Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/OrbProjectile/Review/V1/Niagara/Modules/'+(Split-Path $path -Leaf)+'.uasset'
 if(Test-Path $disk){
   Write-Output ('ORBP_EXISTING_OWN_REVIEW_MODULE '+$path)
 }else{
   $verify=@"
import unreal
assert '-OrbProjectileV1Owned' in unreal.SystemLibrary.get_command_line()
assert not unreal.EditorAssetLibrary.does_asset_exist('$path'), '同名审核模块存在，禁止覆盖'
print('ORBP_MODULE_PATH_AVAILABLE','$path')
"@
   $v=CallM 'editor_query' @{action='run_python';params=@{command=$verify;unattended=$true}}
   $out=CallM 'niagara_query' @{action='create_module_from_hlsl';params=@{
      name=$c.name;save_path=$path;hlsl=$c.hlsl;inputs=$c.inputs;outputs=@();
      description='神兽联盟OrbProjectile V1纯视觉审核模块，无伤害、命中或权威运动'}}
   $str=$out|ConvertTo-Json -Depth 8 -Compress
   Write-Output ('ORBP_MODULE_CREATED '+$c.name+' '+$str.Substring(0,[Math]::Min(380,$str.Length)))
 }
 $fix=@"
import unreal
p='$path'
a=unreal.load_asset(p)
assert isinstance(a,unreal.NiagaraScript)
nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith(a.get_path_name()+':')]
assert len(nodes)==1,'模块参数映射节点数不是1'
n=nodes[0]
v=n.get_editor_property('exposure_options')
v.set_editor_property('exposed',True)
v.set_editor_property('required',True)
v.set_editor_property('hidden',False)
n.set_editor_property('exposure_options',v)
assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
print('ORBP_MODULE_NATIVE_SAVED',p)
"@
 $saved=CallM 'editor_query' @{action='run_python';params=@{command=$fix;unattended=$true}}
 Write-Output ('ORBP_MODULE_READY '+$path+' '+($saved.output|ConvertTo-Json -Depth 5 -Compress))
}
