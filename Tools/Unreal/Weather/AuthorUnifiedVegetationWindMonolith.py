"""SSLM第三层植物风作者；只由直接Monolith在锁定Editor内执行。

inspect只读当前正式World并备份将修改包；后续作者函数在审核后逐步实现。
不重置散布、Actor位置、碰撞或原图；不保存任何无关脏包。
"""
import hashlib
import json
import pathlib
import shutil
import sys
import os
import unreal as u

ROOT = pathlib.Path(u.Paths.project_dir()).resolve().parent
EVIDENCE = ROOT / 'Saved/Validation/UnifiedVegetationWind20261024/Native'
FORMAL = '/DBAWorldPack_Village/Maps/L_Village_Start'
MPC = '/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal'
LIB = u.MaterialEditingLibrary
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import UnifiedVegetationWindContract as contract
OWNED = set()


def require_idle():
    """所有作者入口拒绝PIE和未保存地图；保护其他对话及手工编辑，不能隐式结束PIE。"""
    if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('PIE正在运行，拒绝资产修改')
    if u.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('存在未保存地图，先保护并保存人工现场')
    if pathlib.Path(u.Paths.project_dir()).resolve() != ROOT / 'Game':
        raise RuntimeError('错误编辑器工程')


def expressions(asset):
    return list(u.GamePlatformSurfaceMaterialAuthoringLibrary.get_material_expressions(asset))


def node(asset, cls):
    create = LIB.create_material_expression_in_function if isinstance(asset, u.MaterialFunction) else LIB.create_material_expression
    result = create(asset, cls, 0, 0)
    if not result:
        raise RuntimeError('节点创建失败: ' + str(cls))
    return result


def wire(source, target, pin='', output=''):
    if not LIB.connect_material_expressions(source, output, target, pin):
        raise RuntimeError('原生连线失败: ' + target.get_name() + '.' + pin)


def custom(asset, code, inputs, output_type):
    result = node(asset, u.MaterialExpressionCustom)
    result.set_editor_property('code', code)
    result.set_editor_property('output_type', output_type)
    rows = []
    for name in inputs:
        row = u.CustomInput(); row.set_editor_property('input_name', name); rows.append(row)
    result.set_editor_property('inputs', rows)
    for name, (source, output) in inputs.items():
        wire(source, result, name, output)
    return result


def scalar(asset, name, value):
    result = node(asset, u.MaterialExpressionScalarParameter)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result


def function_output(asset, name, source, output='', order=0):
    result = node(asset, u.MaterialExpressionFunctionOutput)
    result.set_editor_property('output_name', name)
    result.set_editor_property('sort_priority', order)
    wire(source, result, '', output)
    return result


def function_input(asset, name, vector=False, default=0.):
    result = node(asset, u.MaterialExpressionFunctionInput)
    result.set_editor_property('input_name', name)
    result.set_editor_property('input_type', u.FunctionInputType.FUNCTION_INPUT_VECTOR3 if vector else u.FunctionInputType.FUNCTION_INPUT_SCALAR)
    result.set_editor_property('preview_value', u.Vector4(default, 0, 0, 0))
    result.set_editor_property('use_preview_value_as_default', True)
    return result


def call(asset, function):
    result = node(asset, u.MaterialExpressionMaterialFunctionCall)
    result.set_editor_property('material_function', function)
    return result


def acquire(path, source=None, material=False):
    """新资产分阶段制作；已有非本工具资产/中断资产明确拒绝，不能静默覆盖。"""
    existing = u.load_asset(path)
    if existing:
        version = u.EditorAssetLibrary.get_metadata_tag(existing, 'UnifiedWindVersion')
        state = u.EditorAssetLibrary.get_metadata_tag(existing, 'UnifiedWindState')
        if version != contract.VERSION or state != 'GraphAuthored':
            raise RuntimeError('目标已存在且未通过本轮完整制作: ' + path)
        OWNED.add(existing.get_path_name())
        return existing, False
    if source:
        result = u.EditorAssetLibrary.duplicate_asset(source, path)
    else:
        cls, factory = (u.Material, u.MaterialFactoryNew()) if material else (u.MaterialFunction, u.MaterialFunctionFactoryNew())
        result = u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], cls, factory)
    if not result:
        raise RuntimeError('资产创建失败: ' + path)
    u.EditorAssetLibrary.set_metadata_tag(result, 'UnifiedWindVersion', contract.VERSION)
    u.EditorAssetLibrary.set_metadata_tag(result, 'UnifiedWindState', 'Building')
    OWNED.add(result.get_path_name())
    return result, True


