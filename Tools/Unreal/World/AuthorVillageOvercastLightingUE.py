"""通过直接Monolith调整正式新手村柔和阴天光照；不改天气类型、材质、PCG或出生点。"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT / 'Saved/Validation/VillageOvercastLighting20261025'
FORMAL = '/DBAWorldPack_Village/Maps/L_Village_Start'
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import AuthorUnifiedVegetationWindMonolith as base

def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

def world():
    s = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    w = s.get_editor_world()
    if not w or w.get_path_name().split('.')[0] != FORMAL or s.get_game_world():
        raise RuntimeError('仅允许正式新手村Editor世界，无PIE')
    return w

def lights():
    w = world()
    sun = u.GameplayStatics.get_all_actors_of_class(w, u.DirectionalLight)
    sky = u.GameplayStatics.get_all_actors_of_class(w, u.SkyLight)
    if len(sun) != 1 or len(sky) != 1: raise RuntimeError('灯光不唯一，拒绝猜测目标')
    return sun[0].get_component_by_class(u.DirectionalLightComponent), sky[0].get_component_by_class(u.SkyLightComponent)

def state():
    sun, sky = lights()
    def color(c):
        v = c.get_editor_property('light_color')
        return [v.r, v.g, v.b, v.a]
    return {'sun': sun.get_path_name(), 'sky': sky.get_path_name(),
            'sun_intensity': sun.get_editor_property('intensity'),
            'sky_intensity': sky.get_editor_property('intensity'),
            'sun_angle': sun.get_editor_property('light_source_angle'),
            'sun_color': color(sun), 'sky_color': color(sky),
            'sky_realtime': sky.get_editor_property('real_time_capture')}

def prepare():
    world()
    if u.EditorLoadingAndSavingUtils.get_dirty_map_packages(): raise RuntimeError('保护已有未保存地图改动')
    if (OUT / 'Before.json').exists(): raise RuntimeError('已有基线，拒绝覆盖')
    source = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / 'L_Village_Start.BeforeOvercast.umap'
    shutil.copy2(source, backup)
    write('Before.json', {'scene': base.scene_snapshot(), 'lights': state(), 'author_pid': os.getpid(),
                          'map_backup': str(backup), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    print(json.dumps(state()))

def stage(sun_intensity=2.5, sky_intensity=2.5, angle=4.):
    before = json.loads((OUT / 'Before.json').read_text())
    sun, sky = lights()
    # 减小直射/环境光比例并扩大光源角；保留固定曝光、灯的方向、雾以及所有材质原RGB。
    sun.set_intensity(sun_intensity); sun.set_editor_property('light_source_angle', angle)
    sun.set_editor_property('light_color', u.Color(255, 255, 255, 255))
    sky.set_intensity(sky_intensity)
    if base.scene_snapshot() != before['scene']: raise RuntimeError('场景对象/实例/位置/材质出现非授权变化')
    write('Candidate.json', state())
    print(json.dumps(state()))

def save():
    before = json.loads((OUT / 'Before.json').read_text())
    if base.scene_snapshot() != before['scene']: raise RuntimeError('原有场景改变，停止保存')
    current = state()
    if not u.EditorLoadingAndSavingUtils.save_map(world(), FORMAL): raise RuntimeError('原生保存失败')
    write('Authored.json', {'lights': current, 'author_pid': os.getpid()})
    audit()

def audit(cold=False):
    before = json.loads((OUT / 'Before.json').read_text())
    authored = json.loads((OUT / 'Authored.json').read_text())
    if base.scene_snapshot() != before['scene']: raise RuntimeError('原有场景/PCG/出生点变化')
    if state() != authored['lights']: raise RuntimeError('重载灯光参数不一致')
    if cold and os.getpid() == authored['author_pid']: raise RuntimeError('需要独立Editor冷重载')
    w = world()
    if u.GameplayStatics.get_all_actors_of_class(w, u.WindDirectionalSource): raise RuntimeError('不应新增地图风源')
    report = {'status': 'PASS', 'cold_reload': cold, 'lights': state(), 'fixed_exposure_preserved': True,
              'original_scene_preserved': True, 'editor_pid': os.getpid(), 'human_visual_accepted': False}
    write('Audit_Cold.json' if cold else 'Audit_Author.json', report)
    print(json.dumps(report))
