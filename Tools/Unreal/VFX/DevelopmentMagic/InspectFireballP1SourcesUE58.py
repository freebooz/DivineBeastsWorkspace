# -*- coding: utf-8 -*-
"""只读盘点火球 P1 的原生纹理、材质、Niagara、骨骼挂点和编译风险，不保存任何资产。"""
import unreal, json, hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
B='/DBAPresentationPack_Core/VFX/Library/Game/FireMage'
paths={
    'charge':B+'/Niagara/NS_PY_Charge',
    'template':'/DBAPresentationPack_Core/VFX/Library/01_Cast/NS_Fire_Fireball_Charge',
    'flame_texture':B+'/Textures/T_PY_FlameFlipbook',
    'core_instance':B+'/Materials/Instances/MI_PY_FlipbookCore',
    'plume_instance':B+'/Materials/Instances/MI_PY_FlipbookPlume',
    'ember_instance':B+'/Materials/Instances/MI_PY_Ember',
    'fire_material':B+'/Materials/M_PY_Flipbook',
    'cast_anim':'/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast',
    'mannequin':'/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'
}
out={'project':unreal.Paths.get_project_file_path(),'engine':unreal.SystemLibrary.get_engine_version(),'assets':{},'success':True}
for k,path in paths.items():
    d={'path':path,'exists':False}
    try:
        x=unreal.load_asset(path)
        if x:
            d.update(exists=True,className=x.get_class().get_name())
            if isinstance(x,unreal.MaterialInterface):
                parent=x if isinstance(x,unreal.Material) else x.get_editor_property('parent')
                d['parent']=parent.get_path_name() if parent else None
                for pname,method in [('scalars','get_scalar_parameter_names'),('vectors','get_vector_parameter_names'),('textures','get_texture_parameter_names')]:
                    try:
                        keys=getattr(unreal.MaterialEditingLibrary,method)(x)
                        d[pname]=list(map(str,keys))
                    except Exception as ex:d[pname+'Error']=str(ex)
                if isinstance(x,unreal.MaterialInstanceConstant):
                    for typ,func,prop in [('scalars',unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value,'float'),('vectors',unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value,'color')]:
                        d[typ+'Values']={}
                        for name in d.get(typ,[]):
                            try:
                                value=func(x,name)
                                d[typ+'Values'][name]=str(value)
                            except Exception: pass
            if isinstance(x,unreal.Texture2D):
                try:d['size']=[x.blueprint_get_size_x(),x.blueprint_get_size_y()]
                except Exception: pass
            if isinstance(x,unreal.SkeletalMesh):
                try:
                    skeleton=x.get_editor_property('skeleton')
                    d['skeleton']=skeleton.get_path_name()
                    d['sockets']=[str(z.socket_name) for z in skeleton.get_editor_property('sockets')][:50]
                except Exception as ex:d['skeletonError']=str(ex)
            if isinstance(x,unreal.NiagaraSystem):
                try:d['effectType']=str(x.get_editor_property('effect_type').get_path_name())
                except Exception:pass
    except Exception as ex:
        d['error']=str(ex);out['success']=False
    out['assets'][k]=d
P=ROOT/'Saved/Validation/FireballV3P1/SourceAssetsReview.json'
P.parent.mkdir(parents=True,exist_ok=True)
P.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('P1_READONLY_INVENTORY',str(P),'ASSETS',len(out['assets']),'PASS',out['success'])
