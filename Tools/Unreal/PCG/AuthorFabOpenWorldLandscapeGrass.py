# -*- coding: utf-8 -*-
"""在 Monolith 中为已导入的 Fab 花草创建可复用的 Landscape Grass 资源。

只创建新资产，不修改原包、正式地图、已有材质或 PlayerStart。原包交互分支保留；
交互位置的运行时数据仍需由集成方提供。成功不代表场景接入或人工验收。
"""
import json
import unreal as u

SRC = '/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage'
DEST = '/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld'
MASTER = DEST + '/M_DBA_OpenWorldFoliage_GlobalWind'
INSTANCE = DEST + '/MI_DBA_OpenWorldFoliage_GlobalWind'
GRASS = DEST + '/LGT_DBA_OpenWorldGrass'
FLOWERS = DEST + '/LGT_DBA_OpenWorldFlowers'
WIND = '/GamePlatformSurface/MaterialFunctions/Vegetation/MF_GP_DirectionalVegetationWind'
ALL = [MASTER, INSTANCE, GRASS, FLOWERS]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def run():
    require(u.Paths.get_project_file_path().replace('\\', '/').endswith('/Game/DivineBeastsArena.uproject'), '工程不匹配')
    dirty = u.EditorLoadingAndSavingUtils.get_dirty_map_packages() + u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    require(all(p.get_name() == MASTER for p in dirty), '编辑器存在其他未保存内容')
    resume = u.EditorAssetLibrary.does_asset_exist(MASTER)
    require(not any(u.EditorAssetLibrary.does_asset_exist(p) for p in ALL[1:]), '目标资产已存在，拒绝覆盖')
    if resume:
        require(len(dirty) == 1, '已有目标母材质不是本次未保存的作者状态')
    lib = u.MaterialEditingLibrary
    source = require(u.load_asset(SRC + '/Materials/MA_OpenWorldFoliage'), '缺少原材质')
    source_instance = require(u.load_asset(SRC + '/Materials/MI_OpenWorldFoliage'), '缺少原材质实例')
    wind = require(u.load_asset(WIND), '缺少统一风函数')
    registry = u.AssetRegistryHelpers.get_asset_registry()
    candidates = registry.get_assets_by_path(SRC + '/Meshes', recursive=True)
    meshes = sorted((str(a.asset_name), a.get_asset()) for a in candidates
                    if str(a.asset_class_path.asset_name) == 'StaticMesh' and
                    (str(a.asset_name).startswith('openWorld_grass_') or str(a.asset_name).startswith('openWorld_flower_')))
    grasses = [m for n, m in meshes if n.startswith('openWorld_grass_')]
    flowers = [m for n, m in meshes if n.startswith('openWorld_flower_')]
    require(len(grasses) == 24 and len(flowers) == 12, '原包植物清单与已核验清单不同')
    created = []
    try:
        if not resume:
            require(u.EditorAssetLibrary.duplicate_asset(source.get_path_name(), MASTER), '复制原材质失败')
            created.append(MASTER)
        material = require(u.load_asset(MASTER), '无法读取新材质')
        nodes = lib.get_material_expressions(material)
        old_wind = [n for n in nodes if isinstance(n, u.MaterialExpressionMaterialFunctionCall) and
                    'SimpleGrassWind.SimpleGrassWind' in str(n.get_editor_property('material_function'))]
        require(len(old_wind) == 1, '原风节点拓扑不唯一')
        add = lib.get_material_property_input_node(material, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        require(isinstance(add, u.MaterialExpressionAdd), '原位移拓扑不匹配')
        inputs = lib.get_inputs_for_material_expression(material, add)
        require(len(inputs) == 2 and inputs[0] == old_wind[0] and
                isinstance(inputs[1], u.MaterialExpressionMaterialFunctionCall) and
                'Interaction.Interaction' in str(inputs[1].get_editor_property('material_function')), '原交互支路不匹配')
        wind_inputs = lib.get_inputs_for_material_expression(material, old_wind[0])
        require(len(wind_inputs) == 4 and isinstance(wind_inputs[1], u.MaterialExpressionVertexColor), '原顶点风权重不匹配')
        if resume:
            require(len(nodes) == 42, '待续材质节点数不匹配')
            position = require(next((n for n in nodes if n.get_name() == 'MaterialExpressionWorldPosition_0'), None), '待续位置节点缺失')
            transform = require(next((n for n in nodes if n.get_name() == 'MaterialExpressionTransformPosition_0'), None), '待续转换节点缺失')
            call = require(next((n for n in nodes if isinstance(n, u.MaterialExpressionMaterialFunctionCall) and n.get_editor_property('material_function') == wind), None), '待续风节点缺失')
            params = {str(n.get_editor_property('parameter_name')): n for n in nodes if isinstance(n, u.MaterialExpressionScalarParameter)}
            height, amplitude, distance = (params[k] for k in ('GP_FabPlantHeightCm', 'GP_FabPlantWindAmplitudeCm', 'GP_FabPlantWindDistanceCm'))
        else:
            position = require(lib.create_material_expression(material, u.MaterialExpressionWorldPosition, -1000, 1200), '创建位置节点失败')
            transform = require(lib.create_material_expression(material, u.MaterialExpressionTransformPosition, -800, 1200), '创建坐标转换失败')
            call = require(lib.create_material_expression(material, u.MaterialExpressionMaterialFunctionCall, -500, 1200), '创建风函数节点失败')
            def scalar(name, value, y):
                n = require(lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -800, y), '创建参数失败')
                n.set_editor_property('parameter_name', name)
                n.set_editor_property('default_value', value)
                return n
            height = scalar('GP_FabPlantHeightCm', 40., 1450)
            amplitude = scalar('GP_FabPlantWindAmplitudeCm', 25., 1600)
            distance = scalar('GP_FabPlantWindDistanceCm', 10000., 1750)
        position.set_editor_property('world_position_shader_offset', u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        transform.set_editor_property('transform_source_type', u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        transform.set_editor_property('transform_type', u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE)
        call.set_editor_property('material_function', wind)
        def connect(a, out, b, pin):
            require(lib.connect_material_expressions(a, out, b, pin), '连接失败: ' + pin)
        connect(position, 'XYZ', transform, '')
        connect(transform, '', call, 'PositionLS')
        connect(wind_inputs[1], 'R', call, 'Weight')
        connect(height, '', call, 'HeightCm')
        connect(amplitude, '', call, 'AmplitudeCm')
        connect(distance, '', call, 'MaxDistanceCm')
        connect(call, 'WorldPositionOffset', add, 'A')
        require(lib.get_inputs_for_material_expression(material, add)[1] == inputs[1], '交互支路意外变化')
        errors = list(lib.recompile_material(material))
        require(not errors, '材质编译失败: ' + str(errors))
        require(u.EditorAssetLibrary.save_loaded_asset(material), '材质保存失败')
        require(u.EditorAssetLibrary.duplicate_asset(source_instance.get_path_name(), INSTANCE), '复制原材质实例失败')
        created.append(INSTANCE)
        instance = require(u.load_asset(INSTANCE), '读取材质实例失败')
        lib.set_material_instance_parent(instance, material)
        require(u.EditorAssetLibrary.save_loaded_asset(instance), '材质实例保存失败')
        tools = u.AssetToolsHelpers.get_asset_tools()
        counts = {}
        for path, group, density in ((GRASS, grasses, 5.), (FLOWERS, flowers, .5)):
            asset = require(tools.create_asset(path.rsplit('/', 1)[1], DEST, u.LandscapeGrassType, u.LandscapeGrassTypeFactory()), '创建 Landscape Grass 失败')
            created.append(path)
            varieties = []
            for mesh in group:
                v = u.GrassVariety()
                v.set_editor_property('grass_mesh', mesh)
                v.set_editor_property('override_materials', [instance])
                d = u.PerPlatformFloat(); d.set_editor_property('default', density)
                start = u.PerPlatformInt(); start.set_editor_property('default', 6000)
                end = u.PerPlatformInt(); end.set_editor_property('default', 8000)
                dq = u.PerQualityLevelFloat(); dq.set_editor_property('default', density)
                startq = u.PerQualityLevelInt(); startq.set_editor_property('default', 6000)
                endq = u.PerQualityLevelInt(); endq.set_editor_property('default', 8000)
                v.set_editor_property('grass_density', d)
                v.set_editor_property('start_cull_distance', start)
                v.set_editor_property('end_cull_distance', end)
                v.set_editor_property('grass_density_quality', dq)
                v.set_editor_property('start_cull_distance_quality', startq)
                v.set_editor_property('end_cull_distance_quality', endq)
                v.set_editor_property('random_rotation', True)
                v.set_editor_property('align_to_surface', True)
                v.set_editor_property('cast_dynamic_shadow', False)
                v.set_editor_property('cast_contact_shadow', False)
                varieties.append(v)
            asset.set_editor_property('grass_varieties', varieties)
            asset.set_editor_property('enable_density_scaling', True)
            require(u.EditorAssetLibrary.save_loaded_asset(asset), 'Landscape Grass 保存失败')
            counts[path] = len(asset.get_editor_property('grass_varieties'))
        print('FAB_LANDSCAPE_GRASS_CREATED ' + json.dumps({'counts': counts, 'master': MASTER, 'instance': INSTANCE,
              'map_changed': False, 'interaction_runtime_verified': False, 'visual_verified': False}, ensure_ascii=False))
    except Exception:
        # 保留本轮脏资产供核查，避免 UE 内部引用导致回滚不完整。
        print('FAB_LANDSCAPE_GRASS_INCOMPLETE ' + json.dumps(created, ensure_ascii=False))
        raise


run()
