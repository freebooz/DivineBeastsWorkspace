"""通过直接 Monolith 读取当前真实演示关卡，留存对象/宿主/尺寸证据。"""
from pathlib import Path
from collections import Counter
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginSceneFontAudit/20261030'

LABS = {
    'vfx': '/GamePlatformVFX/Development/Review/Maps/L_VFXReview',
    'surface_marks': '/GamePlatformSurface/Development/Review/Maps/L_GP_SurfaceMarksReview',
    'camera': '/Game/Development/PluginDemos/Maps/L_DBA_CameraEffects',
    'asset_gallery': '/Game/Development/PluginDemos/Maps/L_DBA_AssetGallery',
}


def inspect_current():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != ROOT / 'Game/DivineBeastsArena.uproject':
        raise RuntimeError('只读当前正式工程')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    package = world.get_path_name().split('.')[0]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    entries = []
    for actor in actors:
        origin, extent = actor.get_actor_bounds(False)
        entries.append({'label': actor.get_actor_label(), 'class': actor.get_class().get_path_name(),
                        'location_cm': list(actor.get_actor_location().to_tuple()),
                        'bounds_center_cm': list(origin.to_tuple()), 'bounds_extent_cm': list(extent.to_tuple())})
    game_mode = world.get_world_settings().get_editor_property('default_game_mode')
    data = {'package': package, 'reader_pid': os.getpid(), 'epoch': time.time(),
            'game_mode': game_mode.get_path_name() if game_mode else None,
            'actor_count': len(entries), 'classes': dict(Counter(e['class'] for e in entries)),
            'actors': entries, 'manual_acceptance': 'pending'}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ('Scene.' + package.rsplit('/', 1)[-1] + '.json')).write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {k: data[k] for k in ('package', 'game_mode', 'actor_count', 'classes')}


def author_lab(kind):
    """只创建缺失的最小真实观察关卡；播放仍由现有案例适配器执行。"""
    package = LABS[kind]
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != ROOT / 'Game/DivineBeastsArena.uproject':
        raise RuntimeError('拒绝其他工程')
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:
        raise RuntimeError('已有脏包，保留并拒绝关卡创建')
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        raise RuntimeError('关卡已存在；拒绝重建或覆盖: ' + package)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_service = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    sphere = unreal.load_asset('/Engine/BasicShapes/Sphere')
    if not isinstance(cube, unreal.StaticMesh) or not isinstance(sphere, unreal.StaticMesh):
        raise RuntimeError('原生观察网格缺失')
    if not level.new_level(package, is_partitioned_world=False):
        raise RuntimeError('真实关卡创建失败')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)

    def spawn(klass, name, position, rotation=(0, 0, 0), scale=(1, 1, 1)):
        actor = actor_service.spawn_actor_from_class(klass, unreal.Vector(*position),
            unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), transient=False)
        if actor is None:
            raise RuntimeError('Actor 创建失败: ' + name)
        actor.set_actor_label(name)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    def mesh(name, shape, position, scale):
        actor = spawn(unreal.StaticMeshActor, name, position, scale=scale)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component.set_static_mesh(shape):
            raise RuntimeError('观察网格绑定失败: ' + name)
        component.set_collision_profile_name('BlockAll')
        return actor

    mesh('PluginReview_Ground', cube, (0, 0, -35), (24, 20, .5))
    mesh('PluginReview_SourceMarker', sphere, (0, 0, 50), (.4, .4, .4))
    mesh('PluginReview_TargetMarker', sphere, (300, 0, 120), (1, 1, 1))
    mesh('PluginReview_Backdrop', cube, (900, 0, 180), (.2, 16, 4))
    spawn(unreal.PlayerStart, 'PluginReview_PlayerStart', (-650, -650, 220), (-10, 45, 0))
    sun = spawn(unreal.DirectionalLight, 'PluginReview_Sun', (0, 0, 1500), (-45, -35, 0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(3)
    spawn(unreal.SkyLight, 'PluginReview_SkyLight', (0, 0, 500))
    spawn(unreal.SkyAtmosphere, 'PluginReview_Atmosphere', (0, 0, 0))
    spawn(unreal.CameraActor, 'PluginReview_FixedCamera', (-650, -650, 300), (-12, 45, 0))
    if kind == 'surface_marks':
        for index, name in enumerate(('Bullet', 'Scorch', 'Slash', 'Fluid', 'Crack')):
            mesh('MarkPanel_' + name, cube, ((index - 2) * 300, 350, 170), (2.5, .3, 3.2))
        # 不新增自动播放器：交互页继续调用唯一 SurfaceMark 服务提交/取消。
    if kind == 'camera':
        for index in range(3):
            mesh('CameraReview_DepthReference_' + str(index), cube,
                 (index * 350, 250, 100 + index * 100), (1, 1, 2 + index * 2))
    if not level.save_current_level():
        raise RuntimeError('原生关卡保存失败: ' + package)
    result = inspect_current()
    result.update({'author_pid': os.getpid(), 'saved_epoch': time.time(), 'saved': True,
                   'effect_execution': 'existing_manifest_adapter_only', 'cold_reload': 'pending'})
    (OUT / ('Scene.Authored.' + kind + '.json')).write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return result