def complete(asset):
    if isinstance(asset, u.Material):
        errors = list(LIB.recompile_material(asset))
        if errors:
            write('CompileFailure.json', {'asset': asset.get_path_name(), 'errors': errors})
            raise RuntimeError('原生材质编译失败: ' + str(errors))
    elif isinstance(asset, u.MaterialFunction):
        LIB.update_material_function(asset)
    # 仅表示连线已创建并已请求编译；真实Shader、保存/重载与GPU证据由独立审计门禁提供。
    u.EditorAssetLibrary.set_metadata_tag(asset, 'UnifiedWindState', 'GraphAuthored')
    OWNED.add(asset.get_path_name())


def write(name, value):
    """写本任务证据，不写资产二进制；中文JSON保持UTF-8。"""
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def package_file(asset_path):
    """将真实挂载包解析到唯一插件Content，供只读指纹和可回退备份。"""
    package = asset_path.split('.')[0]
    mount, relative = package.strip('/').split('/', 1)
    if mount == 'Game':
        base = ROOT / 'Game/Content'
    else:
        matches = list((ROOT / 'Game/Plugins').rglob(mount + '.uplugin'))
        if len(matches) != 1:
            raise RuntimeError('无法唯一解析包: ' + package)
        base = matches[0].parent / 'Content'
    path = base / (relative + '.uasset')
    return path if path.exists() else base / (relative + '.umap')


def backup(asset_path):
    """首次修改前备份已保存包；幂等保留首份，不覆盖本轮回退点。"""
    source = package_file(asset_path)
    if not source.is_file():
        raise RuntimeError('缺少已保存包: ' + str(source))
    target = EVIDENCE / 'Before' / source.relative_to(ROOT)
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    current_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if hashlib.sha256(target.read_bytes()).hexdigest() != current_hash:
        # 后续人工编辑产生了更新版本时另留当前回退点，不覆盖首份或用首份回退人工更新。
        latest = EVIDENCE / 'BeforeLatest' / current_hash / source.relative_to(ROOT)
        if not latest.exists():
            latest.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(source, latest)
    return current_hash


def scene_snapshot():
    """按当前真实组件与每实例世界变换取快照；不以旧数量或Actor原点代替实例位置。"""
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_path_name().split('.')[0] != FORMAL:
        raise RuntimeError('必须先在编辑模式打开正式新手村，不能检查PIE副本')
    def transform(value):
        # repr包含临时内存地址，不能用于跨进程变换指纹。
        t, r, s = value.translation, value.rotation, value.scale3d
        return {'translation': [t.x, t.y, t.z], 'rotation': [r.x, r.y, r.z, r.w], 'scale': [s.x, s.y, s.z]}
    rows = []
    for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        item = {'actor': actor.get_path_name(), 'class': actor.get_class().get_name(),
                'transform': transform(actor.get_actor_transform()), 'components': []}
        for component in actor.get_components_by_class(u.StaticMeshComponent):
            mesh = component.get_editor_property('static_mesh')
            if not mesh:
                continue
            row = {'component': component.get_name(), 'mesh': mesh.get_path_name(),
                   'transform': transform(component.get_world_transform()),
                   'materials': [component.get_material(i).get_path_name() if component.get_material(i) else None
                                 for i in range(component.get_num_materials())]}
            if isinstance(component, u.InstancedStaticMeshComponent):
                transforms = [transform(component.get_instance_transform(i, world_space=True))
                              for i in range(component.get_instance_count())]
                row['instances'] = len(transforms)
                row['instance_transform_sha256'] = hashlib.sha256(json.dumps(transforms, sort_keys=True).encode()).hexdigest()
            item['components'].append(row)
        rows.append(item)
    return {'world': world.get_path_name(), 'actors': sorted(rows, key=lambda row: row['actor'])}


