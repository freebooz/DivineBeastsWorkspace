# -*- coding: utf-8 -*-
"""通过已核对项目身份的 Monolith/UE5.8 导入 Blender 神兽圣境。

第三层 DBAFrontEndPack 独占网格、纹理、材质和环境 Actor；不创建 Widget、
账号或权威世界，不修改唯一应用流程。先分批 import_resources，再 author_studio。
原工作室在第一次修改前完整备份；有未保存包时拒绝覆盖，保留既有舞台和落地锚点。
报告只记录实际保存结果，不代替客户端 Cook 和人工视觉签收。
"""
import hashlib
import json
import shutil
from pathlib import Path

import unreal

PACK = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / 'Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack'
SOURCE = PACK / 'SourceArt/Blender/ZodiacSanctuary'
ROOT = '/DBAFrontEndPack/CharacterPreview/Environment/ZodiacSanctuary'
STUDIO = '/DBAFrontEndPack/Maps/L_DBA_CharacterStudio'
TAG = 'DBA.Preview.ZodiacSanctuary'
ENVIRONMENT_SCALE = .45
MANIFEST = json.loads((SOURCE / 'SceneManifest.json').read_text(encoding='utf-8'))
ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
MATERIAL_ATTRIBUTES = {}


def require_editor():
    """拒绝错误工程、PIE 或未保存的其他编辑；只读取当前正式编辑器。"""
    project = Path(unreal.Paths.get_project_file_path()).name
    assert project == 'DivineBeastsArena.uproject', '当前不是正式神兽联盟主工程'
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), '先结束当前 PIE，不能改运行实例'


def save(asset):
    """只保存本调用所拥有的明确资产；保存失败直接终止。"""
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), '保存失败：' + asset.get_path_name()


def node(material, cls, **properties):
    """创建真实材质表达式，中文说明记录在表达式描述内。"""
    item = LIB.create_material_expression(material, cls)
    assert item, '材质表达式创建失败'
    for key, value in properties.items():
        item.set_editor_property(key, value)
    return item


def link(source, target, input_name, output_name=''):
    """连接原生材质节点；错误输入名不得被静默忽略。"""
    assert LIB.connect_material_expressions(source, output_name, target, input_name), '材质连接失败：' + input_name


def connect(expression, property_name):
    """连接 PBR 输出；节点图由 UE 编译而非 FBX 自动材质猜测。"""
    properties = {
        unreal.MaterialProperty.MP_BASE_COLOR: 'BaseColor',
        unreal.MaterialProperty.MP_ROUGHNESS: 'Roughness',
        unreal.MaterialProperty.MP_METALLIC: 'Metallic',
        unreal.MaterialProperty.MP_NORMAL: 'Normal',
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: 'EmissiveColor',
    }
    attributes = MATERIAL_ATTRIBUTES[expression.get_outer().get_path_name()]
    link(expression, attributes, properties[property_name])


