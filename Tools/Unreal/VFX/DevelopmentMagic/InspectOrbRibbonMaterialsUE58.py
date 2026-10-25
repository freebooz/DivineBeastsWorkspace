# -*- coding:utf-8 -*-
"""只读诊断OrbProjectile独立审核Ribbon材质透明度和原生Niagara默认绑定，避免以编译成功推定可见。"""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
base='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
targets=[
 base+'/Materials/MI_DBA_FireOrb_Flight_Ribbon_Review',
 base+'/Materials/MI_DBA_IceBlock_Flight_Ribbon_Review',
 '/DBAPresentationPack_Core/VFX/Library/00_Core/Materials/ProjectileTrail/MI_VFX_ProjectileTrail_SoftCrimson',
 '/DBAPresentationPack_Core/VFX/IceArrow/Materials/MI_DBA_IceArrow_Trail'
]
result={'materials':[],'api':[],'errors':[]}
try:
 for path in targets:
  m=unreal.load_asset(path)
  row={'path':path,'exists':bool(m)}
  if not m:raise RuntimeError('Missing '+path)
  row['class']=m.get_class().get_name()
  if isinstance(m,unreal.MaterialInstanceConstant):
   par=m.get_editor_property('parent')
   row['parent']=par.get_path_name() if par else None
  for kind,fn in [('scalars',unreal.MaterialEditingLibrary.get_scalar_parameter_names),('vectors',unreal.MaterialEditingLibrary.get_vector_parameter_names),
                    ('textures',unreal.MaterialEditingLibrary.get_texture_parameter_names)]:
   try:
    names=[str(x) for x in fn(m)]
    row[kind]=names
    for name in names:
     try:
      if kind=='scalars':v=unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(m,name)
      elif kind=='vectors':v=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(m,name)
      else:v=unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m,name)
      row.setdefault(kind+'Values',{})[name]=str(v)
     except Exception as e:row.setdefault(kind+'Errors',{})[name]=str(e)[:220]
   except Exception as e:row[kind+'Error']=str(e)[:300]
  result['materials'].append(row)
 for cls_name in ['NiagaraSimCacheFunctionLibrary','NiagaraSimCache']:
  cls=getattr(unreal,cls_name,None)
  if cls:
   result['api'].append({cls_name:[x for x in dir(cls) if any(k in x.lower() for k in ['particle','cache','attribute','capture','frame','emitter'])]})
except Exception:
 result['errors'].append(traceback.format_exc()[-3500:])
dest=root/'Saved/Validation/OrbProjectileV1/OrbRibbonMaterialDiagnostic.json'
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('ORBP_RIBBON_MATERIAL_DIAGNOSTIC',len(result['materials']),dest,flush=True)
