# -*- coding: utf-8 -*-
"""经Monolith在正式新手村用原生WaterBodyLake替换湖体，保留Actor身份与水域轮廓。"""
import hashlib
import json
import re
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
EVIDENCE = ROOT / 'Saved/Validation/VillageNativeLake20261029'
MAP_FILE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
MAP = '/DBAWorldPack_Village/Maps/L_Village_Start'


def vec(v):
    return [v.x, v.y, v.z]


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run():
    baseline = json.loads((EVIDENCE / 'Before/Actor.json').read_text(encoding='utf-8'))
    assert sha(MAP_FILE) == sha(EVIDENCE / 'Before/L_Village_Start.umap'), '地图已变更，拒绝覆盖'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().split('.')[0] == MAP
    subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    candidates = [a for a in actors if a.get_actor_label() == 'HillsideWater_Lake']
    assert len(candidates) == 1
    old = candidates[0]
    assert old.get_path_name() == baseline['actor'] and old.get_class().get_path_name() == baseline['class']
    assert isinstance(old, u.WaterBodyLake) and old.get_class().get_path_name() != '/Script/Water.WaterBodyLake'
    guid = old.get_editor_property('actor_guid').export_text()
    old_transform = old.get_actor_transform().export_text()
    c = old.get_component_by_class(u.WaterBodyComponent)
    spline = c.get_water_spline()
    assert not c.get_editor_property('affects_landscape') and spline.is_closed_loop()
    assert spline.get_number_of_spline_points() == len(baseline['points']) == 24
    material, static_material = c.get_water_material(), c.get_editor_property('water_static_mesh_material')
    info, zone = c.get_editor_property('water_info_material'), c.get_editor_property('water_zone_override')
    settings = c.get_editor_property('static_mesh_settings')
    depth = float(c.get_editor_property('fixed_water_depth'))
    folder, tags = old.get_folder_path(), list(old.get_editor_property('tags'))
    points = [(spline.get_location_at_spline_point(i, u.SplineCoordinateSpace.WORLD),
               spline.get_scale_at_spline_point(i), spline.get_spline_point_type(i))
              for i in range(spline.get_number_of_spline_points())]
    assert all(t == u.SplinePointType.CURVE_CLAMPED for _, _, t in points)
    starts = {a.get_path_name(): a.get_actor_transform().export_text() for a in actors if isinstance(a, u.PlayerStart)}
    assert starts == baseline['playerStarts']

    # 编辑器桥接引擎ReplaceActors以保持Actor GUID和引用；延迟构造时先关闭雕刻，不修改全局CDO。
    replacement, error = u.GamePlatformWaterEditorLibrary.replace_with_native_no_carve_lake(old)
    assert replacement and not str(error), str(error)
    matches = [a for a in subsystem.get_all_level_actors() if a.get_path_name() == baseline['actor']]
    assert len(matches) == 1, '原生替换失败，保留脏地图供检查，禁止保存'
    new = matches[0]
    assert new.get_class().get_path_name() == '/Script/Water.WaterBodyLake'
    assert new.get_editor_property('actor_guid').export_text() == guid, 'Actor GUID未保留'
    assert new.get_actor_transform().export_text() == old_transform
    new.set_actor_label('HillsideWater_Lake')
    new.set_folder_path(folder)
    new.set_editor_property('tags', tags)
    component = new.get_component_by_class(u.WaterBodyComponent)
    assert not component.get_editor_property('affects_landscape'), '原生Actor创建时不能雕刻地形'
    component.set_water_zone_override(zone)
    component.set_water_material(material)
    component.set_water_static_mesh_material(static_material)
    component.set_water_info_material(info)
    component.set_editor_property('static_mesh_settings', settings)
    component.set_editor_property('fixed_water_depth', depth)
    component.set_editor_property('can_ever_affect_navigation', False)
    component.set_editor_property('generate_overlap_events', False)
    component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    new.set_water_waves(None)
    target = component.get_water_spline()
    target.clear_spline_points(False)
    for location, _, _ in points:
        target.add_spline_point(location, u.SplineCoordinateSpace.WORLD, False)
    target.set_closed_loop(True, False)
    for i, (_, scale, point_type) in enumerate(points):
        target.set_scale_at_spline_point(i, scale, False)
        target.set_spline_point_type(i, point_type, False)
    target.update_spline()
    target.k2_synchronize_and_broadcast_data_change()
    component.set_water_body_static_mesh_enabled(False)
    component.set_water_body_static_mesh_enabled(True)
    for primitive in new.get_components_by_class(u.PrimitiveComponent):
        primitive.set_editor_property('can_ever_affect_navigation', False)
        primitive.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    assert component.get_water_material() == material and component.get_editor_property('water_zone_override') == zone
    assert component.get_editor_property('static_mesh_settings').export_text() == baseline['static_settings']
    assert abs(target.get_spline_length() - baseline['spline_length']) < .1
    max_error = 0.
    for i, expected in enumerate(baseline['samples']):
        actual = target.get_location_at_distance_along_spline(target.get_spline_length() * i / 256, u.SplineCoordinateSpace.WORLD)
        max_error = max(max_error, sum((a-b)**2 for a,b in zip(vec(actual), expected)) ** .5)
    assert max_error < .1, '湖岸样条变化超过0.1厘米'
    actors = subsystem.get_all_level_actors()
    assert {a.get_path_name(): a.get_actor_transform().export_text() for a in actors if isinstance(a, u.PlayerStart)} == starts
    survey_path = ROOT / 'Saved/Validation/FabVillageGrass/6d678a97-2cfb-448b-85c6-4e667813127a/VillageWaterSurvey.json'
    survey = json.loads(survey_path.read_text(encoding='utf-8'))
    assert survey['mapSha256'] == sha(EVIDENCE / 'Before/L_Village_Start.umap')
    river = next(a for a in actors if a.get_actor_label() == 'HillsideWater_River')
    river_component = river.get_component_by_class(u.WaterBodyRiverComponent)
    river_spline = river_component.get_water_spline()
    for i, previous in enumerate(survey['riverPoints']):
        key = river_spline.get_spline_length() * i / (len(survey['riverPoints']) - 1)
        actual = river_spline.get_location_at_distance_along_spline(key, u.SplineCoordinateSpace.WORLD)
        assert sum((a-b)**2 for a,b in zip(vec(actual), previous[:3])) ** .5 < .1, '河流样条变化'
    landscapes = [a for a in actors if isinstance(a, u.LandscapeProxy)]
    assert len(landscapes) == 1
    ignore = [a for a in actors if not isinstance(a, u.LandscapeProxy)]
    max_terrain_error = 0.
    for y, row in enumerate(survey['heights']):
        for x, previous in enumerate(row):
            px = survey['origin'][0] + x * survey['step']
            py = survey['origin'][1] + y * survey['step']
            hit = u.SystemLibrary.line_trace_single(world, u.Vector(px, py, 16000), u.Vector(px, py, -12000),
                u.TraceTypeQuery.ECC_VISIBILITY, False, ignore, u.DrawDebugTrace.NONE).export_text()
            assert 'LandscapeHeightfieldCollisionComponent' in hit and 'bBlockingHit=True' in hit
            match = re.search(r'ImpactPoint=\(X=[^,]+,Y=[^,]+,Z=([^\)]+)\)', hit)
            assert match
            max_terrain_error = max(max_terrain_error, abs(float(match.group(1)) - previous))
    assert max_terrain_error < .1, '地形高度发生变化，拒绝保存'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '出现非地图脏资产，拒绝保存'
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level(), '地图保存失败'
    result = {'class': new.get_class().get_path_name(), 'actor': new.get_path_name(), 'guid': guid,
              'points': len(points), 'maxSplineErrorCm': max_error, 'playerStartsUnchanged': True,
              'terrainSampleCount': survey['nx'] * survey['ny'], 'maxTerrainErrorCm': max_terrain_error,
              'riverSplineUnchanged': True,
              'mapSha256': sha(MAP_FILE), 'affectsLandscape': False, 'material': material.get_path_name(),
              'zone': zone.get_path_name(), 'nativeReplaceReferences': True}
    (EVIDENCE / 'NativeReplacement.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('NATIVE_LAKE_REPLACED', json.dumps(result, ensure_ascii=False))


run()
