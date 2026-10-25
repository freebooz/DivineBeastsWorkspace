"""Monolith真实PIE固定镜头及参数取证；不保存地图或发行资产。

用capture_pie_movement_clip抓取真实viewport，延迟probe执行setup_viewport(camera_name)。
camera_name取actor_setup实际返回值或明确的PIE相机名；只移动PIE副本。
capture_state写本世界实际MPC参数。像素变化仍不代替人工方向/根部验收。
"""
import json
import pathlib
import unreal as u

OUT = pathlib.Path(u.Paths.project_dir()).resolve().parent / 'Saved/Validation/UnifiedVegetationWind20261024/Native20261025/Capture'


def setup_viewport(camera_name='CameraActor_0'):
    """Monolith临时CameraActor驱动真实PIE viewport，不经独立预览场景。"""
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
    if not world or 'UEDPIE_' not in world.get_path_name() or 'L_Village_Start' not in world.get_path_name():
        raise RuntimeError('仅在真实新手村PIE副本运行')
    trees = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
             if (c := a.get_component_by_class(u.StaticMeshComponent)) and
             (m := c.get_editor_property('static_mesh')) and '/WeepingWillow/' in m.get_path_name()]
    cameras = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor)
               if a.get_name() == camera_name]
    if not trees or len(cameras) != 1:
        raise RuntimeError('PIE需要真实柳树与唯一临时CameraActor')
    tree = sorted(trees, key=lambda a: a.get_path_name())[0]
    center, extent = tree.get_actor_bounds(False)
    radius = max(extent.x, extent.y, extent.z, 100.)
    location = center + u.Vector(-2.0 * radius, -2.4 * radius, .9 * radius)
    cameras[0].set_actor_location_and_rotation(location, u.MathLibrary.find_look_at_rotation(location, center), False, True)
    cameras[0].get_component_by_class(u.CameraComponent).set_editor_property('field_of_view', 50.)
    u.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cameras[0])
    print('WIND_VIEWPORT_READY', tree.get_path_name(), location, center)


def capture_state(name):
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
    if not world or 'UEDPIE_' not in world.get_path_name():
        raise RuntimeError('仅记录真实PIE参数')
    if not name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in name):
        raise ValueError('证据名只能使用字母数字下划线及连字符')
    collection = u.load_asset('/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal')
    values = {}
    for parameter in ('GlobalWindFrom', 'GlobalWindTo', 'GlobalWindTiming'):
        value = u.MaterialLibrary.get_vector_parameter_value(world, collection, parameter)
        values[parameter] = [value.r, value.g, value.b, value.a]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (name + '.json')).write_text(json.dumps({'world': world.get_path_name(), 'parameters': values}, indent=2), encoding='utf-8')
    print('WIND_STATE_SAVED', name)
