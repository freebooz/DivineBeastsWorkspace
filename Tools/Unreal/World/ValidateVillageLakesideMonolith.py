# -*- coding: utf-8 -*-
"""在实际新手村中只读审计湖边新增模型，保护原场景和树木实例。"""
import json
import re
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/ThatchHut20261024'
MAP = '/DBAWorldPack_Village/Maps/L_Village_Start'
FOLDERS = [
    '/DBAWorldPack_Village/Environment/Buildings/ThatchHut',
    '/DBAFoliagePack_Core/Foliage/Trees/WeepingWillow',
]


def stable(value):
    """移除Python打印的瞬态内存地址，保留序列化数值与资产身份。"""
    return re.sub(r'\(0x[0-9a-fA-F]+\)', '(address)', value)


def audit(phase='Reload'):
    """首次32个Actor及107棵树逐项不变，且只有两个新模型。"""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name() == MAP + '.L_Village_Start'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    by_path = {a.get_path_name(): a for a in actors}
    before = json.loads((OUT / 'ProtectedSceneBefore.json').read_text(encoding='utf-8'))
    instances = 0
    for old in before:
        actor = by_path[old['path']]
        assert actor.get_actor_label() == old['label'], old['label']
        assert stable(str(actor.get_actor_transform())) == stable(old['transform']), old['label']
        components = {c.get_name(): c for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent)}
        for item in old['instances']:
            c = components[item['name']]
            assert c.get_instance_count() == item['count']
            assert stable(str(c.static_mesh)) == stable(item['mesh'])
            current = [stable(str(c.get_instance_transform(i, world_space=True))) for i in range(c.get_instance_count())]
            assert current == [stable(t) for t in item['transforms']], old['label']
            instances += c.get_instance_count()
    labels = ['Village_ThatchHut_Lakeside', 'Village_WeepingWillow_Lakeside']
    added = [a for a in actors if a.get_path_name() not in {x['path'] for x in before}]
    assert sorted(a.get_actor_label() for a in added) == sorted(labels)
    assets = []
    textures = 0
    for folder in FOLDERS:
        for path in unreal.EditorAssetLibrary.list_assets(folder, True, False):
            obj = unreal.load_asset(str(path))
            assert obj and obj.get_class().get_name() != 'ObjectRedirector', str(path)
            assets.append(obj.get_path_name())
            if isinstance(obj, unreal.Texture2D):
                textures += 1
                normal = obj.get_name().endswith('_Normal')
                assert obj.get_editor_property('srgb') == (not normal), obj.get_name()
                assert obj.get_editor_property('adjust_min_alpha') == 0.0
            elif isinstance(obj, unreal.MaterialInstanceConstant):
                for parameter in ['ColorOpacity', 'NormalRoughness']:
                    texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(obj, parameter)
                    assert isinstance(texture, unreal.Texture2D) and texture.get_path_name().startswith(folder + '/'), (str(path), parameter)
    assert len(assets) == 39 and textures == 25, (len(assets), textures)
    report = {'phase': phase, 'map': world.get_path_name(), 'original_actors_unchanged': len(before),
              'original_tree_instances_unchanged': instances, 'assets': assets, 'textures': textures,
              'placements': [{'label': a.get_actor_label(), 'path': a.get_path_name(),
                              'transform': stable(str(a.get_actor_transform())),
                              'mesh': a.static_mesh_component.static_mesh.get_path_name(),
                              'collision_profile': str(a.static_mesh_component.get_collision_profile_name())} for a in added]}
    (OUT / ('Audit' + phase + '.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('LAKESIDE_AUDIT_PASS', phase, len(before), instances, len(assets), textures)
    return report


def viewport_capture(name='LakesideFinal'):
    """原生Editor GPU截图；调用后必须独立打开文件，不把调度当完成。"""
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert editor.get_editor_world().get_path_name() == MAP + '.L_Village_Start'
    location = unreal.Vector(-8200, 4200, 1200)
    target = unreal.Vector(-8500, 7450, 450)
    rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
    editor.set_level_viewport_camera_info(location, rotation)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    camera = actors.spawn_actor_from_class(unreal.CameraActor, location, rotation, transient=True)
    camera.set_actor_label('Lakeside_TransientCapture_DoNotSave')
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(80.0)
    actors.clear_actor_selection_set()
    task = unreal.AutomationLibrary.take_high_res_screenshot(
        1920, 1080, str(OUT / (name + '.png')), camera=camera, delay=0.5, force_game_view=True)
    assert task
    print('LAKESIDE_GPU_CAPTURE_SCHEDULED', name)
    # 调用者看到PNG实际写入后销毁此瞬态镜头，再审计并保存；不带进正式地图。
    return task, camera
