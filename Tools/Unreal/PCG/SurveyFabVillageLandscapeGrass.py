# -*- coding: utf-8 -*-
"""Monolith 只读采样正式新手村的湖、河与地形，供草地水域排除遮罩使用。

运行前必须由操作者安全交接并打开正式地图；不加载地图、不改 Actor、不保存地图。
输出只写工程内 Saved；地图 SHA 与水体变换用于拒绝复用过期采样。
"""
import hashlib
import json
import math
import re
import uuid
from pathlib import Path
import unreal as u

MAP = '/DBAWorldPack_Village/Maps/L_Village_Start'
ROOT = Path(u.Paths.project_dir()).parent
MAP_FILE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
STEP = 150.
MARGIN = 900.


def run():
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().split('.')[0] == MAP, '请先安全交接并打开正式Village地图'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '地图有未保存修改，拒绝采样'
    actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    terrain = [a for a in actors if isinstance(a, u.LandscapeProxy)]
    assert len(terrain) == 1, '正式地形不是唯一Landscape'
    lake = next(a for a in actors if a.get_actor_label() == 'HillsideWater_Lake')
    river = next(a for a in actors if a.get_actor_label() == 'HillsideWater_River')
    lc = lake.get_component_by_class(u.WaterBodyComponent)
    rc = river.get_component_by_class(u.WaterBodyRiverComponent)
    assert lc and rc and not lc.get_editor_property('affects_landscape') and not rc.get_editor_property('affects_landscape')
    ls = lc.get_water_spline(); length = ls.get_spline_length()
    polygon = []
    for i in range(256):
        p = ls.get_location_at_distance_along_spline(length * i / 256, u.SplineCoordinateSpace.WORLD)
        polygon.append([p.x, p.y, p.z])
    water_z = sum(p[2] for p in polygon) / len(polygon)
    assert max(abs(p[2] - water_z) for p in polygon) < .01, '湖面不是水平面'
    rs = rc.get_water_spline(); segments = max(1, math.ceil(rs.get_spline_length() / STEP))
    river_points = []
    for i in range(segments + 1):
        distance = rs.get_spline_length() * i / segments
        p = rs.get_location_at_distance_along_spline(distance, u.SplineCoordinateSpace.WORLD)
        key = rs.get_input_key_value_at_distance_along_spline(distance)
        river_points.append([p.x, p.y, p.z, rc.get_river_width_at_spline_input_key(key)])
    bounds = polygon + [[p[0] + sign * p[3] / 2, p[1] + sign * p[3] / 2, p[2]] for p in river_points for sign in [-1, 1]]
    ox = math.floor((min(p[0] for p in bounds) - MARGIN) / STEP) * STEP
    oy = math.floor((min(p[1] for p in bounds) - MARGIN) / STEP) * STEP
    nx = math.ceil((max(p[0] for p in bounds) + MARGIN - ox) / STEP) + 1
    ny = math.ceil((max(p[1] for p in bounds) + MARGIN - oy) / STEP) + 1
    ignore = [a for a in actors if not isinstance(a, u.LandscapeProxy)]
    heights = []
    for y in range(ny):
        row = []
        for x in range(nx):
            text = u.SystemLibrary.line_trace_single(world, u.Vector(ox+x*STEP, oy+y*STEP, 16000),
                u.Vector(ox+x*STEP, oy+y*STEP, -12000), u.TraceTypeQuery.ECC_VISIBILITY,
                False, ignore, u.DrawDebugTrace.NONE).export_text()
            assert 'LandscapeHeightfieldCollisionComponent' in text and 'bBlockingHit=True' in text
            match = re.search(r'ImpactPoint=\(X=[^,]+,Y=[^,]+,Z=([^\)]+)\)', text)
            assert match, '地形高度解析失败'
            row.append(float(match.group(1)))
        heights.append(row)
    output = ROOT / 'Saved/Validation/FabVillageGrass' / str(uuid.uuid4())
    output.mkdir(parents=True, exist_ok=False)
    snapshot = {'map': MAP, 'mapSha256': hashlib.sha256(MAP_FILE.read_bytes()).hexdigest(),
        'origin': [ox, oy], 'step': STEP, 'nx': nx, 'ny': ny, 'waterZ': water_z,
        'lakePolygon': polygon, 'riverPoints': river_points, 'heights': heights,
        'lakeTransform': lake.get_actor_transform().export_text(),
        'riverTransform': river.get_actor_transform().export_text(),
        'terrainTransform': terrain[0].get_actor_transform().export_text(),
        'playerStarts': [a.get_path_name() for a in actors if isinstance(a, u.PlayerStart)]}
    (output / 'VillageWaterSurvey.json').write_text(json.dumps(snapshot, ensure_ascii=False), encoding='utf-8')
    print('FAB_VILLAGE_WATER_SURVEY_OK', str(output), nx * ny)


run()
