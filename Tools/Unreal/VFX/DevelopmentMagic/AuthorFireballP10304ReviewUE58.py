# -*- coding: utf-8 -*-
"""SSLM项目层火球P1—03/04独立审核素材作者，由Monolith在UE5.8游戏线程调用。

只创建P1Hands内本次新母材质及实例，不修改FireMage、01/02、正式目录或权威玩法。
Niagara结构通过同一Monolith的niagara原生动作制作，脚本不写伪.uasset。
任何目标已存在且不在本次清单中、原稿指纹变化或未保存工作冲突时立即失败。
"""
from pathlib import Path
import unreal as u
import json
import hashlib

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
OUT = ROOT / 'Saved/Validation/FireballV3P10304_20261026'
BASE = '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands'
LIB = '/DBAPresentationPack_Core/VFX/Library/Game/FireMage'
MATS = BASE + '/Materials/'
SOURCES = {
    'charge': LIB + '/Niagara/NS_PY_Charge',
    'texture': LIB + '/Textures/T_PY_FlameFlipbook',
    'flipbook': LIB + '/Materials/M_PY_Flipbook',
    'additive': LIB + '/Materials/M_PY_Additive',
    'core': LIB + '/Materials/Instances/MI_PY_FlipbookCore',
    'ember': LIB + '/Materials/Instances/MI_PY_Ember',
    'heat': '/DBAPresentationPack_Core/VFX/Library/00_Core/Materials/Fire/MI_VFX_Fire_Heat01',
}


def write(name, value):
    """仅写本次Saved证据，不将报告当成原生资产或人工批准。"""
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def protected_sources():
    """验证人工原稿和01/02的实际磁盘字节没有因审核制作发生变化。"""
    rows = json.loads((OUT / 'ProtectedSources.before.json').read_text(encoding='utf-8'))['protectedFiles']
    for row in rows:
        assert hashlib.sha256((ROOT / row['path']).read_bytes()).hexdigest() == row['sha256'], row['path']
    return len(rows)


