#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通过现有编辑器Monolith创建三种鸟类Niagara变体、EffectType和WorldDefinition。
平台系统仅用Engine中立网格；项目变体覆盖厘米网格与材质，资源唯一所有者为项目表现包。
创建前验证目标空缺，重入只能读取已存在同类型资产；任何编译错误均中止。
"""
import argparse
import json
from BirdFlockMonolithProtocol import niagara,editor_python,ROOT,EVIDENCE
from AuthorBirdFlockNiagaraMonolith import GP,DBA,BASE,EMITTER
EFFECT=GP+'/EffectTypes/NET_GP_BirdFlock_Ambient'
SPECIES={'Seagull':(32,1700,850,250,0),'Swallow':(72,1150,570,340,2),'Eagle':(5,2400,1400,350,0)}

def systems():
    # 只在本任务独占的鸟群目录创建，存在时核验类型并复用，不修改别人的uasset。
    print('DUPLICATE',editor_python("import unreal\na=unreal.EditorAssetLibrary\n"+ '\n'.join(
        f"assert a.does_asset_exist({BASE!r}); x=a.load_asset({(DBA+'/Niagara/NS_DBA_BirdFlock_'+s)!r}) if a.does_asset_exist({(DBA+'/Niagara/NS_DBA_BirdFlock_'+s)!r}) else a.duplicate_asset({BASE!r},{(DBA+'/Niagara/NS_DBA_BirdFlock_'+s)!r}); assert isinstance(x,unreal.NiagaraSystem); print('SPECIES_ASSET',x.get_path_name())" for s in SPECIES)))
    # EffectType只创建一次。它与基础/项目三个Niagara系统保持一致，走现有环境预算。
    check=editor_python(f"import unreal; print('BIRD_EFFECT_EXISTS',unreal.EditorAssetLibrary.does_asset_exist({EFFECT!r}))")
    exists=any('BIRD_EFFECT_EXISTS True' in o.get('output','') for o in check.get('output',[]))
    if not exists:
        print('EFFECT',niagara('create_effect_type',save_path=EFFECT,cull_reaction='DeactivateResume',update_frequency='Low',max_distance=35000.0))
    for species,settings in SPECIES.items():
        path=DBA+'/Niagara/NS_DBA_BirdFlock_'+species
        print('SPECIES_MESH',species,niagara('set_renderer_mesh',asset_path=path,emitter=EMITTER,renderer_index=0,mesh=DBA+'/Meshes/SM_DBA_Bird_'+species+'_Cm'))
        niagara('set_renderer_material',asset_path=path,emitter=EMITTER,renderer_index=0,material=DBA+'/Materials/MI_DBA_Bird_'+species)
        niagara('set_renderer_property',asset_path=path,emitter=EMITTER,renderer_index=0,property='MeshBoundsScale',value='(X=1.0,Y=1.0,Z=3.0)')
        for parameter,value in zip(('BirdCount','FlockRadius','FlockHeight','FlightSpeed','FlightMode'),settings):
            niagara('set_parameter_default',asset_path=path,parameter='User.'+parameter,value=str(value))
        niagara('set_effect_type',asset_path=path,effect_type=EFFECT)
        diagnostic=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        print('SPECIES_COMPILE',species,diagnostic.get('error_count'),diagnostic.get('warning_count'))
        if diagnostic.get('error_count',1) or diagnostic.get('warning_count',0):
            raise RuntimeError('物种真实Niagara编译未通过：'+species)
        # 复制后诊断可能清除dirty标记但包尚未落盘；强制保存并检查真实磁盘文件。
        saved=niagara('save_system',asset_path=path,only_if_dirty=False)
        disk=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Niagara'/('NS_DBA_BirdFlock_'+species+'.uasset')
        if not disk.is_file() or disk.stat().st_size < 1024:
            raise RuntimeError('Niagara复制资产未真实写盘：'+str(disk))
        print('SPECIES_SAVE',saved,'BYTES',disk.stat().st_size)
    niagara('set_effect_type',asset_path=BASE,effect_type=EFFECT)
    niagara('save_system',asset_path=BASE)
    print('BIRD_SPECIES_SYSTEMS_CREATED',len(SPECIES))

def definitions():
    source=(ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockDefinitions.py').read_text(encoding='utf-8')
    # 使用当前已编译的WorldDefinition类型，无需等待新建原型类的LiveCoding。
    print(json.dumps(editor_python(source),ensure_ascii=False))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--systems',action='store_true');p.add_argument('--definitions',action='store_true');a=p.parse_args()
    if a.systems: systems()
    elif a.definitions: definitions()
    else: print('BIRD_SPECIES_PLANNED',SPECIES)
