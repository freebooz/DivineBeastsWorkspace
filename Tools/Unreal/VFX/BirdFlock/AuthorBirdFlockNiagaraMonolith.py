#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P4真实GPU鸟群系统作者：通用HLSL+Niagara模板，项目物种只覆盖网格/材质。
仅经已连接Monolith官方编辑器动作写入uasset，不碰当前关卡或其他任务的资源。
每一步保留JSON证据，异常不自动重放写入；inspect不写UE资源。
"""
import argparse
import json
import math
from BirdFlockMonolithProtocol import ROOT,EVIDENCE,niagara,editor_python
GP='/GamePlatformVFX/Environment/BirdFlock'
DBA='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
MODULE=GP+'/Niagara/NM_GP_BirdFlockMotion'
BASE=GP+'/Niagara/NS_GP_BirdFlock_Base'
EMITTER='BirdFlock'

PARAMS=[('BirdCount','int',32),('FlockRadius','float',1700.0),('FlockHeight','float',850.0),
        ('FlightSpeed','float',250.0),('FlockSeed','int',137),('FlightMode','int',0),('WindInfluence','float',80.0),
        ('FlockMinimumSpacing','float',220.0)]

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

def repair_spacing(resume_definitions=False):
    """增量修复四个现有系统：预检工程/网格后加参数、替换现有HLSL、编译并保存。

    必须由主流程协调独占UE编辑器后显式调用；不重建模块、发射器、系统或项目定义身份。
    失败保留逐步Monolith请求证据并中止，不自动重放或掩盖半完成结果；独立重载和GPU验收另行执行。
    resume_definitions仅适用于原编辑器仍保留参数/Bounds/HLSL的半完成状态；只读核对后续做定义和保存。
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
        # 真实组件/服务/Schema均允许0..512，正式入口只覆盖Count，模式/半径/高度来自系统默认值。
        # 按当前模式保留512合同；不得为了未请求的八字鹰群扩大默认盘旋系统的剔除体积。
        radius = float(niagara('get_parameter_value',asset_path=path,parameter='User.FlockRadius')['value'])*0.9
        height = float(niagara('get_parameter_value',asset_path=path,parameter='User.FlockHeight')['value'])
        mode = int(float(niagara('get_parameter_value',asset_path=path,parameter='User.FlightMode')['value']))
        default_count = int(float(niagara('get_parameter_value',asset_path=path,parameter='User.BirdCount')['value']))
        assert radius>0 and height>0 and mode in (0,1,2) and 0<=default_count<=512
        single_lane_limit = 2.0*radius
        if mode == 1:
            single_lane_limit *= 0.42
        if mode == 2:
            single_lane_limit = 0.65*radius
        capacity = max(1,math.floor(math.pi/math.asin(min(1.0,spacing/single_lane_limit))))
        if spacing > single_lane_limit:
            capacity = 1
        envelopes = []
        # 燕子资产默认72只，Village正式入口覆盖为24只；二者均留证，不能混作同一个默认场景。
        runtime_count = 24 if path.endswith('_Swallow') else max(1,default_count)
        for count in sorted({max(1,default_count),runtime_count,512}):
            lanes = max(2 if mode==2 else 1,math.ceil(count/capacity))
            lanes = math.ceil(count/math.ceil(count/lanes))
            span = (lanes-1)*spacing
            center = max(height,height*0.35+span*0.5+42.0)
            envelopes.append({'count':count,'laneCount':lanes,
                'centerZMinCm':center-span*0.5-42.0,'centerZMaxCm':center+span*0.5+42.0})
        maximum_z = max(12000,math.ceil((envelopes[-1]['centerZMaxCm']+spacing)/1000.0)*1000)
        bounds_evidence[path]={'authoredMode':mode,'radiusCm':radius/0.9,'heightCm':height,
            'niagaraDefaultCount':default_count,'formalWorldDefaultCount':runtime_count,'allowedMaximumCount':512,
            'minimumSpacingCm':spacing,'envelopes':envelopes,'fixedBoundsMaxZCm':maximum_z,
            'recomputeRequiredWhenChanging':['FlightMode','FlockRadius','FlockHeight','FlockMinimumSpacing']}
        if not resume_definitions:
            niagara('set_fixed_bounds',asset_path=path,min=[-24000,-16000,-1000],max=[24000,16000,maximum_z],enabled=True)
            niagara('set_fixed_bounds',asset_path=path,emitter=EMITTER,min=[-24000,-16000,-1000],max=[24000,16000,maximum_z],enabled=True)
    shader=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    if resume_definitions:
        if niagara('get_custom_hlsl_text',script_path=MODULE).get('hlsl')!=shader:
            raise RuntimeError('当前编辑器HLSL与待续源码不一致，不能跳过预阶段。')
    else:
        niagara('set_custom_hlsl_text',script_path=MODULE,hlsl=shader)

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
        'resumedFromDefinitions':resume_definitions,'species':rows,'bounds':bounds_evidence,'diagnostics':diagnostics},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_SPACING_SAVED_PENDING_RELOAD_GPU_AND_MANUAL',json.dumps(rows,ensure_ascii=False))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--apply-base',action='store_true');p.add_argument('--configure-base',action='store_true');p.add_argument('--repair-bindings',action='store_true');p.add_argument('--repair-spacing',action='store_true');p.add_argument('--resume-spacing-definitions',action='store_true');a=p.parse_args()
    if a.apply_base:
        author_base()
    elif a.configure_base:
        configure_base()
    elif a.repair_bindings:
        repair_bindings()
    elif a.resume_spacing_definitions:
        repair_spacing(resume_definitions=True)
    elif a.repair_spacing:
        repair_spacing()
    else:
        print('BIRD_NIAGARA_PLAN',BASE,MODULE,PARAMS)
