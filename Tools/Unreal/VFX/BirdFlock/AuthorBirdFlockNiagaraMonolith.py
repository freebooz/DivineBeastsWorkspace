#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4真实GPU鸟群系统作者：通用HLSL+Niagara模板，项目物种只覆盖网格/材质。
仅经已连接Monolith官方编辑器动作写入uasset，不碰当前关卡或其他任务的资源。
每一步保留JSON证据，异常不自动重放写入；inspect不写UE资源。
"""
import argparse
import json
import math
import re
import hashlib
from BirdFlockMonolithProtocol import ROOT,EVIDENCE,niagara,editor_python,call
GP='/GamePlatformVFX/Environment/BirdFlock'
DBA='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
MODULE=GP+'/Niagara/NM_GP_BirdFlockMotion'
BASE=GP+'/Niagara/NS_GP_BirdFlock_Base'
EMITTER='BirdFlock'

PARAMS=[('BirdCount','int',32),('FlockRadius','float',1700.0),('FlockHeight','float',850.0),
        ('FlightSpeed','float',250.0),('FlockSeed','int',137),('FlightMode','int',0),('WindInfluence','float',80.0),
        ('FlockMinimumSpacing','float',220.0)]

def route_profile(mode):
    """自然路线的宽/深倍率、曲线距离保守下界及公共起伏厘米；纯测试逐项对照实际HLSL。"""
    if mode not in (0,1,2):
        raise ValueError('自然路线只支持已发布模式0/1/2。')
    return ((1.7,0.8,1.10,120.0),(1.8,0.55,1.02,72.0),(1.6,0.65,1.015,60.0))[mode]


def route_envelope(radius_cm,height_cm,count,spacing_cm,mode,wind_cm=2000.0,mesh_margin_cm=0.0):
    """解析全时XYZ包络，包含任意相位、公共漂移/风、Z层与实际网格缩放/WPO单边余量。

    输入单位厘米，数量0..512；返回是否装进现有固定Bounds，不扩大资源边界或截断粒子。
    字段各自合法不代表任意组合都受支持；小半径高数量可能因固定Z层超高，作者须明确拒绝。
    """
    values=(radius_cm,height_cm,spacing_cm,wind_cm,mesh_margin_cm)
    if not all(math.isfinite(value) for value in values) or not (200<=radius_cm<=10000 and
            200<=height_cm<=10000 and 0<=count<=512 and int(count)==count and
            1<=spacing_cm<=1000 and 0<=wind_cm<=2000 and mesh_margin_cm>=0):
        raise ValueError('自然路线包络输入超出已发布字段范围或存在非有限值。')
    width,depth,distance_bound,bob=route_profile(mode)
    radius=radius_cm*0.9
    limit=radius*distance_bound
    capacity=1 if spacing_cm>limit else max(1,math.floor(math.pi/math.asin(min(1.0,spacing_cm/limit))))
    active=max(1,count)
    lanes=max(2,math.ceil(active/capacity))
    slots=max(1,math.ceil(active/lanes))
    lanes=math.ceil(active/slots)
    span=(lanes-1)*spacing_cm
    center=max(height_cm,height_cm*0.35+span*0.5+bob)
    extent_x=(width+0.22)*radius+wind_cm
    extent_y=(depth+0.13)*radius+wind_cm*0.26
    minimum=[-extent_x,-extent_y,center-span*0.5-bob]
    maximum=[extent_x,extent_y,center+span*0.5+bob]
    mesh_min=[value-mesh_margin_cm for value in minimum]
    mesh_max=[value+mesh_margin_cm for value in maximum]
    fixed_min=[-24000.0,-16000.0,-1000.0]
    fixed_max=[24000.0,16000.0,12000.0]
    return {'count':count,'mode':mode,'radiusCm':radius_cm,'heightCm':height_cm,
            'minimumSpacingCm':spacing_cm,'windAmplitudeCm':wind_cm,'meshMarginCm':mesh_margin_cm,
            'capacityPerLane':capacity,'laneCount':lanes,'centerMinCm':minimum,'centerMaxCm':maximum,
            'meshMinCm':mesh_min,'meshMaxCm':mesh_max,'fixedMinCm':fixed_min,'fixedMaxCm':fixed_max,
            'withinFixedBounds':all(low>=fixed_min[i] and high<=fixed_max[i] for i,(low,high) in enumerate(zip(mesh_min,mesh_max)))}


def validate_motion_graph(graph, call_text):
    """检查真实图的ParameterMap合同及八个输入；纯图夹具可复用，不能替代引擎冷重载。

    输入为Monolith完整nodes/edges与调用节点原生T3D，孤立引脚、回环、下游读取或错绑立即失败。
    返回只读结构摘要；不修改资源，不接受编译缓存代替可审核的实际接线。
    """
    functions=[n for n in graph['nodes'] if n.get('function_name')=='NM_GP_BirdFlockMotion']
    assert len(functions)==1,'运动模块必须唯一'
    function=functions[0]
    inputs=[p for p in function['pins'] if p['name']=='InputMap' and p['direction']=='Input']
    assert len(inputs)==1 and inputs[0]['linked_count']==1,'InputMap必须有唯一上游'
    assert 'bOrphanedPin=True' not in call_text,'保存后的调用节点包含孤立引脚'
    edges=graph['edges']
    nodes={n['node_guid']:n for n in graph['nodes']}
    adjacency={key:[] for key in nodes}
    for edge in edges:
        adjacency[edge['from_node_guid']].append(edge['to_node_guid'])
    visiting=set()
    visited=set()
    def visit(key):
        # Niagara参数读取必须沿同一上游流动，不能把当前模块的输出反馈给自己的覆盖参数。
        assert key not in visiting,'Niagara运动图存在参数回环'
        if key in visited:
            return
        visiting.add(key)
        for target in adjacency[key]:
            visit(target)
        visiting.remove(key)
        visited.add(key)
    for key in nodes:
        visit(key)
    incoming=[e for e in edges if e['to_node_guid']==function['node_guid'] and e['to_pin_name']=='InputMap']
    assert len(incoming)==1,'运动模块输入链断开'
    override=incoming[0]['from_node_guid']
    assert nodes[override].get('class')=='NiagaraNodeParameterMapSet','缺少八个输入的覆盖节点'
    upstream=[e for e in edges if e['to_node_guid']==override and e['to_pin_name']=='Source']
    assert len(upstream)==1,'覆盖节点必须有唯一上游'
    def map_origin(edge):
        """动态输入可增加合法覆盖节点；沿Source回溯同一个ParameterMap根，不误判为下游读取。"""
        key=edge['from_node_guid']
        while nodes[key].get('class')=='NiagaraNodeParameterMapSet':
            parents=[e for e in edges if e['to_node_guid']==key and e['to_pin_name']=='Source']
            assert len(parents)==1,'覆盖链没有唯一上游'
            key=parents[0]['from_node_guid']
        assert nodes[key].get('class')=='NiagaraNodeInput','参数读取不来自图根'
        return key
    source=map_origin(upstream[0])
    bindings={'Clock':'System.Age','Individual':'Particles.MaterialRandom','Radius':'User.FlockRadius',
              'Height':'User.FlockHeight','Speed':'User.FlightSpeed','Seed':'User.FlockSeed',
              'Mode':'User.FlightMode','Wind':'User.WindInfluence'}
    for name,binding in bindings.items():
        links=[e for e in edges if e['to_node_guid']==override and
               e['to_pin_name']=='NM_GP_BirdFlockMotion.'+name]
        assert len(links)==1,'运动模块输入错绑：'+name
        reader=links[0]['from_node_guid']
        if name in ('Mode','Seed'):
            converter=nodes[reader]
            assert converter.get('function_script','').split('.')[0]=='/Niagara/DynamicInputs/TypeConversions/MakeFloatFromInt', 'Mode/Seed必须从整数Schema经原生转换进入既有float接口'
            maps=[e for e in edges if e['to_node_guid']==reader and e['to_pin_name']=='InputMap']
            assert len(maps)==1 and nodes[maps[0]['from_node_guid']].get('class')=='NiagaraNodeParameterMapSet','整数转换缺少覆盖节点'
            prefix=converter['function_name']+'.'
            values=[e for e in edges if e['to_node_guid']==maps[0]['from_node_guid'] and e['to_pin_name'].startswith(prefix)]
            assert len(values)==1 and values[0]['from_pin_name']==binding,'整数转换未读取正式User参数：'+name
            reader=values[0]['from_node_guid']
        else:
            assert links[0]['from_pin_name']==binding,'运动模块输入错绑：'+name
        assert nodes[reader].get('class')=='NiagaraNodeParameterMapGet','输入必须从真实参数图读取'
        reads=[e for e in edges if e['to_node_guid']==reader and e['to_pin_name']=='Source']
        assert len(reads)==1 and map_origin(reads[0])==source, '输入读取了当前模块下游：'+name
    return {'bindingCount':len(bindings),'integerConversions':2,'acyclic':True,'inputMapOrphaned':False}


def validate_user_parameter_types(parameters,allow_legacy_float_shadows=False):
    """正式整数必须唯一；明确许可时登记未使用Float影子，消费类型仍由转换图门禁证明。"""
    expected={'BirdCount':'NiagaraInt32','FlightMode':'NiagaraInt32','FlockSeed':'NiagaraInt32',
              'FlockRadius':'NiagaraFloat','FlockHeight':'NiagaraFloat','FlightSpeed':'NiagaraFloat',
              'WindInfluence':'NiagaraFloat','FlockMinimumSpacing':'NiagaraFloat'}
    for name,kind in expected.items():
        entries=[p for p in parameters if p['name'].removeprefix('User.')==name]
        if allow_legacy_float_shadows and name in ('FlightMode','FlockSeed'):
            assert all(p['type'] in ('NiagaraInt32','NiagaraFloat') for p in entries)
            assert sum(p['type']=='NiagaraFloat' for p in entries)<=1,'出现多份旧Float影子'
            entries=[p for p in entries if p['type']=='NiagaraInt32']
        assert len(entries)==1 and entries[0]['type']==kind,'User字段同名多类型或Schema类型不符：'+name
    return {'schemaTypes':expected,'legacyFloatShadows':[
        p for p in parameters if p['name'].removeprefix('User.') in ('FlightMode','FlockSeed') and p['type']=='NiagaraFloat']}


def write_integer_defaults(path,defaults):
    """只修需要变化的整数默认；旧Float会被编辑元数据自动补回，不声称清理了受保护存量。

    Monolith按名只命中首项；确认为Float时，在一个原子批次暂移store影子并设置原Int。
    批次结束立即按类型回读整数；编译/冷重载后再验，不靠按名Getter混淆类型。
    """
    for name,desired in defaults.items():
        parameters=niagara('export_system_spec',asset_path=path,include_values=True)['spec']['user_parameters']
        integer=[p for p in parameters if p['name'].removeprefix('User.')==name and p['type']=='NiagaraInt32']
        assert len(integer)==1,'整数原字段丢失，停止而不猜补：'+name
        if int(integer[0]['default'])==desired:
            continue
        first=niagara('get_parameter_value',asset_path=path,parameter='User.'+name)
        operations=[]
        if first['type']=='NiagaraFloat':
            operations.append({'op':'remove_user_parameter','name':'User.'+name})
        else:
            assert first['type']=='NiagaraInt32','未支持参数类型：'+name
        operations.append({'op':'set_parameter_default','parameter':'User.'+name,'value':desired})
        niagara('batch_execute',asset_path=path,operations=operations)
        after=niagara('export_system_spec',asset_path=path,include_values=True)['spec']['user_parameters']
        actual=[p for p in after if p['name'].removeprefix('User.')==name and p['type']=='NiagaraInt32']
        assert len(actual)==1 and int(actual[0]['default'])==desired,'整数默认未生效，保留断点：'+name


def inspect_motion_contract(paths):
    """只读核对模块输入暴露标志和系统实际调用T3D；用于保存后独立冷读门禁。"""
    native=editor_python('''import unreal,json
module='''+repr(MODULE)+'''
unreal.EditorAssetLibrary.load_asset(module)
inputs=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith(module+'.')]
assert len(inputs)==1,'运动模块ParameterMap输入不唯一'
options=inputs[0].get_editor_property('exposure_options')
assert options.get_editor_property('exposed') and options.get_editor_property('required') and not options.get_editor_property('hidden'),options.export_text()
rows={}
for p in '''+repr(list(paths))+''':
    unreal.EditorAssetLibrary.load_asset(p)
    nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall) if n.get_path_name().startswith(p+'.') and n.get_editor_property('function_script') and n.get_editor_property('function_script').get_path_name().split('.')[0]==module]
    assert len(nodes)==1,(p,len(nodes))
    rows[p]=nodes[0].get_name()
print('BIRD_MOTION_CONTRACT '+json.dumps({'exposure':options.export_text(),'calls':rows}))
''')
    payload=None
    for output in native.get('output',[]):
        for line in output.get('output','').splitlines():
            if line.startswith('BIRD_MOTION_CONTRACT '):
                payload=json.loads(line.removeprefix('BIRD_MOTION_CONTRACT '))
    assert payload and set(payload['calls'])==set(paths),'未取得真实模块暴露合同'
    result={'exposure':payload['exposure'],'systems':{}}
    for path,node in payload['calls'].items():
        graph=niagara('get_module_graph',script_path=path+'.'+path.rsplit('/',1)[1]+':BirdFlock_0.UpdateScript',links=True)
        exported=call('project_query',{'action':'export_asset_text','params':{
            'asset_path':path,'object_filter':node,'max_bytes':32000}})
        text=exported['text']
        assert exported['full_bytes']==exported['returned_bytes'],'调用节点T3D被截断'
        summary=validate_motion_graph(graph,text)
        result['systems'][path]={'summary':summary,'graph':graph,'callT3D':text}
    return result


def refresh_motion_contract(paths,integer_defaults=None):
    """编辑器作者步骤：修正原模块暴露合同，刷新同一模块调用并重绑原八个输入。

    不创建第二个模块/系统；None只用于原生触发调用引脚重配，随即恢复原模块。
    单独赋同值不会刷新部分旧节点；继续沿用旧孤立引脚会在保存重载后损坏GPU图。
    """
    editor_python('''import unreal
module='''+repr(MODULE)+'''
script=unreal.EditorAssetLibrary.load_asset(module)
inputs=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith(module+'.')]
assert len(inputs)==1
node=inputs[0]
options=node.get_editor_property('exposure_options')
options.set_editor_property('exposed',True)
options.set_editor_property('required',True)
options.set_editor_property('hidden',False)
node.set_editor_property('exposure_options',options)
for path in '''+repr(list(paths))+''':
    unreal.EditorAssetLibrary.load_asset(path)
    nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall) if n.get_path_name().startswith(path+'.') and n.get_editor_property('function_script') and n.get_editor_property('function_script').get_path_name().split('.')[0]==module]
    assert len(nodes)==1,(path,len(nodes))
    nodes[0].set_editor_property('function_script',None)
    nodes[0].set_editor_property('function_script',script)
    print('BIRD_MOTION_CALL_REFRESH '+path)
''')
    for path in paths:
        parameters=niagara('export_system_spec',asset_path=path,include_values=True)['spec']['user_parameters']
        defaults={}
        for name in ('FlightMode','FlockSeed'):
            choices=[p for p in parameters if p['name'].removeprefix('User.')==name]
            # 旧Mode的可见轨迹来自Float；Seed始终以已发布整数默认值为准，不能继承误生的Float零值。
            preferred='NiagaraFloat' if name=='FlightMode' and any(p['type']=='NiagaraFloat' for p in choices) else 'NiagaraInt32'
            selected=[p for p in choices if p['type']==preferred]
            assert len(selected)==1,'迁移前参数类型不明确：'+name
            defaults[name]=int(float(selected[0]['default']))
        defaults.update((integer_defaults or {}).get(path,{}))
        for name,binding in {'Clock':'System.Age','Individual':'Particles.MaterialRandom',
                'Radius':'User.FlockRadius','Height':'User.FlockHeight','Speed':'User.FlightSpeed',
                'Seed':'User.FlockSeed','Mode':'User.FlightMode','Wind':'User.WindInfluence'}.items():
            if name not in ('Mode','Seed'):
                niagara('set_module_input_binding',asset_path=path,emitter=EMITTER,
                        module_node='NM_GP_BirdFlockMotion',input=name,binding=binding)
                continue
            # 原生Int→Float动态输入保持模块8个接口不变，真正消费Definition整数覆盖，不再影子读取Float。
            added=niagara('add_dynamic_input',asset_path=path,emitter=EMITTER,module_node='NM_GP_BirdFlockMotion',
                input=name,dynamic_input_script='/Niagara/DynamicInputs/TypeConversions/MakeFloatFromInt.MakeFloatFromInt')
            inputs=[p for p in added['inputs'] if p['type']=='NiagaraInt32']
            assert len(inputs)==1,'引擎整数转换输入与已核对版本不符，停止作者'
            niagara('set_module_input_binding',asset_path=path,emitter=EMITTER,
                module_node=added['dynamic_input_node_guid'],input=inputs[0]['name'],binding=binding)
        write_integer_defaults(path,defaults)
        validate_user_parameter_types(niagara('export_system_spec',asset_path=path,include_values=True)['spec']['user_parameters'],allow_legacy_float_shadows=True)
    return inspect_motion_contract(paths)


def author_base():
    shader=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    print('MODULE',niagara('create_module_from_hlsl',name='BirdFlockMotion',save_path=MODULE,hlsl=shader,
        inputs=[{'name':n,'type':'float'} for n in ['Clock','Individual','Radius','Height','Speed','Seed','Mode','Wind']],
        outputs=[],description='通用鸟群GPU盘旋/掠过/八字路径，局部安全体积，无权威碰撞。'))
    refresh_motion_contract([])
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
    refresh_motion_contract([BASE])
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
    # Minimal模板默认由System控制生命周期，隐藏的Loop Once配置不会生效。
    # 必须先改为Self，再配置单轮；否则System每5秒回环会再次Burst并无限增长。
    print('SELF_LIFECYCLE',niagara('set_static_switch_value',asset_path=BASE,emitter=EMITTER,
          module_node='EmitterState',input='Life Cycle Mode',value='Self'))
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
        if name in ('Mode','Seed'):
            continue
        print('REBIND',name,niagara('set_module_input_binding',asset_path=BASE,emitter=EMITTER,
                                  module_node='NM_GP_BirdFlockMotion',input=name,binding=binding))
    # EmitterState重新排到Burst之前后，同样重建数量绑定的读取来源。
    print('REBIND_COUNT',niagara('set_module_input_binding',asset_path=BASE,emitter=EMITTER,
         module_node='SpawnBurst_Instantaneous',input='Spawn Count',binding='User.BirdCount'))
    refresh_motion_contract([BASE])
    result=niagara('get_system_diagnostics',asset_path=BASE,compile_first=True)
    print('DIAGNOSTICS',result.get('error_count'),result.get('warning_count'),json.dumps(result.get('errors',[])[:3],ensure_ascii=False))
    if result.get('error_count',1):
        raise RuntimeError('真实GPU图仍存在编译错误，保留完整JSON证据')
    print('SAVE',niagara('save_system',asset_path=BASE))

def inspect_route_assets(paths):
    """只读真实渲染网格/材质和Definition，审计原固定XYZ Bounds；不依赖历史网格报告。"""
    renderer_materials={}
    for path in paths:
        renderers=niagara('list_renderers',asset_path=path,emitter=EMITTER)['renderers']
        assert len(renderers)==1 and renderers[0]['type']=='mesh','必须唯一实际网格渲染器'
        # 官方动作已判断bOverrideMaterials；不能看到残留OverrideMaterials数组就误认它正在生效。
        renderer_materials[path]=renderers[0].get('material')
    result=editor_python('''import unreal,json,math
assert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
a=unreal.EditorAssetLibrary
renderer_materials='''+repr(renderer_materials)+'''
renderer_class=unreal.load_class(None,'/Script/Niagara.NiagaraMeshRendererProperties')
assert renderer_class is not None,'无法反射真实Niagara网格渲染器UClass'
rows={}
for path in '''+repr(list(paths))+''':
    a.load_asset(path)
    species='Base' if path=='''+repr(BASE)+''' else path.rsplit('_',1)[1]
    # 沿用已验证的Fauna作者：load_class反射UClass，不依赖未公开Python类。
    # 任意包装对象的同名方法可能遮蔽Object方法，路径统一走公开SystemLibrary静态API。
    renderers=[r for r in unreal.ObjectIterator(renderer_class) if unreal.SystemLibrary.get_path_name(r).startswith(path+'.')]
    assert len(renderers)==1,(path,'真实网格渲染器数量异常')
    renderer=renderers[0]
    slots=list(renderer.get_editor_property('Meshes'))
    assert len(slots)==1,'不支持未审计的多网格变体'
    slot=slots[0]
    mesh=slot.get_editor_property('mesh')
    expected='/Engine/BasicShapes/Plane' if species=='Base' else '''+repr(DBA)+'''+'/Meshes/Flight/SM_DBA_Bird_'+species
    assert mesh and unreal.SystemLibrary.get_path_name(mesh).split('.')[0]==expected,'渲染网格与预检物种不符'
    scale=slot.get_editor_property('scale')
    scale_max=max(abs(scale.x),abs(scale.y),abs(scale.z))
    pivot=slot.get_editor_property('pivot_offset')
    pivot_length=math.sqrt(pivot.x*pivot.x+pivot.y*pivot.y+pivot.z*pivot.z)
    bounds=mesh.get_bounds()
    origin=bounds.origin
    origin_length=math.sqrt(origin.x*origin.x+origin.y*origin.y+origin.z*origin.z)
    material=a.load_asset(renderer_materials[path]) if renderer_materials[path] else mesh.get_material(0)
    assert material,'网格未绑定可审计材质'
    if isinstance(material,unreal.MaterialInstanceConstant):
        amplitude=float(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,'WingAmplitudeCm'))
    else:
        assert isinstance(material,unreal.Material)
        amplitude=float(unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(material,'WingAmplitudeCm'))
    assert math.isfinite(scale_max) and scale_max>0 and math.isfinite(amplitude) and amplitude>=0
    margin=((float(bounds.sphere_radius)+origin_length)*scale_max+pivot_length)*1.11+amplitude+7.5
    row={'species':species,'mesh':unreal.SystemLibrary.get_path_name(mesh),'material':unreal.SystemLibrary.get_path_name(material),
        'meshSphereRadiusCm':float(bounds.sphere_radius),'boundsOriginLengthCm':origin_length,
        'rendererScaleMaximum':scale_max,'pivotOffsetLengthCm':pivot_length,
        'wingAmplitudeCm':amplitude,'meshMarginCm':margin}
    if species!='Base':
        definition=a.load_asset('''+repr(DBA)+'''+'/Definitions/DA_DBA_VFX_BirdFlock_'+species)
        assert isinstance(definition,unreal.GamePlatformVFXWorldDefinition)
        parameters=definition.get_editor_property('default_parameters')
        row['definitionIntegers']={str(k):int(v) for k,v in parameters.get_editor_property('integer_parameters').items()}
        row['definitionFloats']={str(k):float(v) for k,v in parameters.get_editor_property('float_parameters').items()}
        rules=definition.get_editor_property('parameter_schema').get_editor_property('rules')
        for name in ['User.FlightMode','User.FlockSeed']:
            matches=[r for r in rules if str(r.get_editor_property('name'))==name]
            assert len(matches)==1 and 'INTEGER' in str(matches[0].get_editor_property('type')).upper(),'Definition整数Schema发生变化'
    rows[path]=row
print('BIRD_ROUTES_ASSETS '+json.dumps(rows))
''')
    rows=None
    for output in result.get('output',[]):
        for line in output.get('output','').splitlines():
            if line.startswith('BIRD_ROUTES_ASSETS '):
                rows=json.loads(line.removeprefix('BIRD_ROUTES_ASSETS '))
    assert rows and set(rows)==set(paths),'未取得真实渲染网格和材质预检'
    for path,row in rows.items():
        spec=niagara('export_system_spec',asset_path=path,include_values=True)['spec']
        assert len(spec['emitters'])==1 and spec['emitters'][0]['calculate_bounds_mode']=='Fixed'
        bounds=spec['system_properties']
        assert bounds['fixed_bounds'],'系统未启用固定Bounds'
        expected=[-24000.0,-16000.0,-1000.0,24000.0,16000.0,12000.0]
        system_box=[float(v) for key in ('min','max') for v in bounds['bounds'][key].strip('()').split(',')]
        emitter_box=niagara('get_emitter_property',asset_path=path,emitter=EMITTER,property='FixedBounds')['value']
        emitter_values=[float(v) for v in re.findall(r'[XYZ]=([-+0-9.eE]+)',str(emitter_box))]
        assert system_box==expected and emitter_values==expected,'原系统/发射器Bounds不一致；停止而不扩大'
        row['spec']=spec
        row['systemBoundsCm']=system_box
        row['emitterBoundsCm']=emitter_values
    return rows


def repair_routes(resume=False):
    """主流程独占Monolith时更新自然路线、整数桥接与海鸥Mode，不重放间距修复。

    独立RoutesBefore保留原HLSL/四系统/定义参数与旧GPU报告；失败停止，不自动重试写操作。
    resume由主流程核对同一批半完成资源后显式调用；不覆盖SpacingBefore或旧GPU证据。
    """
    before_path=EVIDENCE/'BirdFlockRoutesBefore.json'
    if before_path.exists() and not resume:
        raise RuntimeError('RoutesBefore已存在；核对半完成状态后显式--resume-routes，禁止盲目重放。')
    if resume and not before_path.is_file():
        raise RuntimeError('缺少原RoutesBefore，拒绝猜测续跑。')
    paths=[BASE]+[DBA+'/Niagara/NS_DBA_BirdFlock_'+s for s in ('Seagull','Swallow','Eagle')]
    assets=inspect_route_assets(paths)
    defaults={}
    audits={}
    for path,row in assets.items():
        parameters=row['spec']['user_parameters']
        def read(name):
            values=[p for p in parameters if p['name'].removeprefix('User.')==name]
            assert len(values)==1,'物种运动基础参数不唯一：'+name
            return float(values[0]['default'])
        spacing=read('FlockMinimumSpacing')
        assert spacing=={'Base':220.0,'Seagull':240.0,'Swallow':180.0,'Eagle':380.0}[row['species']], '不静默改变已验收间距'
        radius,height=read('FlockRadius'),read('FlockHeight')
        default_count=int(read('BirdCount'))
        formal=24 if row['species']=='Swallow' else default_count
        modes=[]
        for mode in (0,1,2):
            for count in sorted({formal,default_count,128,512}):
                for r,h in ((radius,height),(10000.0,10000.0)):
                    envelope=route_envelope(r,h,count,spacing,mode,2000.0,row['meshMarginCm'])
                    assert envelope['withinFixedBounds'],'路线组合越过真实网格/WPO Bounds，拒绝写入：'+str(envelope)
                    modes.append(envelope)
        audits[path]=modes
        mode={'Base':0,'Seagull':1,'Swallow':2,'Eagle':0}[row['species']]
        seed=137 if row['species']=='Base' else row['definitionIntegers']['User.FlockSeed']
        defaults[path]={'FlightMode':mode,'FlockSeed':seed}
        if row['species']!='Base':
            assert row['definitionFloats']['User.FlockMinimumSpacing']==spacing,'Definition与真实系统间距不一致'
    shader=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    if resume:
        previous=json.loads(before_path.read_text(encoding='utf-8'))
        assert set(previous['assets'])==set(paths),'续跑资源身份不符'
    else:
        before={'module':niagara('get_custom_hlsl_text',script_path=MODULE),'assets':assets}
        gpu_path=ROOT/'Saved/Validation/BirdFlockP4P7/GPUReview/SpacingReview/GPUReview.json'
        if gpu_path.is_file():
            # 下一轮18个GPU样本仍走原入口；先保存旧报告完整内容和哈希，不能带入新路线结论。
            raw=gpu_path.read_bytes()
            before['previousGPUReportSha256']=hashlib.sha256(raw).hexdigest()
            before['previousGPUReport']=json.loads(raw)
        with before_path.open('x',encoding='utf-8') as stream:
            json.dump(before,stream,ensure_ascii=False,indent=2)
    if niagara('get_custom_hlsl_text',script_path=MODULE).get('hlsl')!=shader:
        niagara('set_custom_hlsl_text',script_path=MODULE,hlsl=shader)
    contract=refresh_motion_contract(paths,integer_defaults=defaults)
    editor_python('''import unreal
a=unreal.EditorAssetLibrary
path='''+repr(DBA+'/Definitions/DA_DBA_VFX_BirdFlock_Seagull')+'''
d=a.load_asset(path)
parameters=d.get_editor_property('default_parameters')
integers={str(k):int(v) for k,v in parameters.get_editor_property('integer_parameters').items()}
if integers.get('User.FlightMode')!=1:
    integers['User.FlightMode']=1
    parameters.set_editor_property('integer_parameters',integers)
    d.set_editor_property('default_parameters',parameters)
    version=d.get_editor_property('data_version')
    replacement=unreal.GamePlatformDataVersion()
    assert replacement.import_text('(SchemaVersion=%d,ContentRevision=%d)'%(int(version.get_editor_property('schema_version')),int(version.get_editor_property('content_revision'))+1))
    d.set_editor_property('data_version',replacement)
    assert a.save_loaded_asset(d,only_if_is_dirty=False)
print('BIRD_ROUTE_SEAGULL_DEFINITION_MODE_1')
''')
    diagnostics={}
    for path in paths:
        diagnostic=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        assert diagnostic.get('error_count',1)==0 and diagnostic.get('warning_count',1)==0,(path,diagnostic)
        assert all(s['compile_status']=='UpToDate' for e in diagnostic['emitter_stats'] for s in e['scripts'])
        validate_user_parameter_types(niagara('export_system_spec',asset_path=path,include_values=True)['spec']['user_parameters'],allow_legacy_float_shadows=True)
        niagara('save_system',asset_path=path,only_if_dirty=False)
        diagnostics[path]=diagnostic
    niagara('save_system',asset_path=MODULE,only_if_dirty=False)
    (EVIDENCE/'BirdFlockRoutesRepair.json').write_text(json.dumps({
        'status':'saved_pending_native_reload_gpu18_and_manual_review','sourceSha256':hashlib.sha256(shader.encode()).hexdigest(),
        'assets':assets,'boundsAudit':audits,'graphContract':contract,'diagnostics':diagnostics},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_ROUTES_SAVED_PENDING_NATIVE_RELOAD_GPU18_AND_MANUAL')


def repair_spacing(resume_definitions=False):
    """增量修复四个现有系统：预检工程/网格后加参数、替换现有HLSL、编译并保存。

    必须由主流程协调独占UE编辑器后显式调用；不重建模块、发射器、系统或项目定义身份。
    失败保留逐步Monolith请求证据并中止，不自动重放或掩盖半完成结果；独立重载和GPU验收另行执行。
    resume_definitions仅适用于原编辑器仍保留参数/Bounds/HLSL的半完成状态；只读核对后续做调用图合同、定义和保存。
    """
    before_path=EVIDENCE/'BirdFlockSpacingBefore.json'
    if resume_definitions:
        if not before_path.is_file():
            raise RuntimeError('没有原Before证据，不能确认待续的资产操作；停止。')
    elif before_path.exists():
        raise RuntimeError('原Before证据已存在，拒绝覆盖或盲目重放；核对原编辑器后使用--resume-spacing-definitions。')
    preflight = editor_python('''import unreal, json, math
assert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
a=unreal.EditorAssetLibrary
root='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
rows=[]
for species,planned in [('Seagull',220.0),('Swallow',140.0),('Eagle',340.0)]:
    mesh=a.load_asset(root+'/Meshes/Flight/SM_DBA_Bird_'+species)
    material=a.load_asset(root+'/Materials/MI_DBA_Bird_'+species)
    definition=a.load_asset(root+'/Definitions/DA_DBA_VFX_BirdFlock_'+species)
    system=a.load_asset(root+'/Niagara/NS_DBA_BirdFlock_'+species)
    assert isinstance(mesh,unreal.StaticMesh) and isinstance(material,unreal.MaterialInstanceConstant)
    assert isinstance(definition,unreal.GamePlatformVFXWorldDefinition) and isinstance(system,unreal.NiagaraSystem)
    radius=float(mesh.get_bounds().sphere_radius)
    amplitude=float(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,'WingAmplitudeCm'))
    assert math.isfinite(radius) and radius>0 and math.isfinite(amplitude) and amplitude>=0
    # 包围球直径覆盖任意朝向/侧倾；1.11为既有最大模型Scale，WPO在两鸟相向振翅时各计一次。
    required=2.0*radius*1.11+2.0*amplitude+15.0
    spacing=max(planned,math.ceil(required/20.0)*20.0)
    assert spacing<=1000.0, '网格/材质尺寸异常，拒绝自动扩大群落'
    rows.append({'species':species,'meshSphereRadiusCm':radius,'wingAmplitudeCm':amplitude,'minimumSpacingCm':spacing})
print('BIRD_SPACING_PREFLIGHT '+json.dumps(rows))
''')
    rows = None
    for output in preflight.get('output', []):
        for line in output.get('output', '').splitlines():
            if line.startswith('BIRD_SPACING_PREFLIGHT '):
                rows = json.loads(line.removeprefix('BIRD_SPACING_PREFLIGHT '))
    if not rows or {row['species'] for row in rows} != {'Seagull','Swallow','Eagle'}:
        raise RuntimeError('Monolith没有返回真实三物种尺寸预检；未修改鸟群资产。')

    # 原图、原系统规格先留证，便于定位和由人工决定回退；不通过复制生成第二套运行资产。
    paths = {BASE:220.0}
    for row in rows:
        paths[DBA+'/Niagara/NS_DBA_BirdFlock_'+row['species']] = row['minimumSpacingCm']
    if resume_definitions:
        before=json.loads(before_path.read_text(encoding='utf-8'))
        if not before.get('module') or set(before.get('systems',{}))!=set(paths):
            raise RuntimeError('原Before证据的模块/四系统身份不匹配，拒绝猜测续跑。')
    else:
        before = {'module':niagara('get_custom_hlsl_text',script_path=MODULE), 'systems':{}}
        for path in paths:
            before['systems'][path] = niagara('export_system_spec',asset_path=path,include_values=True)
        # 排他创建保护最早快照；即使另一次进程并行开始，也不能覆盖已留下的原始证据。
        with before_path.open('x',encoding='utf-8') as stream:
            json.dump(before,stream,ensure_ascii=False,indent=2)

    bounds_evidence = {}
    for path, spacing in paths.items():
        current = unpack(niagara('get_user_parameters',asset_path=path))
        if not isinstance(current, list):
            raise RuntimeError('无法回读系统User参数，停止修改：'+path)
        has_spacing=any(parameter['name'].removeprefix('User.') == 'FlockMinimumSpacing' for parameter in current)
        if resume_definitions:
            if not has_spacing or float(niagara('get_parameter_value',asset_path=path,parameter='User.FlockMinimumSpacing')['value'])!=spacing:
                raise RuntimeError('四系统间距预阶段未保留在当前编辑器，不能从定义阶段续跑：'+path)
        else:
            if not has_spacing:
                niagara('add_user_parameter',asset_path=path,name='User.FlockMinimumSpacing',type='float',default=str(spacing))
            niagara('set_parameter_default',asset_path=path,parameter='User.FlockMinimumSpacing',value=str(spacing))
        # 全时XYZ审计必须随路线公式更新；当前原Bounds保持不变，未支持组合拒绝而不任意扩大。
        radius = float(niagara('get_parameter_value',asset_path=path,parameter='User.FlockRadius')['value'])
        height = float(niagara('get_parameter_value',asset_path=path,parameter='User.FlockHeight')['value'])
        mode = int(float(niagara('get_parameter_value',asset_path=path,parameter='User.FlightMode')['value']))
        default_count = int(float(niagara('get_parameter_value',asset_path=path,parameter='User.BirdCount')['value']))
        runtime_count = 24 if path.endswith('_Swallow') else max(1,default_count)
        envelopes=[route_envelope(radius,height,count,spacing,mode,2000.0,spacing*0.5)
                   for count in sorted({max(1,default_count),runtime_count,512})]
        assert all(e['withinFixedBounds'] for e in envelopes),'间距/路线组合越出原Bounds，先重新制作审计'
        bounds_evidence[path]={'authoredMode':mode,'radiusCm':radius,'heightCm':height,
            'niagaraDefaultCount':default_count,'formalWorldDefaultCount':runtime_count,'allowedMaximumCount':512,
            'minimumSpacingCm':spacing,'envelopes':envelopes,'fixedBoundsMaxZCm':12000,
            'recomputeRequiredWhenChanging':['FlightMode','FlockRadius','FlockHeight','FlockMinimumSpacing']}
        if not resume_definitions:
            niagara('set_fixed_bounds',asset_path=path,min=[-24000,-16000,-1000],max=[24000,16000,12000],enabled=True)
            niagara('set_fixed_bounds',asset_path=path,emitter=EMITTER,min=[-24000,-16000,-1000],max=[24000,16000,12000],enabled=True)
    shader=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    if resume_definitions:
        if niagara('get_custom_hlsl_text',script_path=MODULE).get('hlsl')!=shader:
            raise RuntimeError('当前编辑器HLSL与待续源码不一致，不能跳过预阶段。')
    else:
        niagara('set_custom_hlsl_text',script_path=MODULE,hlsl=shader)

    graph_contract=refresh_motion_contract(paths)

    # 原@1定义新增可选字段，保留所有既有规则、默认值和逻辑身份；不重放旧创建脚本。
    editor_python('''import unreal, json
a=unreal.EditorAssetLibrary
rows='''+repr(rows)+'''
for row in rows:
    path='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Definitions/DA_DBA_VFX_BirdFlock_'+row['species']
    d=a.load_asset(path)
    schema=d.get_editor_property('parameter_schema')
    rules=list(schema.get_editor_property('rules'))
    names=[str(rule.get_editor_property('name')) for rule in rules]
    assert len(names)==len(set(names)), '旧定义参数重复，停止修改'
    # Rule与Schema均有EditDefaultsOnly字段，临时struct不能逐字段set_editor_property。
    # 沿用真实定义作者的import_text；旧规则由原生export_text保留其类型/边界/必填语义。
    rule_texts=[rule.export_text() for rule in rules]
    missing_rule='User.FlockMinimumSpacing' not in names
    if missing_rule:
        rule_texts.append('(Name="User.FlockMinimumSpacing",Type=Float,bRequired=False,MinValue=1.0,MaxValue=1000.0)')
    limit=int(schema.get_editor_property('max_override_count'))
    schema_changed=missing_rule or limit<len(rule_texts)
    if schema_changed:
        replacement_schema=unreal.GamePlatformVFXParameterSchema()
        text='(MaxOverrideCount=%d,Rules=(%s))'%(max(limit,len(rule_texts)),','.join(rule_texts))
        assert replacement_schema.import_text(text), '增量参数Schema解析失败，停止保存'
        d.set_editor_property('parameter_schema',replacement_schema)
    parameters=d.get_editor_property('default_parameters')
    floats={str(key):float(value) for key,value in parameters.get_editor_property('float_parameters').items()}
    changed=floats.get('User.FlockMinimumSpacing')!=row['minimumSpacingCm']
    if not schema_changed and not changed:
        print('BIRD_SPACING_DEFINITION_ALREADY_COMPLETE '+path)
        continue
    floats['User.FlockMinimumSpacing']=row['minimumSpacingCm']
    parameters.set_editor_property('float_parameters',floats)
    d.set_editor_property('default_parameters',parameters)
    # DataVersion同样是EditDefaultsOnly；保留表结构版本，内容变更只增加一次修订。
    version=d.get_editor_property('data_version')
    replacement_version=unreal.GamePlatformDataVersion()
    assert replacement_version.import_text('(SchemaVersion=%d,ContentRevision=%d)'%(
        int(version.get_editor_property('schema_version')),int(version.get_editor_property('content_revision'))+1))
    d.set_editor_property('data_version',replacement_version)
    assert a.save_loaded_asset(d,only_if_is_dirty=False)
    print('BIRD_SPACING_DEFINITION_SAVED '+path)
''')
    diagnostics = {}
    for path in paths:
        diagnostic = niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        diagnostics[path] = diagnostic
        if diagnostic.get('error_count',1) or diagnostic.get('warning_count',1):
            raise RuntimeError('鸟群间距真实Niagara编译未通过：'+path)
        niagara('save_system',asset_path=path,only_if_dirty=False)
    niagara('save_system',asset_path=MODULE,only_if_dirty=False)
    (EVIDENCE/'BirdFlockSpacingRepair.json').write_text(json.dumps({
        'status':'saved_pending_independent_reload_gpu_and_manual_review',
        'resumedFromDefinitions':resume_definitions,'species':rows,'bounds':bounds_evidence,
        'graphContract':graph_contract,'diagnostics':diagnostics},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_SPACING_SAVED_PENDING_RELOAD_GPU_AND_MANUAL',json.dumps(rows,ensure_ascii=False))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--apply-base',action='store_true');p.add_argument('--configure-base',action='store_true');p.add_argument('--repair-bindings',action='store_true');p.add_argument('--repair-spacing',action='store_true');p.add_argument('--resume-spacing-definitions',action='store_true');p.add_argument('--repair-routes',action='store_true');p.add_argument('--resume-routes',action='store_true');p.add_argument('--inspect-routes',action='store_true');a=p.parse_args()
    if a.apply_base:
        author_base()
    elif a.configure_base:
        configure_base()
    elif a.repair_bindings:
        repair_bindings()
    elif a.inspect_routes:
        rows=inspect_route_assets([BASE]+[DBA+'/Niagara/NS_DBA_BirdFlock_'+s for s in ('Seagull','Swallow','Eagle')])
        print('BIRD_ROUTES_READONLY_PREFLIGHT_PASS',len(rows),json.dumps(rows,ensure_ascii=False))
    elif a.repair_routes or a.resume_routes:
        repair_routes(resume=a.resume_routes)
    elif a.resume_spacing_definitions:
        repair_spacing(resume_definitions=True)
    elif a.repair_spacing:
        repair_spacing()
    else:
        print('BIRD_NIAGARA_PLAN',BASE,MODULE,PARAMS)
