# -*- coding: utf-8 -*-
"""P1寒气初聚/冰霜蓄力资产冷启动只读审核：核对系统、材质、真实源纹理、共用运动模块。"""
import unreal,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1'
motion=BASE+'/Niagara/NM_DBA_PalmGatherMotion_Review'
result={'engine':unreal.SystemLibrary.get_engine_version(),'phase':'P1-01-02','systems':{},'errors':[]}
def assert_true(c,msg):
 if not c:result['errors'].append(msg)
m=unreal.load_asset(motion)
assert_true(isinstance(m,unreal.NiagaraScript),'共用Niagara Script不存在')
result['motionExists']=bool(m)
texfiles={name:unreal.load_asset(BASE+'/Textures/'+name) for name in
 ['T_DBA_PalmIce_MistSoft_Review','T_DBA_PalmIce_SpiralFlow_Review','T_DBA_PalmIce_Core_Review',
  'T_DBA_PalmIce_CrystalSeed_Review','T_DBA_PalmIce_MistFlipbook_4x4_Review','T_DBA_PalmIce_SpiralFlipbook_4x4_Review']}
result['textures']={name:bool(t) for name,t in texfiles.items()}
for name,asset in texfiles.items():assert_true(isinstance(asset,unreal.Texture2D),'贴图缺失/类型错误'+name)
for stage,suffix in [('01','P101_GatherReview'),('02','P102_ChargeReview')]:
 path=BASE+'/Niagara/NS_DBA_PalmIce_'+suffix
 system=unreal.load_asset(path)
 d={'path':path,'exists':bool(system),'rendererMaterials':[],'sharedMotionReferences':0,'failed':[]}
 assert_true(isinstance(system,unreal.NiagaraSystem),'Niagara系统缺失 '+stage)
 if system:
  calls=[x for x in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall)
    if x.get_path_name().startswith(system.get_path_name()+':')]
  for c in calls:
   fx=c.get_editor_property('function_script')
   if fx and fx.get_path_name()==m.get_path_name():d['sharedMotionReferences']+=1
  rs=[x for x in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties)
      if x.get_path_name().startswith(system.get_path_name()+':')]
  for r in rs:
   mat=r.get_editor_property('material')
   d['rendererMaterials'].append(mat.get_path_name() if mat else None)
  assert_true(d['sharedMotionReferences']==2,'共享运动模块绑定数量异常 '+stage)
  assert_true(len(rs)==2,'SpriteRenderer数量异常 '+stage)
  assert_true(all(mat and 'PalmElementGather/Review/V1' in mat for mat in d['rendererMaterials']),'渲染器未独立使用审核材质 '+stage)
 result['systems'][stage]=d
result['pass']=len(result['errors'])==0
dest=ROOT/'Saved/Validation/PalmIceV1/NativeColdLoadResult.json'
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PALM_ICE_COLD_LOAD',result['pass'],result['errors'])
if not result['pass']:raise RuntimeError('冷启动未通过：'+str(result['errors']))
