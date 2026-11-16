# -*- coding: utf-8 -*-
"""SSLM P1-01/02 冰霜双掌通用聚能：导入原创RGBA技术纹理，派生高质量寒雾/核心/冰片材质，
并从公共FireMage可用CPU Niagara克隆两套项目审核系统、重绑定单一向心旋涡算法。
仅在 Review/V1 目录创建新.uasset，不覆盖原有素材、正式资产、服务器施法逻辑或地图。
原生编译/PIE视觉人工审核另行执行；命令行Null RHI存盘不代表画面通过。
"""
from pathlib import Path
import json,unreal,hashlib,traceback
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/PalmElementGather/Review/V1'
BASE='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1'
ICE='/DBAPresentationPack_Core/VFX/IceArrow'
FIRE='/DBAPresentationPack_Core/VFX/Library/Game/FireMage'
SOURCE_SYS=FIRE+'/Niagara/NS_PY_Charge'
SOURCE_COMPAT='/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5'
MODULE=BASE+'/Niagara/NM_DBA_PalmGatherMotion_Review'
SYS={'01':BASE+'/Niagara/NS_DBA_PalmIce_P101_GatherReview',
     '02':BASE+'/Niagara/NS_DBA_PalmIce_P102_ChargeReview'}
MATERIALS={
 '01':{'Mist':BASE+'/Materials/MI_DBA_PalmIce_P101_MistReview',
       'Crystal':BASE+'/Materials/MI_DBA_PalmIce_P101_CrystalReview'},
 '02':{'Mist':BASE+'/Materials/MI_DBA_PalmIce_P102_MistReview',
       'Crystal':BASE+'/Materials/MI_DBA_PalmIce_P102_CrystalReview'}}
TEX={
 'MistSoft':'T_DBA_PalmIce_MistSoft_Review',
 'SpiralFlow':'T_DBA_PalmIce_SpiralFlow_Review',
 'Core':'T_DBA_PalmIce_Core_Review',
 'CrystalSeed':'T_DBA_PalmIce_CrystalSeed_Review',
 'MistFlipbook':'T_DBA_PalmIce_MistFlipbook_4x4_Review',
 'SpiralFlipbook':'T_DBA_PalmIce_SpiralFlipbook_4x4_Review',
}
REPORT=ROOT/'Saved/Validation/PalmIceV1/NativeAuthoringResult.json'
RESULT={'project':unreal.Paths.get_project_file_path(),'engine':unreal.SystemLibrary.get_engine_version(),
        'mode':'only-create-PalmIce-review','assetsCreated':[],'existingAssetsUntouched':True,
        'serverTimingsModified':False,'gameplayModified':False,'manualAccepted':False,
        'PIEVisualVerified':False,'gpuCompileVerified':False,'status':'begin','errors':[]}
HLSL=r"""
// SSLM 双掌元素聚能公共审核运动函数：仅运动和粒子形态，不含火/冰玩法属性。
// 复用既有Spawn/Update接口 SpellTime(粒子秒), ParticleIndex(稳定粒子身份), Layer(发射器层)。
float t=max(0.0,SpellTime);
float id=(float)ParticleIndex;
float h=sin(id*127.1+37.7)*43758.5453;
float v=h-floor(h);
float h2=sin(id*311.7+81.9)*21563.3431;
float q=h2-floor(h2);
float a=id*2.39996323;
float progress=saturate(t/.80);
float spiral=a+progress*(9.0+v*3.0);
float r0=21.0+8.0*v;
float rad=3.4+(r0-3.4)*pow(1.0-progress,1.23);
float3 p=float3(0.0,rad*cos(spiral),rad*sin(spiral));
float2 size=float2(34.0,43.0);
float3 rgb=float3(0.32,0.67,1.0);
float alpha=.65;
float rot=a*57.2958+t*70.0;
if(Layer==10) {
 // 中央冷核+环绕寒雾：粒子0固定掌心，其余为向心螺旋与流动气雾。
 if (id<1.0) {
   p=float3(0.0,0.0,0.0);
   size=float2(44.0,44.0)*(.45+.55*progress);
   alpha=.70*saturate(t/.08);
   rgb=float3(.76,.91,1.0);
 } else {
   p.x=5.0*(1.0-progress);
   p.z+=8.0*(1.0-progress);
   size=float2(36.0+16.0*v,40.0+18.0*q)*(.7+.28*progress);
   alpha=(.40+.12*q)*saturate(t/.09)*saturate((1.12-t)/.25);
   rgb=float3(.35,.68,1.0);
 }
}
if(Layer==11) {
 // 冰屑沿同一涡流归心，后期速度衰减、晶体有短暂可读的稳定阶段。
 p*=1.15;
 p.x+=4.0*q;
 size=(4.2+5.0*v).xx*(.67+.55*progress);
 alpha=(.66+.22*q)*saturate(t/.07)*saturate((1.15-t)/.22);
 rgb=float3(.65,.87,1.0);
 rot=a*57.2958+t*(35.0+30.0*q);
}
Particles.Position=p;
Particles.Color=float4(rgb,alpha);
Particles.SpriteSize=size;
Particles.SpriteRotation=rot;
Particles.Scale=float3(1.0,1.0,1.0);
Particles.MeshOrientation=float4(0,0,0,1);
Particles.SpriteFacing=float3(0,0,1);
Particles.SpriteAlignment=float3(1,0,0);
Particles.SubImageIndex=16.0*frac(t*(.67+.18*v)+q*.4);
"""
def save(a):
 if not unreal.EditorAssetLibrary.save_loaded_asset(a,False):raise RuntimeError('UE5.8 save failed '+a.get_path_name())