def inspect():
    """建立保护基线和植物材质清单；同轮基线已存在时禁止无意覆盖。"""
    baseline = EVIDENCE / 'Baseline.json'
    if baseline.exists():
        raise RuntimeError('已有本轮基线，先审查执行记录，禁止覆盖')
    scene = scene_snapshot()
    protection_path = EVIDENCE / 'SceneProtection.json'
    if not protection_path.exists():
        write('SceneProtection.json', scene)
    materials = set()
    for actor in scene['actors']:
        for component in actor['components']:
            if '/DBAFoliagePack_Core/' in component['mesh']:
                materials.update(path for path in component['materials'] if path)
    paths = sorted(materials | {MPC, FORMAL})
    scene['backup_hashes'] = {path: backup(path) for path in paths}
    scene['plant_materials'] = sorted(materials)
    write('Baseline.json', scene)
    print(json.dumps({'actors': len(scene['actors']), 'plant_materials': sorted(materials)}, ensure_ascii=False))


def author_platform():
    """扩展唯一平台MPC并制作两份通用函数；不修改植物实例或正式地图。"""
    require_idle()
    collection = u.load_asset(MPC)
    if not collection:
        raise RuntimeError('缺少平台唯一MPC')
    backup(MPC)
    scalars = list(collection.get_editor_property('scalar_parameters'))
    vectors = list(collection.get_editor_property('vector_parameters'))
    if not any(str(p.get_editor_property('parameter_name')) == contract.WIND_SCALAR for p in scalars):
        p = u.CollectionScalarParameter(); p.set_editor_property('parameter_name', contract.WIND_SCALAR)
        p.set_editor_property('default_value', 2026.); scalars.append(p)
    for name in contract.WIND_VECTORS:
        if not any(str(p.get_editor_property('parameter_name')) == name for p in vectors):
            p = u.CollectionVectorParameter(); p.set_editor_property('parameter_name', name)
            p.set_editor_property('default_value', u.LinearColor(0, 0, 0, 0)); vectors.append(p)
    if len(scalars) != 9 or len(vectors) != 3:
        raise RuntimeError('MPC含未知参数，先审核平台合同，禁止重置旧参数')
    collection.set_editor_property('scalar_parameters', scalars)
    collection.set_editor_property('vector_parameters', vectors)
    OWNED.add(collection.get_path_name())
    global_function, create = acquire(contract.GLOBAL_FUNCTION)
    if create:
        def collection_parameter(name):
            p = node(global_function, u.MaterialExpressionCollectionParameter)
            p.set_editor_property('collection', collection); p.set_editor_property('parameter_name', name)
            return p
        time = node(global_function, u.MaterialExpressionTime)
        position = node(global_function, u.MaterialExpressionWorldPosition)
        position.set_editor_property('world_position_shader_offset', u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        source = {name: collection_parameter(name) for name in (*contract.WIND_VECTORS, contract.WIND_SCALAR)}
        state = custom(global_function, contract.GLOBAL_STATE_HLSL,
                       {'Time': (time, ''), 'PositionWS': (position, 'XYZ'),
                        'From': (source['GlobalWindFrom'], ''), 'To': (source['GlobalWindTo'], ''),
                        'Timing': (source['GlobalWindTiming'], ''), 'Seed': (source['GlobalWindSeed'], '')},
                       u.CustomMaterialOutputType.CMOT_FLOAT4)
        for order, (name, code, scalar_output) in enumerate([
                ('DirectionWS', 'return float3(State.xy,0.0);', False),
                ('Strength', 'return State.z;', True), ('Phase', 'return State.w;', True)]):
            output = custom(global_function, code, {'State': (state, '')},
                            u.CustomMaterialOutputType.CMOT_FLOAT1 if scalar_output else u.CustomMaterialOutputType.CMOT_FLOAT3)
            function_output(global_function, name, output, order=order)
        server_time = custom(global_function, 'return Time+Timing.w;',
                             {'Time': (time, ''), 'Timing': (source['GlobalWindTiming'], '')}, u.CustomMaterialOutputType.CMOT_FLOAT1)
        function_output(global_function, 'ServerTimeSeconds', server_time, order=3)
        complete(global_function)
    generic_function, create = acquire(contract.GENERIC_FUNCTION)
    if create:
        state = call(generic_function, global_function)
        args = {'PositionLS': (function_input(generic_function, 'PositionLS', True), ''),
                'Weight': (function_input(generic_function, 'Weight', default=1.), ''),
                'RootZcm': (function_input(generic_function, 'RootZcm'), ''),
                'HeightCm': (function_input(generic_function, 'HeightCm', default=500.), ''),
                'AmplitudeCm': (function_input(generic_function, 'AmplitudeCm', default=12.), ''),
                'MaxDistanceCm': (function_input(generic_function, 'MaxDistanceCm'), ''),
                'DirectionWS': (state, 'DirectionWS'), 'Strength': (state, 'Strength'), 'Phase': (state, 'Phase')}
        distance = node(generic_function, u.MaterialExpressionDistance)
        position = node(generic_function, u.MaterialExpressionWorldPosition)
        position.set_editor_property('world_position_shader_offset', u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        camera = node(generic_function, u.MaterialExpressionCameraPositionWS)
        wire(position, distance, 'A', 'XYZ'); wire(camera, distance, 'B')
        args['DistanceCm'] = (distance, '')
        result = custom(generic_function, contract.GENERIC_WPO_HLSL, args, u.CustomMaterialOutputType.CMOT_FLOAT3)
        function_output(generic_function, 'WorldPositionOffset', result)
        complete(generic_function)
    write('PlatformGraphAuthored.json', {'assets': [MPC, contract.GLOBAL_FUNCTION, contract.GENERIC_FUNCTION],
                                        'saved': False, 'native_reload_verified': False})


def author_st9():
    """派生ST9风和母材质；保留引擎Motion/Branch函数、UV及非风材质支路。"""
    require_idle()
    global_function = u.load_asset(contract.GLOBAL_FUNCTION)
    if not global_function:
        raise RuntimeError('先完成平台材质函数')
    wind, create = acquire(contract.ST9_FUNCTION, '/SpeedTreeImporter/SpeedTree9/SpeedTreeWind')
    if create:
        original = expressions(wind)
        state = call(wind, global_function)
        rewired = 0
        # 所有Time消费者只改接统一服务器时间；其他依赖和原节点保留以便对照。
        for expression in original:
            inputs = LIB.get_inputs_for_material_function_expression(wind, expression)
            for pin, source in zip(LIB.get_material_expression_input_names(expression), inputs):
                if isinstance(source, u.MaterialExpressionTime):
                    wire(state, expression, pin, 'ServerTimeSeconds'); rewired += 1
        if rewired != 4:
            raise RuntimeError('锁定ST9 Time拓扑变化，需审查后适配: ' + str(rewired))
        # 只保护UV2.x/UV2.y/UV5.x数值权重；PackedPosition/Direction保留原始位模式。
        for expression in original:
            if not isinstance(expression, u.MaterialExpressionComponentMask):
                continue
            sources = LIB.get_inputs_for_material_function_expression(wind, expression)
            if not sources or not isinstance(sources[0], u.MaterialExpressionTextureCoordinate):
                continue
            channel = sources[0].get_editor_property('coordinate_index')
            if channel not in (2, 5):
                continue
            guard = custom(wind, contract.FINITE_WEIGHT_HLSL, {'Weight': (expression, '')}, u.CustomMaterialOutputType.CMOT_FLOAT1)
            for consumer in original:
                for pin, source in zip(LIB.get_material_expression_input_names(consumer),
                                       LIB.get_inputs_for_material_function_expression(wind, consumer)):
                    if source == expression:
                        wire(guard, consumer, pin)
        output = next(e for e in original if isinstance(e, u.MaterialExpressionFunctionOutput)
                      and str(e.get_editor_property('output_name')) == 'WorldPositionOffset')
        source = LIB.get_inputs_for_material_function_expression(wind, output)[0]
        bounded = custom(wind, contract.BOUNDED_ST9_WPO_HLSL,
                         {'Offset': (source, ''), 'Strength': (state, 'Strength'),
                          'MaxCm': (scalar(wind, 'MaxWindDisplacementCm', 30.), '')}, u.CustomMaterialOutputType.CMOT_FLOAT3)
        wire(bounded, output)
        normal = next(e for e in original if isinstance(e, u.MaterialExpressionFunctionOutput)
                      and str(e.get_editor_property('output_name')) == 'NormalOffset')
        normal_source = LIB.get_inputs_for_material_function_expression(wind, normal)[0]
        normal_scale = node(wind, u.MaterialExpressionMultiply)
        wire(normal_source, normal_scale, 'A'); wire(state, normal_scale, 'B', 'Strength'); wire(normal_scale, normal)
        complete(wind)
    master, create = acquire(contract.ST9_MASTER, '/SpeedTreeImporter/SpeedTree9/SpeedTreeMaster', True)
    if create:
        native_calls = [e for e in expressions(master) if isinstance(e, u.MaterialExpressionMaterialFunctionCall)
                        and 'SpeedTreeWind.SpeedTreeWind' in str(e.get_editor_property('material_function'))]
        if len(native_calls) != 1:
            raise RuntimeError('ST9母材质风调用不唯一')
        native_calls[0].set_editor_property('material_function', wind)
        state = call(master, global_function)
        pin = next(p for p in LIB.get_material_expression_input_names(native_calls[0]) if p.split(' ')[0] == 'WindDirection')
        wire(state, native_calls[0], pin, 'DirectionWS')
        # Native风输出已经整体受强度和边界控制；不能再缩放母材质CameraFacing位移。
        complete(master)
    write('ST9GraphAuthored.json', {'assets': [contract.ST9_FUNCTION, contract.ST9_MASTER],
                                   'engine_motion_reused': True, 'saved': False, 'visual_verified': False})


def bind_village():
    """按当前植物清单重设父材质/响应并接原型树WPO；不移动、重生成或保存正式地图。"""
    require_idle()
    scene = scene_snapshot()
    protection_path = EVIDENCE / 'SceneProtection.json'
    if not protection_path.exists():
        write('SceneProtection.json', scene)
    ini = (ROOT / 'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
    if '("' + FORMAL + '","db.weather.clear@1")' not in ini:
        raise RuntimeError('先在独占窗口运行配置准备工具；禁止仅接材质却不激活服务器风')
    master = u.load_asset(contract.ST9_MASTER); generic = u.load_asset(contract.GENERIC_FUNCTION)
    if not master or not generic:
        raise RuntimeError('必须先完成ST9及平台样板')
    material_paths = {p for a in scene['actors'] for c in a['components'] if contract.is_plant_mesh(c['mesh'])
                      for p in c['materials'] if p}
    changes = []
    prototype_parents = set()
    for path in sorted(material_paths):
        material = u.load_asset(path)
        if not isinstance(material, u.MaterialInstanceConstant):
            raise RuntimeError('植物材质不是可审阅的MIC: ' + path)
        backup(path)
        if path.startswith('/DBAFoliagePack_Core/Foliage/'):
            LIB.set_material_instance_parent(material, master)
            response = contract.FERN_RESPONSE if '/Grass/' in path else contract.TREE_RESPONSE
            for name, value in response.items():
                if not LIB.set_material_instance_scalar_parameter_value(material, name, value):
                    raise RuntimeError('ST9参数不存在: ' + name)
        else:
            parent = material.get_editor_property('parent')
            if not isinstance(parent, u.Material):
                raise RuntimeError('原型树材质父链变化，需检查: ' + path)
            prototype_parents.add(parent.get_path_name())
        OWNED.add(path); changes.append(path)
    for path in sorted(prototype_parents):
        material = u.load_asset(path); backup(path)
        if LIB.get_material_property_input_node(material, u.MaterialProperty.MP_WORLD_POSITION_OFFSET):
            raise RuntimeError('原型树已有WPO，禁止覆盖: ' + path)
        function = call(material, generic)
        position = node(material, u.MaterialExpressionPreSkinnedPosition)
        pin = next(p for p in LIB.get_material_expression_input_names(function) if p.split(' ')[0] == 'PositionLS')
        wire(position, function, pin)
        if not LIB.connect_material_property(function, 'WorldPositionOffset', u.MaterialProperty.MP_WORLD_POSITION_OFFSET):
            raise RuntimeError('原型树WPO接线失败')
        complete(material)
    presets = configure_weather_presets()
    write('VillageMaterialBindings.json', {'materials': changes, 'prototype_masters': sorted(prototype_parents),
                                          'weather_presets': presets, 'formal_world_saved': False,
                                          'weather_activation_pending': False})


def configure_weather_presets():
    """只补既有八份天气Definition的风字段；其他雨雪/温度/时段保持原值。"""
    require_idle()
    changed = []
    for name in ('Clear', 'Cloudy', 'LightRain', 'HeavyRain', 'LightSnow', 'HeavySnow', 'AfterRain', 'AfterSnow'):
        path = '/DBAWorldPack_Village/Weather/Definitions/DA_DBA_Weather_' + name
        definition = u.load_asset(path)
        if not isinstance(definition, u.GamePlatformWeatherPresetDefinition):
            raise RuntimeError('缺少真实天气Definition: ' + path)
        backup(path)
        weather = definition.get_editor_property('weather')
        state = weather.get_editor_property('state')
        state.set_editor_property('wind_yaw_degrees', 0.)
        state.set_editor_property('wind_gust_strength', .2)
        state.set_editor_property('wind_gust_frequency_hz', .1)
        if name == 'Clear':
            state.set_editor_property('wind_intensity', .25)
        weather.set_editor_property('state', state); definition.set_editor_property('weather', weather)
        OWNED.add(definition.get_path_name()); changed.append(path)
    return changed


def author_review():
    """独立审核地图：三植物四向StaticMesh/HISM与非均匀缩放；要求先登记NeverCook。"""
    require_idle()
    config = (ROOT / 'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
    if '+DirectoriesToNeverCook=(Path="/DBAWorldPack_Village/Development/Wind")' not in config:
        raise RuntimeError('尚未登记独立审核目录NeverCook，拒绝创建可能进入发布包的地图')
    if u.EditorAssetLibrary.does_asset_exist(contract.REVIEW_MAP):
        raise RuntimeError('审核地图已存在，先只读验证，不覆盖')
    baseline = json.loads((EVIDENCE / 'Baseline.json').read_text(encoding='utf-8'))
    meshes = sorted({c['mesh'] for a in baseline['actors'] for c in a['components']
                     if c['mesh'].startswith('/DBAFoliagePack_Core/Foliage/')})
    if len(meshes) != 3:
        raise RuntimeError('审核样本不是预期三种植物，先更新清单')
    if not u.EditorLevelLibrary.new_level(contract.REVIEW_MAP):
        raise RuntimeError('原生审核地图创建失败')
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    actors = []
    for row, path in enumerate(meshes):
        mesh = u.load_asset(path); transforms = []
        for column, yaw in enumerate((0, 90, 180, 270)):
            location = u.Vector(column * 1800., row * 2000., 0.)
            actor = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_object(mesh, location, u.Rotator(yaw=yaw))
            if not actor:
                raise RuntimeError('StaticMesh审核夹具生成失败')
            actor.set_actor_label('WindReview_Static_' + str(row) + '_' + str(yaw))
            actors.append(actor.get_path_name())
            transforms.append(u.Transform(location=u.Vector(location.x, location.y + 900., 0.),
                                           rotation=u.Rotator(yaw=yaw).quaternion(), scale=u.Vector(1., .8, 1.2)))
        actor = u.GamePlatformSurfaceMaterialAuthoringLibrary.spawn_instanced_wind_review_actor(world, mesh, transforms)
        if not actor:
            raise RuntimeError('HISM审核夹具生成失败')
        actor.set_actor_label('WindReview_HISM_' + str(row)); actors.append(actor.get_path_name())
    if not u.EditorLoadingAndSavingUtils.save_map(world, contract.REVIEW_MAP):
        raise RuntimeError('审核地图保存失败')
    write('ReviewFixture.json', {'map': contract.REVIEW_MAP, 'actors': actors, 'native_visual_verified': False})


def save_owned():
    """只保存本次明确触碰的包；不会SaveAll或保存当前正式地图，保留他人脏包。"""
    require_idle()
    saved = []
    for path in sorted(OWNED):
        asset = u.load_asset(path)
        if not asset or not u.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('原生保存失败: ' + path)
        saved.append(path)
    write('SavedOwnedAssets.json', {'assets': saved, 'author_pid': os.getpid(), 'reload_verified': False})


if __name__ == '__main__':
    inspect()