def create_material(record):
    """还原源 PBR 基色/粗糙度/金属度；水幕运动仅由 GPU 材质时间驱动。

    不使用业务 Tick，不复制网络状态。水幕是视觉表面，不承担瀑布物理或玩法。
    重跑只清理本圣境拥有的同名材质节点，不修改其他项目材质。
    """
    path = ROOT + '/Materials/' + record['name']
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else ASSETS.create_asset(record['name'], ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    assert isinstance(mat, unreal.Material), '材质身份冲突：' + path
    LIB.delete_all_material_expressions(mat)
    # 原生 MaterialAttributes 提供真实 ClearCoat 输入，避免依赖 Python 未公开的隐藏枚举。
    mat.set_editor_property('use_material_attributes', True)
    attributes = node(mat, unreal.MaterialExpressionMakeMaterialAttributes)
    MATERIAL_ATTRIBUTES[mat.get_path_name()] = attributes
    assert LIB.connect_material_property(attributes, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES), 'PBR 属性节点连接失败'
    mat.set_editor_property('two_sided', record['two_sided'])
    color = node(mat, unreal.MaterialExpressionVectorParameter, parameter_name='SurfaceTint', default_value=unreal.LinearColor(*record['base_color'], 1), group='Surface', desc='源 Blender 线性基色，不重复 Gamma 编码')
    base = color
    if record['texture']:
        texture_path = ROOT + '/Textures/' + Path(record['texture']).stem
        texture = unreal.load_asset(texture_path)
        assert texture, '源贴图尚未导入：' + texture_path
        base = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='BaseColorTexture', group='Surface', texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, desc='原创低对比 sRGB 基色贴图；不叠加颗粒或锐化')
    if record['name'].endswith(('_Waterfall', '_Water')):
        coordinates = node(mat, unreal.MaterialExpressionTextureCoordinate, u_tiling=18 if record['name'].endswith('_Waterfall') else 4, v_tiling=2)
        panner = node(mat, unreal.MaterialExpressionPanner, speed_x=.01, speed_y=-.28 if record['name'].endswith('_Waterfall') else .035, desc='GPU 时间推进水纹；无业务状态轮询')
        link(coordinates, panner, 'Coordinate')
        append = node(mat, unreal.MaterialExpressionAppendVector)
        zero = node(mat, unreal.MaterialExpressionConstant, r=0.)
        link(panner, append, 'A'); link(zero, append, 'B')
        noise = node(mat, unreal.MaterialExpressionNoise, levels=2, quality=1, scale=1., output_min=.65, output_max=1., desc='低频连续水纹，避免高频闪点')
        link(append, noise, 'World Position')
        multiply = node(mat, unreal.MaterialExpressionMultiply)
        link(color, multiply, 'A'); link(noise, multiply, 'B')
        base = multiply
    connect(base, unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='SurfaceRoughness', group='Surface', default_value=record['roughness'], slider_min=.04, slider_max=1., desc='表面粗糙度；低值获得连续宽阔高光，不用碎片闪点')
    metallic = node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='MetallicStrength', group='Surface', default_value=record['metallic'], slider_min=0., slider_max=1., desc='物理金属度；玉石与石材保持介电表面')
    if record.get('normal_texture'):
        normal_asset = unreal.load_asset(ROOT + '/Textures/' + Path(record['normal_texture']).stem)
        assert normal_asset, '真实法线纹理缺失'
        normal_sample = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='NormalTexture', group='Surface', texture=normal_asset, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, desc='线性切线法线；导入显式翻转 OpenGL 绿通道')
        connect(normal_sample, unreal.MaterialProperty.MP_NORMAL)
    if record.get('orm_texture'):
        orm_asset = unreal.load_asset(ROOT + '/Textures/' + Path(record['orm_texture']).stem)
        assert orm_asset, '真实 ORM 控制纹理缺失'
        orm = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='SurfaceControlTexture', group='Surface', texture=orm_asset, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, desc='线性 R=AO、G=粗糙度控制(中值0.5)、B=金属度倍率')
        link(orm, attributes, 'AmbientOcclusion', 'R')
        rough_control = node(mat, unreal.MaterialExpressionMultiply)
        link(orm, rough_control, 'A', 'G'); link(node(mat, unreal.MaterialExpressionConstant, r=2.), rough_control, 'B')
        rough_multiply = node(mat, unreal.MaterialExpressionMultiply)
        link(rough_control, rough_multiply, 'A'); link(roughness, rough_multiply, 'B')
        roughness = rough_multiply
    connect(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    connect(metallic, unreal.MaterialProperty.MP_METALLIC)
    if record.get('coat_weight', 0):
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT)
        link(node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='PolishCoatWeight', group='Polish', default_value=record['coat_weight'], slider_min=0., slider_max=1., desc='玉石/雕塑薄抛光层，保持柔和连续高光'), attributes, 'ClearCoat')
        link(node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='PolishCoatRoughness', group='Polish', default_value=record['coat_roughness'], slider_min=.04, slider_max=1.), attributes, 'ClearCoatRoughness')
    if record['emission_strength']:
        emission = node(mat, unreal.MaterialExpressionMultiply)
        link(color, emission, 'A')
        link(node(mat, unreal.MaterialExpressionConstant, r=record['emission_strength']), emission, 'B')
        connect(emission, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat)
    save(mat)
    return mat


