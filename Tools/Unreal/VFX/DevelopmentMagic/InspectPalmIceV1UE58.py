# -*- coding: utf-8 -*-
"""SSLM P1 冰霜聚能制作前只读清点：真实UE5.8 Niagara内部脚本/发射器材质/可复用母材质。"""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX'
ASSETS={
 'fireSource':BASE+'/Library/Game/FireMage/Niagara/NS_PY_Charge',
 'commonVortex':BASE+'/Library/00_Core/NiagaraModules/Common/NM_VFX_Vortex',
 'fireMotion':BASE+'/Library/Game/FireMage/Niagara/Modules/NM_PY_FireMotionFlipbook',
 'compatMotion':BASE+'/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5',
 'crystalInstance':BASE+'/IceArrow/Materials/MI_DBA_IceArrow_CrystalSprite',
 'mistMaterial':BASE+'/IceArrow/Materials/M_DBA_IceArrow_Mist',
 'mistInstance':BASE+'/IceArrow/Materials/MI_DBA_IceArrow_Mist',
 'snowInstance':BASE+'/IceArrow/Materials/MI_DBA_IceArrow_Snow',
 'coreMaterial':BASE+'/IceArrow/Materials/M_DBA_IceArrow_Luminous',
 'glowInstance':BASE+'/IceArrow/Materials/MI_DBA_IceArrow_ImpactFlash',
 'iceTexture':BASE+'/IceArrow/Textures/T_DBA_IceArrow_FrostMist',
 'iceFlake':BASE+'/IceArrow/Textures/T_DBA_IceArrow_IceFlake',
}
output={'project':unreal.Paths.get_project_file_path(),'engine':unreal.SystemLibrary.get_engine_version(),'assets':{}}
for name,path in ASSETS.items():
 a=unreal.load_asset(path)
 q={'path':path,'exists':bool(a),'type':a.get_class().get_name() if a else 'none'}
 if a and isinstance(a,unreal.MaterialInterface):
  for suffix,func in [('scalar',unreal.MaterialEditingLibrary.get_scalar_parameter_names),
                       ('vector',unreal.MaterialEditingLibrary.get_vector_parameter_names),
                       ('texture',unreal.MaterialEditingLibrary.get_texture_parameter_names)]:
   try:q[suffix+'Params']=[str(v) for v in func(a)]
   except Exception as ex:q[suffix+'Error']=str(ex)
 if a and isinstance(a,unreal.NiagaraSystem):
  q['moduleCalls']=[]
  q['renderers']=[]
  for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    script=n.get_editor_property('function_script')
    q['moduleCalls'].append({'node':n.get_path_name(),'script':script.get_path_name() if script else None})
  for n in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    material=n.get_editor_property('material')
    q['renderers'].append({'node':n.get_path_name(),'material':material.get_path_name() if material else None})
 if a and isinstance(a,unreal.NiagaraScript):
  q['customHlsl']=[]
  q['inputNodes']=[]
  for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    h=str(n.get_editor_property('custom_hlsl'))
    q['customHlsl'].append({'node':n.get_path_name(),'length':len(h),'preview':h[:550]})
  for n in unreal.ObjectIterator(unreal.NiagaraNodeInput):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    q['inputNodes'].append(n.get_path_name())
 output['assets'][name]=q
dest=ROOT/'Saved/Validation/PalmIceV1/NativeAuthoringReadOnly.json'
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PALM_ICE_NATIVE_INVENTORY',dest,len(output['assets']))
