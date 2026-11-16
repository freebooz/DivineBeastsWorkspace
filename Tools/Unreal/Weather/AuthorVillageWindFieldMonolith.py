"""仅在直接Monolith中为正式新手村加入唯一全局风源；不重新生成PCG或保存无关资产。"""
import hashlib
import json
import os
import pathlib
import shutil
import unreal as u
import AuthorUnifiedVegetationWindMonolith as base

ROOT = pathlib.Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT / 'Saved/Validation/VillageGlobalWindField20261025/Native'
TAG = 'GamePlatform.Weather.GlobalWind'
FORMAL = '/DBAWorldPack_Village/Maps/L_Village_Start'

def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')

def world():
    w = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not w or w.get_path_name().split('.')[0] != FORMAL or u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('仅允许正式新手村Editor世界，拒绝PIE或其他地图')
    return w

def values(actor):
    c = actor.get_component_by_class(u.WindDirectionalSourceComponent)
    return {'actor': actor.get_path_name(), 'yaw': actor.get_actor_rotation().yaw,
            'strength': c.get_editor_property('strength'), 'frequency_hz': c.get_editor_property('speed'),
            'gust': c.get_editor_property('max_gust_amount')}

def author():
    w = world()
    if u.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('地图存在未保存改动，拒绝覆盖；先保护当前人工现场')
    if u.GameplayStatics.get_all_actors_of_class(w, u.WindDirectionalSource):
        raise RuntimeError('已有原生风源，先审计，不叠加第二套风场')
    if (OUT / 'Before.json').exists():
        raise RuntimeError('本轮已有基线，拒绝覆盖')
    before = base.scene_snapshot(); write('Before.json', before)
    source = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    backup = OUT / ('L_Village_Start.Before.' + digest[:16] + '.umap')
    shutil.copy2(source, backup)
    write('MapBackup.json', {'source': str(source), 'backup': str(backup), 'sha256': digest})
    starts = sorted(u.GameplayStatics.get_all_actors_of_class(w, u.PlayerStart), key=lambda a: a.get_path_name())
    location = starts[0].get_actor_location() + u.Vector(0, 0, 500) if starts else u.Vector(0, 0, 1500)
    actor = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(
        u.WindDirectionalSource, location, u.Rotator())
    if not actor: raise RuntimeError('原生风源生成失败')
    actor.set_actor_label('Wind_Village_Global_WeatherDriven')
    c = actor.get_component_by_class(u.WindDirectionalSourceComponent)
    c.set_mobility(u.ComponentMobility.MOVABLE)
    c.set_strength(.45); c.set_speed(.15)
    c.set_minimum_gust_amount(0.); c.set_maximum_gust_amount(.25)
    # 最后发布标签，编辑器事件桥才会消费完整参数，不产生半初始化预览。
    actor.set_editor_property('tags', [u.Name(TAG)])
    after = base.scene_snapshot()
    if [row for row in after['actors'] if row['actor'] != actor.get_path_name()] != before['actors']:
        raise RuntimeError('原有Actor/组件/模型/材质/实例改变，停止保存')
    if not u.EditorLoadingAndSavingUtils.save_map(w, FORMAL):
        raise RuntimeError('正式地图原生保存失败，保留现场')
    write('Authored.json', {**values(actor), 'author_pid': os.getpid(), 'existing_scene_preserved': True})
    audit()

def audit(require_cold=False):
    w = world()
    sources = u.GameplayStatics.get_all_actors_of_class(w, u.WindDirectionalSource)
    if len(sources) != 1 or TAG not in [str(t) for t in sources[0].tags]:
        raise RuntimeError('必须有且仅有一个已标记的原生全局风源')
    authored = json.loads((OUT / 'Authored.json').read_text(encoding='utf-8'))
    current = values(sources[0])
    for key in ('yaw', 'strength', 'frequency_hz', 'gust'):
        if abs(current[key] - authored[key]) > .0001: raise RuntimeError('原生风参数不匹配: ' + key)
    before = json.loads((OUT / 'Before.json').read_text(encoding='utf-8'))
    now = base.scene_snapshot()
    if [row for row in now['actors'] if row['actor'] != current['actor']] != before['actors']:
        raise RuntimeError('原有场景或PCG实例变化')
    if require_cold and authored['author_pid'] == os.getpid(): raise RuntimeError('需要独立新Editor冷重载')
    mpc = u.load_asset('/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal')
    vector = u.MaterialLibrary.get_vector_parameter_value(w, mpc, 'GlobalWindTo')
    actual = [vector.r, vector.g, vector.b, vector.a]
    expected = [current['yaw'], current['strength'], current['gust'], current['frequency_hz']]
    if any(abs(a-b) > .0001 for a,b in zip(actual, expected)):
        raise RuntimeError('Editor原生风与植物MPC不一致: ' + str(actual))
    report = {'status': 'PASS', 'source': current, 'editor_mpc_to': actual, 'editor_pid': os.getpid(),
              'cold_reload': require_cold, 'existing_scene_preserved': True,
              'packaged_visual_accepted': False}
    write('Audit_Cold.json' if require_cold else 'Audit_Author.json', report)
    print(json.dumps(report, ensure_ascii=False))
    return report