def clone(source,target,typ):
 if unreal.EditorAssetLibrary.does_asset_exist(target):raise RuntimeError('拒绝覆盖已有审核资产 '+target)
 v=unreal.EditorAssetLibrary.duplicate_asset(source,target)
 if not isinstance(v,typ):raise RuntimeError('真实UE5.8资源复制失败 '+target)
 RESULT['assetsCreated'].append(target)
 return v
def set_param(instance,scalar,colors,texture):
 U=unreal.MaterialEditingLibrary
 for k,v in scalar.items():
  U.set_material_instance_scalar_parameter_value(instance,k,v)
  actual=U.get_material_instance_scalar_parameter_value(instance,k)
  if abs(actual-v)>.001:raise RuntimeError('材质标量参数回读失败 '+k+' '+str(actual))
 for k,v in colors.items():
  U.set_material_instance_vector_parameter_value(instance,k,unreal.LinearColor(*v))
 U.set_material_instance_texture_parameter_value(instance,'MaskTexture',texture)
 actual_texture=U.get_material_instance_texture_parameter_value(instance,'MaskTexture')
 if actual_texture!=texture:raise RuntimeError('MaskTexture纹理参数回读失败')
 save(instance)
def main():
 assert unreal.SystemLibrary.get_engine_version().startswith('5.8'), 'Engine mismatch'
 assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
 manifest=json.loads((SOURCE/'TextureManifest_V1_Review.json').read_text(encoding='utf8'))
 if len(manifest.get('assets',[]))!=6:raise RuntimeError('六张纹理源图清单不完整')
 for item in manifest['assets']:
  b=(SOURCE/item['file']).read_bytes()
  if hashlib.sha256(b).hexdigest()!=item['sha256']:raise RuntimeError('纹理源图校验不符 '+item['file'])
 for p in [SOURCE_SYS,SOURCE_COMPAT,ICE+'/Materials/MI_DBA_IceArrow_Mist',ICE+'/Materials/MI_DBA_IceArrow_CrystalSprite']:
  if not unreal.load_asset(p):raise RuntimeError('现有复用资产缺失 '+p)
 # 六张审核纹理已由本任务首次执行保存；本轮仅复用，不修改其内容。
 expected=[MODULE,*SYS.values()]
 expected.extend(path for block in MATERIALS.values() for path in block.values())
 for path in expected:
  if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('审核目标已存在，拒绝覆盖 '+path)
 RESULT['status']='importing'
 textures={}
 tools=unreal.AssetToolsHelpers.get_asset_tools()
 for key,name in TEX.items():
  f=SOURCE/(name+'.png')
  reused_path=BASE+'/Textures/'+name
  if unreal.EditorAssetLibrary.does_asset_exist(reused_path) or (ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/PalmElementGather/Review/V1/Textures'/(name+'.uasset')).is_file():
   a=unreal.load_asset(reused_path)
   expected_size=1024 if 'Flipbook' in name else 512
   if not isinstance(a,unreal.Texture2D) or a.blueprint_get_size_x()!=expected_size or a.blueprint_get_size_y()!=expected_size:
    raise RuntimeError('前次审核纹理异常，拒绝复用 '+reused_path)
   textures[key]=a
   RESULT.setdefault('reusedPreviousReviewTextures',[]).append(reused_path)
   print('PALM_ICE_TEXTURE_REUSED',name,flush=True)
   continue
  task=unreal.AssetImportTask()
  task.set_editor_property('filename',str(f))
  task.set_editor_property('destination_path',BASE+'/Textures')
  task.set_editor_property('destination_name',name)
  task.set_editor_property('automated',True)
  task.set_editor_property('save',False)
  task.set_editor_property('replace_existing',False)
  tools.import_asset_tasks([task])
  a=unreal.load_asset(BASE+'/Textures/'+name)
  if not isinstance(a,unreal.Texture2D):raise RuntimeError('没有创建真实纹理资产 '+name)
  # 素材是用于特效遮罩/透明通道的技术图；颜色由材质参数决定，因此线性Mask采样。
  a.set_editor_property('srgb',False)
  a.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
  a.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
  a.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
  save(a)
  textures[key]=a
  RESULT['assetsCreated'].append(a.get_path_name().rsplit('.',1)[0])
  print('PALM_ICE_UE_TEXTURE_IMPORTED',name,flush=True)
 RESULT['status']='materials'
 mi_dict={}
 for stage in ('01','02'):
  target=MATERIALS[stage]
  mist=clone(ICE+'/Materials/MI_DBA_IceArrow_Mist',target['Mist'],unreal.MaterialInstanceConstant)
  crystal=clone(ICE+'/Materials/MI_DBA_IceArrow_CrystalSprite',target['Crystal'],unreal.MaterialInstanceConstant)
  # P1-01柔雾适当弱；P1-02偏向卷吸流线，浓雾并非单纯增大白光。
  is_charge=(stage=='02')
  mist_tex=textures['SpiralFlow'] if is_charge else textures['MistSoft']
  set_param(mist,{'EmissiveStrength':1.6 if is_charge else 1.05,
                  'OpacityScale':.74 if is_charge else .46},
                  {'IceTint':(.31,.69,1.0,1.0)},mist_tex)
  set_param(crystal,{'EmissiveStrength':2.2 if is_charge else 1.3,
                     'OpacityScale':.80 if is_charge else .30},
                     {'IceTint':(.64,.85,1.0,1.0)},textures['CrystalSeed'])
  mi_dict[stage]={'Mist':mist,'Crystal':crystal}
  print('PALM_ICE_UE_MI_CREATED',stage,flush=True)
 RESULT['status']='motion'
 module=clone(SOURCE_COMPAT,MODULE,unreal.NiagaraScript)
 nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl)
        if n.get_path_name().startswith(module.get_path_name()+':')]
 if len(nodes)!=1:raise RuntimeError('CustomHLSL节点唯一性不成立 '+str(len(nodes)))
 nodes[0].set_editor_property('custom_hlsl',HLSL)
 if str(nodes[0].get_editor_property('custom_hlsl'))!=HLSL:raise RuntimeError('HLSL回读失败')
 save(module)
 RESULT['status']='systems'
 for stage,target in SYS.items():
  ns=clone(SOURCE_SYS,target,unreal.NiagaraSystem)
  calls=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall)
        if n.get_path_name().startswith(ns.get_path_name()+':')]
  rebound=0
  for c in calls:
   fs=c.get_editor_property('function_script')
   if fs and fs.get_path_name()==unreal.load_asset(SOURCE_COMPAT).get_path_name():
    c.set_editor_property('function_script',module)
    if c.get_editor_property('function_script')!=module:raise RuntimeError('模块绑定失败 '+c.get_path_name())
    rebound+=1
  if rebound!=2:raise RuntimeError('预期绑定两个活跃发射器，实际 '+str(rebound))
  rr=[x for x in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties)
      if x.get_path_name().startswith(ns.get_path_name()+':')]
  if len(rr)!=2:raise RuntimeError('审核系统SpriteRenderer数量不正确 '+str(len(rr)))
  for renderer in rr:
   path=renderer.get_path_name()
   mat=mi_dict[stage]['Mist'] if ':HandFlame_' in path else mi_dict[stage]['Crystal'] if ':GatheringSparks_' in path else None
   if mat is None:raise RuntimeError('未知发射器 '+path)
   renderer.set_editor_property('material',mat)
   if renderer.get_editor_property('material')!=mat:raise RuntimeError('渲染器回读失败 '+path)
  save(ns)
  RESULT.setdefault('systems',{})[stage]={'asset':target,'reboundCalls':rebound,'renderers':len(rr),
                                         'coldReloadChecked':False,'nativePIEVisible':False}
  print('PALM_ICE_UE_NIAGARA_CREATED',stage,target,flush=True)
 RESULT['status']='native_assets_saved_pending_graphics_validation'
 print('PALM_ICE_UE_AUTHOR_FINISHED',len(RESULT['assetsCreated']),flush=True)
try:main()
except Exception:
 RESULT['status']='failed';RESULT['errors'].append(traceback.format_exc()[-4500:])
 print('PALM_ICE_UE_AUTHOR_ERROR',RESULT['errors'][-1],flush=True)
 raise
finally:
 REPORT.parent.mkdir(parents=True,exist_ok=True)
 REPORT.write_text(json.dumps(RESULT,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
