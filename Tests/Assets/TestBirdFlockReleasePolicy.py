#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群发布门禁回归；合成CSV仅测试策略，不伪装实际Cook或GPU运行。
导入真实校验器，验证19项资源、历史模型/附属块/开发图禁入和输入错误；
同时核对实际两个客户端配置。只写临时目录，不改项目资源或历史证据。
"""
from pathlib import Path
import csv
import importlib.util
import ast
import math
import random
import re
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('bird_release', ROOT/'Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockContainer.py')
assert SPEC and SPEC.loader
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
DBA = '../../../DivineBeastsArena/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock'
GP = '../../../DivineBeastsArena/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock'


class ShaderVector:
    """只实现现行鸟群HLSL位置公式使用的向量运算；不模拟Niagara调度或GPU浮点误差。"""
    def __init__(self, *values):
        self.values = tuple(values)

    def __add__(self, other):
        return ShaderVector(*(a+b for a, b in zip(self.values, other.values)))

    def __mul__(self, scalar):
        return ShaderVector(*(a*scalar for a in self.values))

    __rmul__ = __mul__

    @property
    def xy(self):
        return ShaderVector(*self.values[:2])

    @xy.setter
    def xy(self, value):
        self.values = value.values + self.values[2:]

    @property
    def z(self):
        return self.values[2]

    @z.setter
    def z(self, value):
        self.values = self.values[:2] + (value,)


def compile_motion_position():
    """逐句执行真实HLSL至Position输出：测试不抄写轨迹公式，源码改变即改变被测行为。

    仅转译本模块已有的标量声明、if分支及float2/3；未知语法直接失败。
    返回CPU双精度位置求值器；实际Niagara编译、唯一ID绑定和GPU读回仍由引擎门禁确认。
    """
    path = ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl'
    source = path.read_text(encoding='utf-8').split('float yaw =', 1)[0]
    statements = re.sub(r'//[^\n]*|#(?:if|endif)[^\n]*', '', source)
    lines = []
    indent = 0
    for token in re.split(r'([{};])', statements):
        token = token.strip()
        if not token or token == ';':
            continue
        if token == '{':
            indent += 1
            continue
        if token == '}':
            indent -= 1
            continue
        token = re.sub(r'^(?:float[234]?|int)\s+', '', token)
        token = token.replace('else if', 'elif')
        if token.startswith(('if (', 'elif (')):
            token += ':'
        elif token == 'else':
            token += ':'
        lines.append('    '*indent + token)
    return compile('\n'.join(lines), str(path), 'exec')


def motion_states(program, species, count, clock, mode, wind=80.0, seed=137,
                  radius_cm=None, spacing_cm=None, height_cm=None, speed_cm_s=None):
    """执行真实位置和Velocity赋值；返回位置、声明速度和局部标量，供导数/作者包络对照。"""
    author = ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraMonolith.py'
    tree = ast.parse(author.read_text(encoding='utf-8'))
    function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'repair_bindings')
    assignment = next(n for n in function.body if isinstance(n, ast.Assign) and
                      any(isinstance(t, ast.Name) and t.id == 'bindings' for t in n.targets))
    binding = ast.literal_eval(assignment.value)['Individual']
    random_source = random.Random(seed)
    radius, height, speed, spacing = {
        'Seagull': (1700.0, 850.0, 250.0, 240.0),
        'Swallow': (1150.0, 570.0, 340.0, 180.0),
        'Eagle': (2400.0, 1400.0, 350.0, 380.0),
    }[species]
    if radius_cm is not None:
        radius = radius_cm
    if spacing_cm is not None:
        spacing = spacing_cm
    if height_cm is not None:
        height = height_cm
    if speed_cm_s is not None:
        speed = speed_cm_s
    source=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    velocity_expression=source.split('Particles.Velocity =',1)[1].split(';',1)[0].strip()
    states = []
    for index in range(count):
        values = dict(Clock=clock, Individual=index if binding == 'Particles.UniqueID' else random_source.random(),
                      Radius=radius, Height=height, Speed=speed, Seed=seed, Mode=mode, Wind=wind,
                      User=SimpleNamespace(BirdCount=count, FlockMinimumSpacing=spacing),
                      Particles=SimpleNamespace(UniqueID=index),
                      frac=lambda x: x-math.floor(x), clamp=lambda x, lo, hi: min(hi, max(lo, x)),
                      float2=ShaderVector, float3=ShaderVector,
                      normalize=lambda v: v*(1.0/math.sqrt(sum(x*x for x in v.values))),
                      sin=math.sin, cos=math.cos, asin=math.asin, floor=math.floor,
                      ceil=math.ceil, fmod=math.fmod, min=min, max=max)
        exec(program, {'__builtins__': {}}, values)
        velocity=eval(velocity_expression,{'__builtins__':{}},values)
        states.append((values['pos'].values,velocity.values,values))
    return states


def motion_positions(program, species, count, clock, mode, wind=80.0, seed=137,
                     radius_cm=None, spacing_cm=None, height_cm=None, speed_cm_s=None):
    """按作者实际Individual绑定执行源码，不把新编号假传给旧随机绑定。"""
    return [state[0] for state in motion_states(program,species,count,clock,mode,wind,seed,
        radius_cm,spacing_cm,height_cm,speed_cm_s)]


class BirdFlockMotionSpacingTests(unittest.TestCase):
    """回归原缺陷：随机相位或独立角速度会让同群个体在后续飞行追近，不能只测出生点。"""
    @classmethod
    def setUpClass(cls):
        cls.program = compile_motion_position()

    def assert_minimum_spacing(self, species, count, mode, times, spacing, wind=80.0,
                               radius_cm=None, spacing_cm=None):
        for clock in times:
            positions = motion_positions(self.program, species, count, clock, mode, wind,
                                         radius_cm=radius_cm, spacing_cm=spacing_cm)
            for first, position in enumerate(positions):
                for second in range(first):
                    distance = math.dist(position, positions[second])
                    self.assertGreaterEqual(distance+0.05, spacing,
                        f'{species} mode={mode} count={count} t={clock:.4f}s pair={second}/{first} '
                        f'distance={distance:.3f}cm required={spacing}cm')

    def test_village_and_openworld_spacing_through_complete_route_phases(self):
        # 最小间距包含最大1.11模型缩放、两侧振翅WPO和可见余量；数值来自实际mesh包围球/WPO预检后保存的240/180/380厘米。
        for species, count, mode, spacing in (
                ('Seagull', 32, 1, 240.0), ('Swallow', 24, 2, 180.0), ('Eagle', 5, 0, 380.0)):
            with self.subTest(species=species):
                self.assert_minimum_spacing(species, count, mode,
                    [step*0.5 for step in range(1201)] + [3600.0, 86400.0], spacing)

    def test_density_changes_and_maximum_wind_preserve_spacing(self):
        # 八字交汇、数量增加、极限风扰动不能消除稳定高度层或重新引入独立相位漂移。
        for species, spacing in (('Seagull', 240.0), ('Swallow', 180.0), ('Eagle', 380.0)):
            for count in (2, 3, 72, 128, 512):
                for mode in (0, 1, 2):
                    with self.subTest(species=species, count=count, mode=mode):
                        self.assert_minimum_spacing(species, count, mode,
                            (0.0, 17.125, 183.75, 86400.0), spacing, wind=2000.0)

    def test_minimum_radius_does_not_force_two_birds_into_an_insufficient_lane(self):
        # 当中心距大于单层直径，asin夹到1不能误算容量为2；应给每只鸟独立固定高度层。
        for species, spacing in (('Seagull',240.0),('Swallow',180.0),('Eagle',380.0)):
            for count in (2,3):
                for mode in (0,1,2):
                    for requested in (spacing,1000.0):
                        with self.subTest(species=species,count=count,mode=mode,spacing=requested):
                            self.assert_minimum_spacing(species,count,mode,
                                [step*0.5 for step in range(91)],requested,
                                radius_cm=200.0,spacing_cm=requested)


class BirdFlockNaturalRouteTests(unittest.TestCase):
    """自然路线回归检查真实公式，避免把换为另一椭圆误当已解决绕圆问题。"""
    @classmethod
    def setUpClass(cls):
        cls.program=compile_motion_position()

    def test_every_mode_has_both_left_and_right_bends_instead_of_a_circle_or_ellipse(self):
        for mode in (0,1,2):
            points=[motion_positions(self.program,'Seagull',1,t*0.5,mode,wind=0)[0] for t in range(401)]
            turns=[]
            for before,current,after in zip(points,points[1:],points[2:]):
                first=(current[0]-before[0],current[1]-before[1])
                second=(after[0]-current[0],after[1]-current[1])
                turns.append(first[0]*second[1]-first[1]*second[0])
            with self.subTest(mode=mode):
                self.assertLess(min(turns),-1.0,'缺少向右弯曲，仍在单方向绕圈')
                self.assertGreater(max(turns),1.0,'缺少向左弯曲，仍在单方向绕圈')

    def test_declared_velocity_matches_position_derivative_with_wind_and_vertical_motion(self):
        # 中心差分独立验证解析速度；旧normalize(tangent)*Speed会漏掉椭圆倍率、风与高度导数。
        epsilon=0.0001
        for mode in (0,1,2):
            for seconds in (0.5,13.75,48.0,183.75,600.0):
                before=motion_states(self.program,'Seagull',3,seconds-epsilon,mode)
                current=motion_states(self.program,'Seagull',3,seconds,mode)
                after=motion_states(self.program,'Seagull',3,seconds+epsilon,mode)
                for first,state,last in zip(before,current,after):
                    derivative=tuple((b-a)/(2*epsilon) for a,b in zip(first[0],last[0]))
                    self.assertLess(math.dist(derivative,state[1]),0.002,
                        f'Mode={mode}, t={seconds}秒的Velocity不是位置导数')

    def test_route_position_and_velocity_remain_continuous_across_many_turns(self):
        for mode in (0,1,2):
            for seconds in (0.001,8.0,16.0,32.0,64.0,3600.0,86400.0):
                before=motion_states(self.program,'Eagle',5,seconds-0.0001,mode)
                after=motion_states(self.program,'Eagle',5,seconds+0.0001,mode)
                for first,last in zip(before,after):
                    self.assertLess(math.dist(first[0],last[0]),0.25,'位置发生跳变')
                    self.assertLess(math.dist(first[1],last[1]),0.25,'速度发生跳变')

    def envelope_functions(self):
        """只加载作者的纯包络函数，不触发Monolith；真实网格尺寸由作者每次另行原生读取。"""
        path=ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraMonolith.py'
        tree=ast.parse(path.read_text(encoding='utf-8'))
        functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and
                   n.name in ('route_profile','route_envelope')]
        self.assertEqual(len(functions),2,'自然路线缺少XYZ作者审计')
        scope={'math':math}
        exec(compile(ast.Module(body=functions,type_ignores=[]),str(path),'exec'),scope)
        return scope['route_profile'],scope['route_envelope']

    def test_author_profiles_match_hlsl_and_have_proven_distance_lower_bounds(self):
        profile,_=self.envelope_functions()
        for mode in (0,1,2):
            expected=profile(mode)
            values=motion_states(self.program,'Eagle',1,0.0,mode)[0][2]
            self.assertEqual(expected,tuple(values[name] for name in
                ('routeWidth','routeDepth','routeDistanceBound','bobAmplitude')))
            width,depth,distance,_=expected
            proved=math.sqrt(width*width/2.0-width**4/(64.0*depth*depth))
            self.assertLessEqual(distance,proved,'作者容量采用了无法证明的曲线距离下界')

    def test_defaults_and_maximum_radius_height_count_fit_full_xyz_mesh_wpo_bounds(self):
        _,envelope=self.envelope_functions()
        # 真实已保存网格预检的固定回归夹具：球半径覆盖任意朝向，1.11缩放、振翅和7.5cm单边余量。
        species=[('Seagull',32,1700.0,850.0,240.0,86.72951945735184,11.0),
                 ('Swallow',24,1150.0,570.0,180.0,59.505651419104,7.0),
                 ('Eagle',5,2400.0,1400.0,380.0,140.61910809565487,20.0)]
        for name,formal,radius,height,spacing,sphere,wing in species:
            margin=sphere*1.11+wing+7.5
            for mode in (0,1,2):
                for count in sorted({formal,72,128,512}):
                    for requested_radius,requested_height in ((radius,height),(10000.0,10000.0)):
                        bounds=envelope(requested_radius,requested_height,count,spacing,mode,2000.0,margin)
                        self.assertTrue(bounds['withinFixedBounds'],f'{name} Mode{mode} Count{count}: {bounds}')
                        for seconds in (0.0,183.75,86400.0):
                            positions=motion_positions(self.program,name,count,seconds,mode,wind=2000.0,
                                radius_cm=requested_radius,height_cm=requested_height)
                            for position in positions:
                                for axis in range(3):
                                    self.assertGreaterEqual(position[axis]-margin+0.001,bounds['meshMinCm'][axis])
                                    self.assertLessEqual(position[axis]+margin-0.001,bounds['meshMaxCm'][axis])

    def test_small_radius_high_density_is_rejected_instead_of_enlarging_bounds(self):
        _,envelope=self.envelope_functions()
        for mode in (0,1,2):
            bounds=envelope(200.0,1400.0,512,380.0,mode,2000.0,183.5872099861769)
            self.assertFalse(bounds['withinFixedBounds'],'小半径512只不应假称能装进原Z边界')
            self.assertGreater(bounds['meshMaxCm'][2],12000.0)

    def test_small_radius_two_or_three_birds_remain_supported(self):
        _,envelope=self.envelope_functions()
        for mode in (0,1,2):
            for count in (2,3):
                self.assertTrue(envelope(200.0,200.0,count,380.0,mode,2000.0,183.58721)['withinFixedBounds'])

    def test_field_maxima_are_not_mistaken_for_all_combinations_being_supported(self):
        _,envelope=self.envelope_functions()
        self.assertFalse(envelope(10000.0,10000.0,512,1000.0,2,2000.0,183.58721)['withinFixedBounds'])

    def test_integer_schema_rejects_same_name_float_shadow_parameters(self):
        path=ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraMonolith.py'
        tree=ast.parse(path.read_text(encoding='utf-8'))
        nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='validate_user_parameter_types']
        self.assertEqual(len(nodes),1)
        scope={}
        exec(compile(ast.Module(body=nodes,type_ignores=[]),str(path),'exec'),scope)
        parameters=[{'name':name,'type':kind} for name,kind in {
            'BirdCount':'NiagaraInt32','FlightMode':'NiagaraInt32','FlockSeed':'NiagaraInt32',
            'FlockRadius':'NiagaraFloat','FlockHeight':'NiagaraFloat','FlightSpeed':'NiagaraFloat',
            'WindInfluence':'NiagaraFloat','FlockMinimumSpacing':'NiagaraFloat'}.items()]
        self.assertEqual(len(scope['validate_user_parameter_types'](parameters)['schemaTypes']),8)
        for name in ('FlightMode','FlockSeed'):
            shadowed=parameters+[{'name':name,'type':'NiagaraFloat'}]
            with self.assertRaises(AssertionError):
                scope['validate_user_parameter_types'](shadowed)
            audit=scope['validate_user_parameter_types'](shadowed,allow_legacy_float_shadows=True)
            self.assertEqual(audit['schemaTypes'][name],'NiagaraInt32')
            self.assertEqual(audit['legacyFloatShadows'],[{'name':name,'type':'NiagaraFloat'}])


class BirdFlockGPUReadbackPolicyTests(unittest.TestCase):
    """独立坐标夹具只验证实际GPU报告判定器，不能冒充运行过Niagara或取得真实GPU坐标。"""
    def load_function(self,name):
        path=ROOT/'Tools/Unreal/VFX/BirdFlock/RunBirdFlockGPUReview.py'
        tree=ast.parse(path.read_text(encoding='utf-8'))
        functions=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name==name]
        self.assertEqual(len(functions),1,'GPU审核缺少真实采样判定函数：'+name)
        namespace={'math':math}
        exec(compile(ast.Module(body=functions,type_ignores=[]),str(path),'exec'),namespace)
        return namespace[name]

    def test_all_particle_pairs_are_checked_instead_of_only_first_particle(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        result=assess([(0,0,0),(300,0,0),(301,0,0)],3,2.0)
        self.assertFalse(result['pass'])
        self.assertEqual(result['minimumCenterDistanceCm'],1.0)
        self.assertEqual(result['closestParticleIndices'],[1,2])

    def test_declared_spacing_has_only_explicit_float_tolerance(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        self.assertTrue(assess([(0,0,0),(99.97,0,0)],2,100.0)['pass'])
        self.assertFalse(assess([(0,0,0),(99.94,0,0)],2,100.0)['pass'])

    def test_duplicate_position_cannot_pass_when_other_positions_are_unique(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        self.assertFalse(assess([(0,0,0),(100,0,0),(100,0,0)],3,20.0)['pass'])

    def test_fixed_vertical_lane_spacing_is_measured_in_three_dimensions(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        result=assess([(0,0,0),(0,0,140)],2,140.0)
        self.assertTrue(result['pass'])
        self.assertEqual(result['minimumCenterDistanceCm'],140.0)

    def test_wrong_count_and_nonfinite_gpu_position_fail(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        self.assertFalse(assess([(0,0,0)],2,140.0)['pass'])
        self.assertFalse(assess([(0,0,0),(math.nan,0,0)],2,140.0)['pass'])

    def test_zero_or_one_particle_does_not_invent_a_pair_distance(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        for points,count in (([],0),([(0,0,0)],1)):
            result=assess(points,count,220.0)
            self.assertTrue(result['pass'])
            self.assertIsNone(result['minimumCenterDistanceCm'])

    def test_invalid_declared_spacing_is_rejected(self):
        assess=self.load_function('assess_gpu_spacing_sample')
        for spacing in (0.0,-1.0,math.nan,math.inf):
            with self.assertRaises(ValueError):
                assess([(0,0,0)],1,spacing)

    def test_spacing_mode_uses_formal_world_counts_and_density_boundaries(self):
        cases=self.load_function('review_cases')
        self.assertEqual(cases(True),[
            ('Seagull',32),('Seagull',128),('Seagull',512),
            ('Swallow',24),('Swallow',128),('Swallow',512),
            ('Eagle',5),('Eagle',128),('Eagle',512)])
        legacy=cases(False)
        self.assertEqual(len(legacy),15)
        self.assertIn(('Swallow',0),legacy)
        self.assertIn(('Eagle',1),legacy)


class BirdFlockMotionGraphContractTests(unittest.TestCase):
    """纯图夹具只测试接线门禁，真实InputMap标志、保存和冷重载必须另外在引擎取证。"""
    def validator(self):
        path=ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraMonolith.py'
        tree=ast.parse(path.read_text(encoding='utf-8'))
        functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='validate_motion_graph']
        self.assertEqual(len(functions),1,'作者缺少可独立审核的运动模块接线门禁')
        scope={}
        exec(compile(ast.Module(body=functions,type_ignores=[]),str(path),'exec'),scope)
        return scope['validate_motion_graph']

    def graph(self):
        bindings={'Clock':'System.Age','Individual':'Particles.MaterialRandom','Radius':'User.FlockRadius',
                  'Height':'User.FlockHeight','Speed':'User.FlightSpeed','Seed':'User.FlockSeed',
                  'Mode':'User.FlightMode','Wind':'User.WindInfluence'}
        nodes=[{'node_guid':'root','class':'NiagaraNodeInput'},
               {'node_guid':'set','class':'NiagaraNodeParameterMapSet'},
               {'node_guid':'fn','function_name':'NM_GP_BirdFlockMotion','pins':[
                   {'name':'InputMap','direction':'Input','linked_count':1},
                   {'name':'Output','direction':'Output','linked_count':1}]},
               {'node_guid':'out','class':'NiagaraNodeOutput'}]
        edges=[{'from_node_guid':'root','from_pin_name':'Input','to_node_guid':'set','to_pin_name':'Source'},
               {'from_node_guid':'set','from_pin_name':'Dest','to_node_guid':'fn','to_pin_name':'InputMap'},
               {'from_node_guid':'fn','from_pin_name':'Output','to_node_guid':'out','to_pin_name':'Out'}]
        for name,binding in bindings.items():
            nodes.append({'node_guid':name,'class':'NiagaraNodeParameterMapGet'})
            edges.append({'from_node_guid':'root','from_pin_name':'Input','to_node_guid':name,'to_pin_name':'Source'})
            if name in ('Mode','Seed'):
                converter=name+'Conversion'
                nodes.extend([{'node_guid':converter,'class':'NiagaraNodeFunctionCall',
                    'function_name':converter,'function_script':'/Niagara/DynamicInputs/TypeConversions/MakeFloatFromInt.MakeFloatFromInt'},
                    {'node_guid':converter+'Set','class':'NiagaraNodeParameterMapSet'}])
                edges.extend([
                    {'from_node_guid':'root','from_pin_name':'Input','to_node_guid':converter+'Set','to_pin_name':'Source'},
                    {'from_node_guid':name,'from_pin_name':binding,'to_node_guid':converter+'Set','to_pin_name':converter+'.Integer'},
                    {'from_node_guid':converter+'Set','from_pin_name':'Dest','to_node_guid':converter,'to_pin_name':'InputMap'},
                    {'from_node_guid':converter,'from_pin_name':'Output','to_node_guid':'set','to_pin_name':'NM_GP_BirdFlockMotion.'+name}])
            else:
                edges.append({'from_node_guid':name,'from_pin_name':binding,'to_node_guid':'set',
                    'to_pin_name':'NM_GP_BirdFlockMotion.'+name})
        return {'nodes':nodes,'edges':edges}

    def test_complete_upstream_bindings_pass(self):
        self.assertEqual(self.validator()(self.graph(),'bOrphanedPin=False')['bindingCount'],8)

    def test_serialized_orphan_is_rejected_even_with_a_link(self):
        with self.assertRaises(AssertionError):
            self.validator()(self.graph(),'bOrphanedPin=True')

    def test_output_feedback_is_rejected(self):
        graph=self.graph()
        graph['edges'][0]['from_node_guid']='fn'
        with self.assertRaises(AssertionError):
            self.validator()(graph,'bOrphanedPin=False')

    def test_binding_reader_cannot_read_from_its_own_call_output(self):
        graph=self.graph()
        graph['edges'][3]['from_node_guid']='fn'
        with self.assertRaises(AssertionError):
            self.validator()(graph,'bOrphanedPin=False')

    def test_mode_cannot_silently_read_a_same_name_float_instead_of_the_integer_schema(self):
        graph=self.graph()
        link=next(e for e in graph['edges'] if e['to_pin_name']=='NM_GP_BirdFlockMotion.Mode')
        link['from_node_guid']='Mode'
        link['from_pin_name']='User.FlightMode'
        with self.assertRaises(AssertionError):
            self.validator()(graph,'bOrphanedPin=False')


def required_files():
    """独立列出规格19项，避免实现常量出错时测试同步出错。"""
    names = [GP+'/Materials/M_GP_BirdFlock_Master.uasset', GP+'/Niagara/NS_GP_BirdFlock_Base.uasset',
             GP+'/EffectTypes/NET_GP_BirdFlock_Ambient.uasset', DBA+'/Blueprints/BP_DBA_BirdFlockAmbient.uasset']
    for species in ('Seagull', 'Swallow', 'Eagle'):
        names.extend([DBA+'/Meshes/Flight/SM_DBA_Bird_'+species+'.uasset',
                      DBA+'/Textures/T_DBA_Bird_'+species+'_Albedo.uasset',
                      DBA+'/Materials/MI_DBA_Bird_'+species+'.uasset',
                      DBA+'/Niagara/NS_DBA_BirdFlock_'+species+'.uasset',
                      DBA+'/Definitions/DA_DBA_VFX_BirdFlock_'+species+'.uasset'])
    return names


class BirdFlockReleasePolicyTests(unittest.TestCase):
    def test_archived_history_mesh_and_sidecar_are_rejected(self):
        root=DBA.split('/VFX/',1)[0]+'/Development/History/BirdFlock/Meshes'
        for suffix in ('.uasset','.ubulk','.uexp'):
            item=root+'/SM_DBA_Bird_Seagull_Cm'+suffix
            report=self.report(required_files()+[item])
            self.assertEqual(report['status'],'failed')
            self.assertIn(item,report['forbidden'])

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='bird-release-')
        self.addCleanup(self.tmp.cleanup)
        self.csv = Path(self.tmp.name)/'Container.csv'

    def report(self, files, target='Client', platform='WindowsClient'):
        with self.csv.open('w', encoding='utf-8-sig', newline='') as stream:
            writer = csv.writer(stream)
            writer.writerow(('Filename', 'Platform'))
            writer.writerows((name, platform) for name in files)
        return VALIDATOR.validate(self.csv, target)

    def test_clean_client_requires_all_19_assets(self):
        result = self.report(required_files())
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['requiredCount'], 19)

    def test_missing_species_asset_is_rejected(self):
        self.assertEqual(self.report(required_files()[:-1])['status'], 'failed')

    def test_legacy_mesh_main_and_sidecar_blocks_are_rejected(self):
        for suffix in ('.uasset', '.ubulk', '.uexp', '_Cm.uasset', '_Flight.uasset'):
            with self.subTest(suffix=suffix):
                result = self.report(required_files()+[DBA+'/Meshes/SM_DBA_Bird_Seagull'+suffix])
                self.assertEqual(result['status'], 'failed')
                self.assertTrue(result['forbidden'])

    def test_legacy_mixed_case_windows_path_is_rejected(self):
        old = (DBA+'/Meshes/SM_DBA_Bird_Eagle_Cm.uasset').upper().replace('/', '\\')
        self.assertEqual(self.report(required_files()+[old])['status'], 'failed')

    def test_case_insensitive_formal_paths_are_recognized(self):
        names = [p.upper().replace('/', '\\') for p in required_files()]
        self.assertEqual(self.report(names)['status'], 'passed')

    def test_source_art_is_rejected(self):
        self.assertEqual(self.report(required_files()+[DBA+'/SourceArt/model.blend'])['status'], 'failed')

    def test_development_review_map_is_rejected(self):
        path = '../../../DivineBeastsArena/Content/Development/BirdFlock/Maps/L_DBA_BirdFlockReview.umap'
        self.assertEqual(self.report(required_files()+[path])['status'], 'failed')

    def test_external_actor_review_packages_are_rejected(self):
        # 世界分区外置Actor/对象不直接位于Content/Development，仍属于禁止发布的审核图。
        for folder in ('__ExternalActors__', '__ExternalObjects__'):
            path = '../../../DivineBeastsArena/Content/'+folder+'/Development/BirdFlock/Maps/A/BC/Actor.uasset'
            with self.subTest(folder=folder):
                self.assertEqual(self.report(required_files()+[path])['status'], 'failed')

    def test_server_must_have_no_bird_assets(self):
        self.assertEqual(self.report([required_files()[0]], 'Server', 'WindowsServer')['status'], 'failed')

    def test_clean_server_accepts_unrelated_runtime_files(self):
        path = '../../../DivineBeastsArena/Content/World/ServerMap.umap'
        self.assertEqual(self.report([path], 'Server', 'WindowsServer')['status'], 'passed')

    def test_server_rejects_client_platform_evidence(self):
        with self.assertRaises(ValueError):
            self.report(['../../../DivineBeastsArena/Content/World/Map.umap'], 'Server', 'WindowsClient')

    def test_unsupported_target_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report(required_files(), 'Unknown')

    def test_empty_csv_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report([])

    def test_missing_platform_column_is_rejected(self):
        self.csv.write_text('Filename\n'+required_files()[0]+'\n', encoding='utf-8')
        with self.assertRaises(ValueError):
            VALIDATOR.validate(self.csv, 'Server')

    def test_blank_platform_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report(required_files(), platform='')

    def test_client_configs_include_only_formal_bird_subdirectories(self):
        mount = '/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
        for config in ('FrontEndClient', 'VillageDevelopmentClient'):
            with self.subTest(config=config):
                text = (ROOT/'Game/Config/Custom'/config/'DefaultGame.ini').read_text(encoding='utf-8-sig')
                self.assertNotIn('+DirectoriesToAlwaysCook=(Path="'+mount+'")', text)
                for child in ('Definitions', 'Niagara', 'Materials', 'Textures', 'Meshes/Flight', 'Blueprints'):
                    self.assertIn('+DirectoriesToAlwaysCook=(Path="'+mount+'/'+child+'")', text)

    def test_default_release_excludes_review_map(self):
        text = (ROOT/'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
        self.assertIn('+DirectoriesToNeverCook=(Path="/Game/Development/BirdFlock")', text)

    def test_cook_entry_enforces_bird_gate_and_existing_ui_gate(self):
        text = (ROOT/'Build/Game/CookFrontEndClient.ps1').read_text(encoding='utf-8-sig')
        for gate in ('ValidateBirdFlockContainer.py', 'TestUIColorRHI.ps1', 'TestFrontEndClientContainer.ps1'):
            self.assertIn(gate, text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