def import_task(filename, folder, name, options=None):
    """自动导入唯一资产，失败返回空路径时立即终止，禁止固定成功。"""
    task = unreal.AssetImportTask()
    task.filename = str(filename); task.destination_path = folder; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    if options:
        task.options = options
    ASSETS.import_asset_tasks([task])
    assert task.imported_object_paths, '引擎未产生资产：' + str(filename)
    asset = unreal.load_asset(task.imported_object_paths[0])
    assert asset, '保存后无法回读资产'
    return asset


def import_resources(start=0, count=4, prepare_materials=False):
    """小批量导入防止阻塞编辑器；start/count 仅指制作清单中的网格区间。

    第一次必须 prepare_materials=True；厘米与坐标转换由原生 FBX 导入器完成。
    端侧纯表现，不生成碰撞；整体网格静态，原生 Nanite 只用于非水面/非树叶。
    """
    require_editor()
    if prepare_materials:
        for texture in sorted({r[key] for r in MANIFEST['materials'] for key in ('texture', 'normal_texture', 'orm_texture') if r.get(key)}):
            image = import_task(SOURCE / texture, ROOT + '/Textures', Path(texture).stem)
            if texture.endswith('_Normal.png'):
                image.set_editor_property('srgb', False)
                image.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
                image.set_editor_property('flip_green_channel', True)
            elif texture.endswith('_ORM.png'):
                image.set_editor_property('srgb', False)
                image.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
            else:
                image.set_editor_property('srgb', True)
            save(image)
        for record in MANIFEST['materials']:
            create_material(record)
    for record in MANIFEST['objects'][start:start + count]:
        options = unreal.FbxImportUI()
        options.import_mesh = True; options.import_as_skeletal = False
        options.import_materials = False; options.import_textures = False
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        data = options.static_mesh_import_data
        data.combine_meshes = True; data.convert_scene = True; data.convert_scene_unit = True
        data.auto_generate_collision = False; data.generate_lightmap_u_vs = False
        data.build_nanite = not record['name'].endswith(('Foliage', 'WaterPlants', 'Waterfalls', 'Water'))
        mesh = import_task(SOURCE / record['fbx'], ROOT + '/Meshes', record['name'], options)
        for index, slot in enumerate(mesh.static_materials):
            material = unreal.load_asset(ROOT + '/Materials/' + str(slot.material_slot_name))
            assert material, 'FBX 材质槽没有唯一映射：' + str(slot.material_slot_name)
            mesh.set_material(index, material)
        save(mesh)
        print('SANCTUARY_MESH_SAVED ' + mesh.get_path_name())