def preflight():
    """只读查真实引擎/挂载点、材质参数、纹理及待创建目标；保留所有未保存工作。"""
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8')
    dirty = [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    assert not dirty, '存在未保存资产，先保留人工工作：' + str(dirty)
    report = {'project': u.Paths.get_project_file_path(), 'engine': u.SystemLibrary.get_engine_version(),
              'protectedSources': protected_sources(), 'assets': {}, 'manualApprovalSource':
              '用户2026-10-26任务明确确认P1 01/02已通过人工视觉审核；不改写旧技术报告的批准标志。'}
    for key, path in SOURCES.items():
        a = u.load_asset(path)
        assert a, '源资产缺失：' + path
        row = {'path': path, 'class': a.get_class().get_name()}
        if isinstance(a, u.MaterialInterface):
            row['scalars'] = {str(n): u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(a, n)
                              for n in u.MaterialEditingLibrary.get_scalar_parameter_names(a)} if isinstance(a, u.MaterialInstanceConstant) else list(map(str, u.MaterialEditingLibrary.get_scalar_parameter_names(a)))
        if isinstance(a, u.Texture2D):
            row['size'] = [a.blueprint_get_size_x(), a.blueprint_get_size_y()]
        report['assets'][key] = row
    for stage, name in [('01', 'HandsIgnite'), ('02', 'PalmsCharge')]:
        path = BASE + '/Niagara/NS_DBA_Fireball_P1_' + stage + '_' + name + '_Review'
        ns = u.load_asset(path)
        assert isinstance(ns, u.NiagaraSystem), path
        renderers = [r for r in u.ObjectIterator(u.NiagaraSpriteRendererProperties)
                     if r.get_path_name().startswith(ns.get_path_name() + ':')]
        report['assets'][stage] = {'path': path, 'renderers': [{'path': r.get_path_name(),
            'material': r.get_editor_property('material').get_path_name(),
            'subImageSize': str(r.get_editor_property('sub_image_size'))} for r in renderers]}
    write('P0NativeInventory.json', report)
    print(json.dumps({'P0': 'passed', 'assets': len(report['assets']), 'protected': report['protectedSources']}))


def save(asset):
    """同步保存本次拥有的真实资产；失败停止，不自动删除或覆盖源资源。"""
    assert asset and asset.get_path_name().startswith(BASE + '/'), '越出审核目录'
    assert u.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()
    return asset


def new_asset(name, cls, factory):
    """仅创建清单内尚不存在的新审核资产。"""
    path = MATS + name
    ownership = OUT / 'OwnedMaterials.json'
    owned = json.loads(ownership.read_text(encoding='utf-8')) if ownership.exists() else []
    if u.EditorAssetLibrary.does_asset_exist(path):
        assert path in owned, '已有非本次资产，拒绝覆盖：' + path
        a = u.load_asset(path)
        assert isinstance(a, cls), path
        return a
    a = u.AssetToolsHelpers.get_asset_tools().create_asset(name, MATS.rstrip('/'), cls, factory)
    assert a, path
    owned.append(path)
    write('OwnedMaterials.json', owned)
    return a


def mi(name, parent, scalars):
    """保留原母材质的Energy/Density接口，仅用实例表达阶段差异。"""
    a = new_asset(name, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    u.MaterialEditingLibrary.set_material_instance_parent(a, parent)
    available = set(map(str, u.MaterialEditingLibrary.get_scalar_parameter_names(a)))
    for key, value in scalars.items():
        assert key in available, '材质缺少真实参数：' + key
        # UE接口可能在值未变时返回False；以真实回读为准，不能把返回值当作写入结果。
        u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(a, key, float(value))
        assert abs(u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(a, key) - value) < 1.e-4
    return save(a)


def scalar(m, name, default, x, y):
    node = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', float(default))
    return node


def custom(m, code, inputs, x=0, y=0, output=None):
    """建立原生Material Custom节点，逐输入连接；所有引用保留为真实UObject。"""
    node = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output or u.CustomMaterialOutputType.CMOT_FLOAT4)
    params = []
    for key in inputs:
        pin = u.CustomInput()
        pin.set_editor_property('input_name', key)
        params.append(pin)
    node.set_editor_property('inputs', params)
    for key, value in inputs.items():
        expr, output_name = value if isinstance(value, tuple) else (value, '')
        assert u.MaterialEditingLibrary.connect_material_expressions(expr, output_name, node, key), key
    return node


def animated_material(name, sphere):
    """原稿Additive/ParticleSubUV无法承担厚球壳及Ribbon，故只在审核目录创建专用图。
    沿用同一火焰图集；2次采样插帧，低频卷动与软边，避免高频砂砾和多层面片伪体积。
    球壳为单层AlphaComposite Mesh，核心另一个小Mesh；不是流体模拟或全屏折射。
    """
    m = new_asset(name, u.Material, u.MaterialFactoryNew())
    # 只重建本次登记的审核母材质，允许修复本次编译错误，禁止触碰任何源母材质。
    u.MaterialEditingLibrary.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', u.BlendMode.BLEND_ALPHA_COMPOSITE if sphere else u.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', not sphere)
    m.set_editor_property('used_with_niagara_mesh_particles', True)
    if not sphere:
        m.set_editor_property('used_with_niagara_ribbons', True)
    pc = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionParticleColor, -900, 0)
    uv = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionTextureCoordinate, -900, 160)
    tm = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionTime, -900, 320)
    tex = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionTextureObjectParameter, -900, 480)
    tex.set_editor_property('parameter_name', 'FlameAtlas')
    tex.set_editor_property('texture', u.load_asset(SOURCES['texture']))
    energy = scalar(m, 'Energy', 2.2, -900, 640)
    density = scalar(m, 'Density', .85, -900, 720)
    core = scalar(m, 'CoreMode', 0, -900, 800)
    # 统一材质读取ParticleColor；Niagara中的强度、质量、阶段进度拥有可见性和生命周期。
    code = r'''
float2 v=UV;
float frame=frac(T*.57)*16.0;
float i=floor(frame),j=fmod(i+1.0,16.0);
float2 local=saturate(v*.96+.02);
float2 a=(local+float2(fmod(i,4.0),floor(i/4.0)))/4.0;
float2 b=(local+float2(fmod(j,4.0),floor(j/4.0)))/4.0;
float4 f=lerp(Texture2DSample(Atlas,AtlasSampler,a),Texture2DSample(Atlas,AtlasSampler,b),frac(frame));
float lum=dot(f.rgb,float3(.25,.55,.20));
float n=.5+.22*sin(v.x*18.85+T*.63+2.0*sin(v.y*12.56-T*.5))+.13*sin(v.y*25.13-T*.85);
float body=saturate(.43+.52*n+.22*lum);
float edge=pow(saturate(1.0-abs(v.y*2.0-1.0)),.72);
float isSphere=SPHERE;
float opacity=saturate(Density*PCAlpha*lerp(edge*(.35+.65*lum),.82+.16*n,isSphere));
float heat=saturate(.23+.53*n+.18*lum);
float3 flame=lerp(float3(1.0,.075,.009),float3(1.0,.48,.055),smoothstep(.23,.79,heat));
flame=lerp(flame,float3(1.0,.88,.51),pow(saturate((heat-.78)*4.5),2.0)*.35);
flame=lerp(flame,float3(1.0,.84,.46),CoreMode);
float3 rgb=flame*Energy*body*PC.rgb;
return float4(rgb*lerp(1.0,opacity,isSphere),opacity);
'''.replace('SPHERE', '1.0' if sphere else '0.0')
    node = custom(m, code, {'UV': uv, 'T': tm, 'Atlas': tex, 'Energy': energy,
                           'Density': density, 'CoreMode': core, 'PC': pc, 'PCAlpha': (pc, 'A')})
    rgb = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionComponentMask, 330, 0)
    for key, value in [('r', True), ('g', True), ('b', True), ('a', False)]: rgb.set_editor_property(key, value)
    alpha = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionComponentMask, 330, 160)
    for key, value in [('r', False), ('g', False), ('b', False), ('a', True)]: alpha.set_editor_property(key, value)
    u.MaterialEditingLibrary.connect_material_expressions(node, '', rgb, '')
    u.MaterialEditingLibrary.connect_material_expressions(node, '', alpha, '')
    u.MaterialEditingLibrary.connect_material_property(rgb, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY)
    if sphere:
        normal = u.MaterialEditingLibrary.create_material_expression(m, u.MaterialExpressionVertexNormalWS, -900, 920)
        wpo = custom(m, 'return N*(.65*sin(UV.x*18.85+T*.7)+.7*sin(UV.y*25.13-T*.9))*(1.0-CoreMode);',
                     {'N': normal, 'UV': uv, 'T': tm, 'CoreMode': core}, 100, 440, u.CustomMaterialOutputType.CMOT_FLOAT3)
        u.MaterialEditingLibrary.connect_material_property(wpo, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    u.MaterialEditingLibrary.recompile_material(m)
    return save(m)


def materials():
    """P2创建9份专用资产；仅可恢复本次登记资源，禁止覆盖01/02或原FireMage材质。"""
    protected_sources()
    created = []
    stream = animated_material('M_DBA_Fireball_P1_Stream_Review', False); created.append(stream)
    shell = animated_material('M_DBA_Fireball_P1_Shell_Review', True); created.append(shell)
    created.append(mi('MI_DBA_Fireball_P1_03_Stream_Review', stream, {'Energy': 2.4, 'Density': .84}))
    created.append(mi('MI_DBA_Fireball_P1_03_Core_Review', shell, {'Energy': 3.5, 'Density': .96, 'CoreMode': 1}))
    created.append(mi('MI_DBA_Fireball_P1_04_Core_Review', shell, {'Energy': 4.0, 'Density': .96, 'CoreMode': 1}))
    created.append(mi('MI_DBA_Fireball_P1_04_Shell_Review', shell, {'Energy': 3.3, 'Density': .91, 'CoreMode': 0}))
    created.append(mi('MI_DBA_Fireball_P1_04_Licks_Review', u.load_asset(SOURCES['flipbook']), {'Energy': 2.15, 'Density': .70}))
    created.append(mi('MI_DBA_Fireball_P1_03_Residue_Review', u.load_asset(SOURCES['flipbook']), {'Energy': 1.9, 'Density': .70}))
    heat = u.load_asset(SOURCES['heat'])
    created.append(mi('MI_DBA_Fireball_P1_AirRipple_Review', heat, {'P_Intensity': .12, 'P_Opacity': .08}))
    protected_sources()
    write('P2Materials.json', {'assets': [a.get_path_name() for a in created], 'saved': True,
          'originalsUnchanged': True, 'reuseLimit': '原材质为Additive+ParticleSubUV，不能直接提供厚球壳或稳定Ribbon UV；专用材质仅存在审核目录。',
          'manualAccepted': False})
    print(json.dumps({'materialsSaved': len(created), 'protectedUnchanged': True}))


def bind_motion():
    """保留原NS_PY_Charge已经验证的参数图拓扑，仅替换本次副本的兼容运动调用。
    Monolith删除连续模块会破坏此模板的参数图连接，故不删除上游模块、不重写源图。
    创建器默认隐藏InputMap会导致模块输出不可达；只修复本次模块的输入公开合同。
    """
    module = u.load_asset(BASE + '/Niagara/NM_DBA_Fireball_P1_0304_Motion_Review')
    assert isinstance(module, u.NiagaraScript)
    inputs = [n for n in u.ObjectIterator(u.NiagaraNodeInput)
              if n.get_path_name().startswith(module.get_path_name() + ':')]
    assert len(inputs) == 1
    opts = inputs[0].get_editor_property('exposure_options')
    for key, value in [('exposed', True), ('required', True), ('hidden', False)]:
        opts.set_editor_property(key, value)
    inputs[0].set_editor_property('exposure_options', opts)
    save(module)
    source = '/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5'
    result = []
    for path in json.loads((OUT / 'OwnedSystems.json').read_text(encoding='utf-8')):
        system = u.load_asset(path)
        calls = [n for n in u.ObjectIterator(u.NiagaraNodeFunctionCall)
                 if n.get_path_name().startswith(system.get_path_name() + ':')]
        rebound = []
        for call in calls:
            script = call.get_editor_property('function_script')
            if script and script.get_path_name().split('.')[0] == source:
                call.set_editor_property('function_script', module)
                assert call.get_editor_property('function_script') == module
                rebound.append(call.get_path_name())
        assert len(rebound) == 4, (path, len(rebound))
        result.append({'system': path, 'calls': rebound})
    protected_sources()
    write('MotionBindings.json', result)
    print(json.dumps({'systems': len(result), 'reboundCalls': sum(len(r['calls']) for r in result)}))


def bind_material_time():
    """审核材质读取同一粒子的模拟时间，避免录帧时材质时间与Niagara时间分叉。"""
    result = []
    for name in ('M_DBA_Fireball_P1_Stream_Review', 'M_DBA_Fireball_P1_Shell_Review'):
        material = u.load_asset(MATS + name)
        assert material
        nodes = [x for x in u.ObjectIterator(u.MaterialExpressionCustom)
                 if x.get_path_name().startswith(material.get_path_name() + ':')]
        dynamic = u.MaterialEditingLibrary.create_material_expression(material, u.MaterialExpressionDynamicParameter, -660, 340)
        for node in nodes:
            assert u.MaterialEditingLibrary.connect_material_expressions(dynamic, '', node, 'T')
        u.MaterialEditingLibrary.recompile_material(material)
        save(material)
        result.append(material.get_path_name())
    write('MaterialSimulationTime.json', {'materials': result, 'timeSource': 'Particles.DynamicMaterialParameter.x'})
    print('Materials use native particle simulation time', len(result))
