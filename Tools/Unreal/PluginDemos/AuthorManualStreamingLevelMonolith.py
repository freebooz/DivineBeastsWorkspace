"""直接 Monolith 创建真实流送小关卡；保留 Hub 手工出生点与已有对象。"""
from pathlib import Path
import json
import os
import shutil
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
HUB = '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'
STREAM = '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoStream'


def author():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split('.')[0] != HUB:
        raise RuntimeError('只允许当前开发 Hub')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('保留其它脏包，停止')
    if unreal.EditorAssetLibrary.does_asset_exist(STREAM):
        raise RuntimeError('不覆盖已有流送关卡')
    actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    before = {a.get_path_name(): a.get_actor_transform().export_text() for a in actor_api.get_all_level_actors() if isinstance(a, unreal.PlayerStart)}
    levels = unreal.EditorLevelUtils.get_levels(world)
    assert len(levels) == 1
    backup = OUT / 'Backups' / ('HubStreaming-' + str(time.time_ns()))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT / 'Game/Content/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.umap', backup / 'L_DBA_PluginDemoHub.umap')
    streaming = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingDynamic, STREAM, False)
    if not isinstance(streaming, unreal.LevelStreamingDynamic) or streaming.get_loaded_level() is None:
        raise RuntimeError('真实 Streaming Level 创建失败')
    streaming.set_editor_property('initially_loaded', False)
    streaming.set_editor_property('initially_visible', False)
    streaming.set_editor_property('disable_distance_streaming', True)
    level = streaming.get_loaded_level()
    unreal.EditorLevelUtils.make_level_current(streaming)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.get_current_level() != level:
        raise RuntimeError('不能设置本次新关卡为当前层')
    try:
        actor = actor_api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(850, 100, 100))
        if actor is None:
            raise RuntimeError('流送关卡可见样板创建失败')
        actor.set_actor_label('DBA_PluginManual_StreamCube')
        actor.set_folder_path('PluginDemos/StreamingSample')
        actor.set_actor_scale3d(unreal.Vector(2, 2, 2))
        component = actor.static_mesh_component
        component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube.Cube'))
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    finally:
        if not level_editor.set_current_level_by_name('L_DBA_PluginDemoHub'):
            raise RuntimeError('不能归还原 Hub 当前层')
    after = {a.get_path_name(): a.get_actor_transform().export_text() for a in actor_api.get_all_level_actors() if isinstance(a, unreal.PlayerStart)}
    assert before == after
    assert actor.get_outer() == level
    report = {'editor_pid': os.getpid(), 'hub': HUB, 'stream': STREAM,
              'player_starts_preserved': before, 'sample_actor': actor.get_path_name(),
              'initially_loaded': streaming.get_editor_property('initially_loaded'),
              'initially_visible': streaming.get_editor_property('initially_visible'),
              'save': 'requires_exact_Monolith_save_of_two_maps', 'cold_reload': 'pending',
              'streaming_runtime': 'pending', 'human_review': 'pending'}
    (OUT / 'HubStreaming.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'stream_created': True, 'initially_loaded': False, 'initially_visible': False,
                      'player_starts_preserved': len(before), 'sample_actor': actor.get_path_name()}))


if __name__ == '__main__':
    author()
