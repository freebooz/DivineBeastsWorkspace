# -*- coding:utf-8 -*-
"""只读审查冷重载后的NiagaraMeshRenderer绑定，动态加载未暴露的UE5.8渲染器UClass。"""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
B='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
targets=[B+'/Niagara/Systems/NS_DBA_IceBlock_Form_Review',B+'/Niagara/Systems/NS_DBA_IceBlock_FlightTrail_Review']
# 必须加载Niagara系统自身，否则ObjIterator无法枚举尚未驻留的Renderer子对象。
loaded=[unreal.load_asset(path) for path in targets]
assert all(isinstance(a,unreal.NiagaraSystem) for a in loaded)
r={'engine':unreal.SystemLibrary.get_engine_version(),'rendererClass':None,'objects':[],'referencers':[],'errors':[]}
try:
 cls=unreal.load_class(None,'/Script/Niagara.NiagaraMeshRendererProperties')
 r['rendererClass']=str(cls)
 r['loadedTargets']=[a.get_path_name() for a in loaded]
 if cls:
  all_objs=list(unreal.ObjectIterator(cls))
  r['count']=len(all_objs)
  for obj in all_objs:
   p=obj.get_path_name()
   if not any(p.startswith(sys+'.') or p.startswith(sys+':') for sys in targets):continue
   entry={'path':p,'class':obj.get_class().get_name()}
   for prop in ['meshes','Meshes','override_materials','OverrideMaterials']:
    try:
     value=obj.get_editor_property(prop)
     entry[prop]=[str(x) for x in value] if isinstance(value,(list,tuple)) else str(value)
     if prop.lower()=='meshes':
      for m in value:
       for k in ['mesh','Mesh']:
        try:
         sm=m.get_editor_property(k)
         if sm:entry.setdefault('meshes',[]).append(sm.get_path_name() if hasattr(sm,'get_path_name') else str(sm))
        except:pass
    except Exception as e:entry[prop+'Error']=str(e)[:170]
   r['objects'].append(entry)
 mesh=B+'/Meshes/SM_DBA_IceBlock_FacetedCore_Review'
 r['referencers']=[str(x) for x in unreal.EditorAssetLibrary.find_package_referencers_for_asset(mesh,False)]
except Exception:
 r['errors'].append(traceback.format_exc()[-3000:])
dest=root/'Saved/Validation/OrbProjectileV1/MeshRenderersColdProbe.json'
dest.write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('ORBP_COLD_MESH_PROBE',len(r['objects']),len(r['referencers']),str(dest))
