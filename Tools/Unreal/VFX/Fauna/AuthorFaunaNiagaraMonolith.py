#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实群落Array DI与GPU Mesh系统作者；步骤明确分开，任何图编译错误立即失败。"""
import argparse,json
from FaunaMonolithProtocol import ROOT,niagara,editor_python,assert_owned_editor
GP='/GamePlatformVFX/Environment/Fauna'
DBA='/DBAPresentationPack_Core/VFX/Environment/Fauna'
MODULE=GP+'/Niagara/NM_GP_FaunaArrayMotion'
BASE=GP+'/Niagara/NS_GP_Fauna_Base'
EMITTER='Fauna'
ARRAYS=[('Positions','FaunaPositions','NiagaraDataInterfaceArrayFloat3'),
        ('Orientations','FaunaOrientations','NiagaraDataInterfaceArrayQuat'),
        ('MotionStates','FaunaMotionStates','NiagaraDataInterfaceArrayFloat4')]
def unpack(value):
    return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value
def base():
    assert_owned_editor()
    hlsl=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/Fauna/FaunaArrayMotion.hlsl').read_text(encoding='utf-8')
    print('MODULE',niagara('create_module_from_hlsl',name='FaunaArrayMotion',save_path=MODULE,hlsl=hlsl,
        inputs=[{'name':n,'type':t} for n,_,t in ARRAYS]+[{'name':'Index','type':'int'}],
        outputs=[],description='受控CPU群落的GPU网格显示；位置/朝向/动作数组与单次Burst的稳定UniqueID一一对应。'))
    spec={'user_parameters':[{'name':'User.'+u,'type':t} for _,u,t in ARRAYS]+[{'name':'User.FaunaCount','type':'int','default':'2'}],
        'emitters':[{'name':EMITTER,'asset':'/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',
            'properties':{'SimTarget':'GPUComputeSim','bLocalSpace':'True'},
            'modules':[{'stage':'emitter_update','script':'/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',
                        'bindings':{'Spawn Count':'User.FaunaCount'}},
                       {'stage':'particle_update','script':MODULE,'bindings':dict([(n,'User.'+u) for n,u,_ in ARRAYS]+[('Index','Particles.UniqueID')])}],
            'renderers':[{'class':'Mesh','material':'/Engine/BasicShapes/BasicShapeMaterial'}]}]}
    print('BASE',niagara('create_system_from_spec',save_path=BASE,spec=spec))
    configure(BASE)
def configure(path):
    for renderer in reversed(niagara('list_renderers',asset_path=path,emitter=EMITTER)['renderers']):
        if renderer['type']=='sprite':
            niagara('remove_renderer',asset_path=path,emitter=EMITTER,renderer_index=renderer['index'])
    for module in unpack(niagara('get_ordered_modules',asset_path=path,emitter=EMITTER)):
        if module['function_name']=='ParticleState':
            niagara('remove_module',asset_path=path,emitter=EMITTER,module_node=module['node_guid'])
        if module['function_name']=='EmitterState':
            niagara('move_module',asset_path=path,emitter=EMITTER,module_node=module['node_guid'],new_index=0)
    niagara('set_static_switch_value',asset_path=path,emitter=EMITTER,module_node='EmitterState',input='Life Cycle Mode',value='Self')
    niagara('set_emitter_loop_profile',asset_path=path,emitter=EMITTER,loop_behavior='Once',loop_duration=86400.0)
    for n,u,_ in ARRAYS:
        niagara('set_module_input_binding',asset_path=path,emitter=EMITTER,module_node='NM_GP_FaunaArrayMotion',input=n,binding='User.'+u)
    niagara('set_module_input_binding',asset_path=path,emitter=EMITTER,module_node='NM_GP_FaunaArrayMotion',input='Index',binding='Particles.UniqueID')
    niagara('set_module_input_binding',asset_path=path,emitter=EMITTER,module_node='SpawnBurst_Instantaneous',input='Spawn Count',binding='User.FaunaCount')
    niagara('set_fixed_bounds',asset_path=path,min=[-3000,-3000,-1500],max=[3000,3000,1500],enabled=True)
    niagara('set_fixed_bounds',asset_path=path,emitter=EMITTER,min=[-3000,-3000,-1500],max=[3000,3000,1500],enabled=True)
    if path==BASE: niagara('set_renderer_mesh',asset_path=path,emitter=EMITTER,renderer_index=0,mesh='/Engine/BasicShapes/Plane')
    diag=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
    print('COMPILER',json.dumps(diag,ensure_ascii=False))
    if diag.get('error_count',1): raise RuntimeError('群落GPU图编译失败')
    niagara('save_system',asset_path=path,only_if_dirty=False)
    niagara('save_system',asset_path=MODULE,only_if_dirty=False)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--base',action='store_true');p.add_argument('--configure',action='store_true');a=p.parse_args()
    if a.base: base()
    elif a.configure: assert_owned_editor();configure(BASE)
    else: print('FAUNA_NIAGARA_PLAN',BASE)
