# -*- coding: utf-8 -*-
"""只读验证 UE5.8 Niagara 原生渲染器对象与火球可复用参数，供独立P1资产制作使用。"""
from pathlib import Path
import unreal,json
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
paths=[
 '/DBAPresentationPack_Core/VFX/Library/Game/FireMage/Niagara/NS_PY_Charge',
 '/DBAPresentationPack_Core/VFX/Library/01_Cast/NS_Fire_Fireball_Charge',
 '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3RefCharge'
]
results={}
for path in paths:
    v={'asset':path,'exists':False,'renderers':[]}
    s=unreal.load_asset(path)
    if s:
        v['exists']=True
        v['system_methods']=[x for x in dir(s) if 'emitter' in x.lower() or 'parameter' in x.lower()]
        for obj in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
            p=obj.get_path_name()
            if p.startswith(s.get_path_name()+':'):
                v['renderers'].append({'path':p,'class':obj.get_class().get_name(),
                'material':str(obj.get_editor_property('material').get_path_name()) if obj.get_editor_property('material') else None})
        v['renderersCount']=len(v['renderers'])
    results[path]=v
p=root/'Saved/Validation/FireballV3P1/NiagaraRendererInspection.json'
p.parent.mkdir(parents=True,exist_ok=True)
results['RotatorPositional_0_145_0']=str(unreal.Rotator(0,145,0))
results['RotatorNamedYaw145']=str(unreal.Rotator(pitch=0,yaw=145,roll=0))
p.write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print('P1_RENDERER_INSPECTED',str(p))