def author_studio():
    """原位装配共用预览地图，保留稳定软路径、平台舞台和已有风吹锚点。

    旧地形只隐藏不删除；首次改图前备份旧 UMAP/外置 Actor。角色资产不复制，
    场景缩至 45% 为中央英雄及两侧 UI 留空间。只保存当前工作室，不保存其他包。
    """
    require_editor()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0] == STUDIO, '先用 Monolith 打开既定工作室'
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '工作室存在未保存修改，拒绝覆盖'
    backup = Path(unreal.Paths.project_saved_dir()).resolve() / 'Validation/ZodiacSanctuary/BeforeSanctuary'
    if not backup.exists():
        backup.mkdir(parents=True)
        shutil.copy2(PACK / 'Content/Maps/L_DBA_CharacterStudio.umap', backup / 'L_DBA_CharacterStudio.umap')
        for category in ('__ExternalActors__', '__ExternalObjects__'):
            external = PACK / 'Content' / category / 'Maps/L_DBA_CharacterStudio'
            if external.exists():
                shutil.copytree(external, backup / category)
    api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = {a.get_actor_label(): a for a in api.get_all_level_actors()}
    stage = actors['CharacterPreviewStage']
    assert stage.get_class().get_name() == 'GamePlatformCharacterPreviewStage', '平台舞台类型已变，须独立审查'
    for item in actors.values():
        if item.get_class().get_name() in ('Landscape', 'LandscapeStreamingProxy', 'VolumetricCloud') or item.get_actor_label() in ('PreviewFloor', 'PreviewBackdrop'):
            item.set_actor_hidden_in_game(True)
            item.set_is_temporarily_hidden_in_editor(True)
    for record in MANIFEST['objects']:
        label = record['name']
        mesh = unreal.load_asset(ROOT + '/Meshes/' + label)
        assert isinstance(mesh, unreal.StaticMesh), '未完成网格导入：' + label
        actor = actors.get(label) or api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
        actor.set_actor_label(label); actor.set_folder_path('ZodiacSanctuary')
        actor.set_actor_rotation(unreal.Rotator(0, -90, 0), False)
        actor.set_actor_scale3d(unreal.Vector(ENVIRONMENT_SCALE, ENVIRONMENT_SCALE, ENVIRONMENT_SCALE))
        actor.tags = [unreal.Name(TAG)]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_editor_property('static_mesh', mesh)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    pivot = next(c for c in stage.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'CameraPivot')
    pivot.set_editor_property('relative_location', unreal.Vector(330, 0, 340))
    camera = stage.get_component_by_class(unreal.CameraComponent)
    camera.set_editor_property('relative_rotation', unreal.Rotator(-8, 180, 0))
    camera.set_editor_property('field_of_view', 90.)
    sun = actors['DirectionalLight']; sun.set_actor_rotation(unreal.Rotator(-34, -35, 0), False)
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity', 3.)
    light.set_editor_property('atmosphere_sun_light', True)
    sky = actors['SkyLight'].get_component_by_class(unreal.SkyLightComponent)
    sky.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.set_editor_property('real_time_capture', True)
    sky.set_editor_property('intensity', .8)
    fog = actors['ExponentialHeightFog'].get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog.set_editor_property('fog_density', .025)
    fog.set_editor_property('fog_height_falloff', .2)
    fog.set_editor_property('start_distance', 550.)
    fog.set_editor_property('enable_volumetric_fog', True)
    volume = actors['PreviewExposure']; settings = volume.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_min_brightness', True)
    settings.set_editor_property('override_auto_exposure_max_brightness', True)
    settings.set_editor_property('auto_exposure_min_brightness', 1.)
    settings.set_editor_property('auto_exposure_max_brightness', 1.)
    settings.set_editor_property('override_auto_exposure_bias', True)
    settings.set_editor_property('auto_exposure_bias', 0.)
    volume.set_editor_property('settings', settings)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), '工作室保存失败'
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_world_location(), camera.get_world_rotation())
    report = {'map': STUDIO, 'environment_scale': ENVIRONMENT_SCALE, 'mesh_actor_count': len(MANIFEST['objects']), 'source_blend_sha256': hashlib.sha256((SOURCE / 'DBA_ZodiacSanctuary.blend').read_bytes()).hexdigest(), 'backup': str(backup), 'stage_class': stage.get_class().get_name(), 'camera_location_cm': str(camera.get_world_location()), 'actions': ['FBX import', 'PBR material graph', 'preserve stage and foliage anchor', 'save studio'], 'client_cook': 'not_run'}
    evidence = PACK / 'Docs/ZodiacSanctuaryImportManifest.json'
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('SANCTUARY_STUDIO_SAVED ' + json.dumps(report, ensure_ascii=False))
