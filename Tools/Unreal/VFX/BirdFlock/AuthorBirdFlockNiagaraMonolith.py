#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4真实GPU鸟群系统作者：通用HLSL+Niagara模板，项目物种只覆盖网格/材质。
仅经已连接Monolith官方编辑器动作写入uasset，不碰当前关卡或其他任务的资源。
每一步保留JSON证据，异常不自动重放写入；inspect不写UE资源。
"""
import argparse
import json
from BirdFlockMonolithProtocol import ROOT,EVIDENCE,niagara,editor_python
GP='/GamePlatformVFX/Environment/BirdFlock'
DBA='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
MODULE=GP+'/Niagara/NM_GP_BirdFlockMotion'
BASE=GP+'/Niagara/NS_GP_BirdFlock_Base'
EMITTER='BirdFlock'

PARAMS=[('BirdCount','int',32),('FlockRadius','float',1700.0),('FlockHeight','float',850.0),
        ('FlightSpeed','float',250.0),('FlockSeed','int',137),('FlightMode','int',0),('WindInfluence','float',80.0)]

def author_base():
    shader=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    print('MODULE',niagara('create_module_from_hlsl',name='BirdFlockMotion',save_path=MODULE,hlsl=shader,
        inputs=[{'name':n,'type':'float'} for n in ['Clock','Individual','Radius','Height','Speed','Seed','Mode','Wind']],
        outputs=[],description='通用鸟群GPU盘旋/掠过/八字路径，局部安全体积，无权威碰撞。'))
    spec={'user_parameters':[{'name':'User.'+n,'type':t,'default':str(v)} for n,t,v in PARAMS],
        'emitters':[{'name':EMITTER,'asset':'/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',
            'properties':{'SimTarget':'GPUComputeSim','bLocalSpace':'True'},
            'modules':[
                {'stage':'emitter_update','script':'/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',
                 'bindings':{'Spawn Count':'User.BirdCount'}},
                {'stage':'particle_update','script':MODULE,
                 'bindings':{'Clock':'System.Age','Individual':'Particles.MaterialRandom',
                             'Radius':'User.FlockRadius','Height':'User.FlockHeight','Speed':'User.FlightSpeed',
                             'Seed':'User.FlockSeed','Mode':'User.FlightMode','Wind':'User.WindInfluence'}}],
            'renderers':[{'class':'Mesh','material':GP+'/Materials/M_GP_BirdFlock_Master'}]}]}
    (EVIDENCE/'BirdFlockBaseSpec.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2),encoding='utf-8')
    print('SYSTEM',niagara('create_system_from_spec',save_path=BASE,spec=spec))
    print('EMITTERS',json.dumps(niagara('list_emitters',asset_path=BASE),ensure_ascii=False))
    print('MODULES',json.dumps(niagara('get_ordered_modules',asset_path=BASE,emitter=EMITTER),ensure_ascii=False))
    print('RENDERERS',json.dumps(niagara('list_renderers',asset_path=BASE,emitter=EMITTER),ensure_ascii=False))

def unpack(value):
    """Monolith部分动作将数组包装为result字符串，显式解析而非猜测成功。"""
    return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value

def configure_base():
    """已有基础系统精确整改：移除模板Sprite、粒子寿命杀死，防止重复喷出鸟群。"""
    renderers=niagara('list_renderers',asset_path=BASE,emitter=EMITTER)['renderers']
    for renderer in reversed(renderers):
        if renderer['type']=='sprite':
            print('REMOVE_SPRITE',niagara('remove_renderer',asset_path=BASE,emitter=EMITTER,renderer_index=renderer['index']))
    modules=unpack(niagara('get_ordered_modules',asset_path=BASE,emitter=EMITTER))
    for module in modules:
        if module['function_name']=='ParticleState':
            print('NO_LIFETIME_KILL',niagara('remove_module',asset_path=BASE,emitter=EMITTER,module_node=module['node_guid']))
        if module['function_name']=='EmitterState':
            print('ORDER_STATE_FIRST',niagara('move_module',asset_path=BASE,emitter=EMITTER,module_node=module['node_guid'],new_index=0))
    print('BOUNDS',niagara('set_fixed_bounds',asset_path=BASE,min=[-24000,-16000,-1000],max=[24000,16000,12000],enabled=True))
    print('EMITTER_BOUNDS',niagara('set_fixed_bounds',asset_path=BASE,emitter=EMITTER,min=[-24000,-16000,-1000],max=[24000,16000,12000],enabled=True))
    print('NEUTRAL_MESH',niagara('set_renderer_mesh',asset_path=BASE,emitter=EMITTER,renderer_index=0,mesh='/Engine/BasicShapes/Plane'))
    print('ONE_LOOP',niagara('set_emitter_loop_profile',asset_path=BASE,emitter=EMITTER,loop_behavior='Once',loop_duration=86400.0))
    print('COMPILER',json.dumps(niagara('get_system_diagnostics',asset_path=BASE,compile_first=True),ensure_ascii=False))
    print('SAVE_BASE',niagara('save_system',asset_path=BASE))
    print('SAVE_MODULE',niagara('save_system',asset_path=MODULE))

def repair_bindings():
    """删除模板ParticleState后重新建立绑定读节点，修复其旧ParameterMap输入被断开。"""
    bindings={'Clock':'System.Age','Individual':'Particles.MaterialRandom','Radius':'User.FlockRadius',
              'Height':'User.FlockHeight','Speed':'User.FlightSpeed','Seed':'User.FlockSeed',
              'Mode':'User.FlightMode','Wind':'User.WindInfluence'}
    for name,binding in bindings.items():
        print('REBIND',name,niagara('set_module_input_binding',asset_path=BASE,emitter=EMITTER,
                                  module_node='NM_GP_BirdFlockMotion',input=name,binding=binding))
    # EmitterState重新排到Burst之前后，同样重建数量绑定的读取来源。
    print('REBIND_COUNT',niagara('set_module_input_binding',asset_path=BASE,emitter=EMITTER,
         module_node='SpawnBurst_Instantaneous',input='Spawn Count',binding='User.BirdCount'))
    result=niagara('get_system_diagnostics',asset_path=BASE,compile_first=True)
    print('DIAGNOSTICS',result.get('error_count'),result.get('warning_count'),json.dumps(result.get('errors',[])[:3],ensure_ascii=False))
    if result.get('error_count',1):
        raise RuntimeError('真实GPU图仍存在编译错误，保留完整JSON证据')
    print('SAVE',niagara('save_system',asset_path=BASE))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--apply-base',action='store_true');p.add_argument('--configure-base',action='store_true');p.add_argument('--repair-bindings',action='store_true');a=p.parse_args()
    if a.apply_base:
        author_base()
    elif a.configure_base:
        configure_base()
    elif a.repair_bindings:
        repair_bindings()
    else:
        print('BIRD_NIAGARA_PLAN',BASE,MODULE,PARAMS)
