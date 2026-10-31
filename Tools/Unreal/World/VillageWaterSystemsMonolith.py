"""SSLM 水域原生作者/核验入口。仅经正式工程 Monolith 执行，输出留在工程内。"""
import hashlib
import json
import os
import shutil
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT / 'Saved/Validation/VillageWaterSystems20261031'
VILLAGE = '/DBAWorldPack_Village/Maps/L_Village_Start'
REVIEW = '/Game/Development/Water/L_GP_WaterReview'


def prop(obj, name):
    try:
        value = obj.get_editor_property(name)
        if isinstance(value, u.Object):
            return value.get_path_name()
        if hasattr(value, 'export_text'):
            return value.export_text()
        return str(value)
    except Exception:
        return None


def world_actors():
    assert str(u.Paths.get_project_file_path()).replace('\\', '/').endswith('/Game/DivineBeastsArena.uproject')
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().split('.')[0] in (VILLAGE, REVIEW)
    return world, u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()


def inspect_world(label='Before'):
    world, actors = world_actors()
    rows = []
    for actor in actors:
        if not isinstance(actor, (u.WaterBody, u.GamePlatformWaterRegion, u.PhysicsVolume, u.PlayerStart)):
            continue
        row = {'actor': actor.get_path_name(), 'label': actor.get_actor_label(),
               'class': actor.get_class().get_path_name(), 'transform': actor.get_actor_transform().export_text()}
        if isinstance(actor, u.WaterBody):
            c = actor.get_water_body_component()
            row['component'] = c.get_path_name()
            row['properties'] = {p: prop(c, p) for p in (
                'affects_landscape', 'generate_collisions', 'collision_profile_name',
                'underwater_post_process_material', 'underwater_post_process_settings',
                'water_material', 'water_static_mesh_material', 'fixed_water_depth',
                'collision_height_offset', 'water_body_settings')}
            row['primitives'] = [{'name': p.get_name(), 'class': p.get_class().get_path_name(),
                                  'collision': str(p.get_collision_enabled()),
                                  'profile': str(p.get_collision_profile_name()),
                                  'bounds': str(p.get_actor_bounds(False)) if hasattr(p, 'get_actor_bounds') else None}
                                 for p in actor.get_components_by_class(u.PrimitiveComponent)]
            spline = c.get_water_spline()
            row['spline'] = [spline.get_location_at_distance_along_spline(
                spline.get_spline_length()*i/256, u.SplineCoordinateSpace.WORLD).to_tuple() for i in range(256)]
        if isinstance(actor, u.GamePlatformWaterRegion):
            row['region'] = prop(actor, 'region')
            row['physics_volume'] = prop(actor, 'water_physics_volume')
            row['requires_volume'] = prop(actor, 'require_physics_volume')
        if isinstance(actor, u.PhysicsVolume):
            row['water_volume'] = prop(actor, 'water_volume')
            row['friction'] = prop(actor, 'fluid_friction')
        rows.append(row)
    registry = u.AssetRegistryHelpers.get_asset_registry()
    animation_assets = registry.get_assets_by_class(u.TopLevelAssetPath('/Script/Engine', 'AnimSequence'), True)
    swims = [str(a.package_name) for a in animation_assets if any(x in str(a.asset_name).lower() for x in ('swim', 'tread', 'dive'))]
    report = {'pid': os.getpid(), 'map': world.get_path_name(), 'actors': rows,
              'animation_count': len(animation_assets), 'swim_animation_candidates': swims}
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / (world.get_name() + '.' + label + '.json')
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('WATER_INSPECT', json.dumps({'path': str(path), 'rows': len(rows), 'animations': len(animation_assets), 'swims': swims}, ensure_ascii=False))
    return report


def verify_world():
    """真实基线必须先失败；资产作者完成后在新进程用同一合同重新核验。"""
    world, actors = world_actors()
    volumes = [a for a in actors if isinstance(a, u.PhysicsVolume) and a.get_editor_property('water_volume')]
    assert volumes, '缺少双端原生Water PhysicsVolume，只有WaterBodyLake不能形成原生游泳'
    regions = [a for a in actors if isinstance(a, u.GamePlatformWaterRegion)]
    exact = [r for r in regions if r.get_editor_property('require_physics_volume')]
    assert exact, '湖体尚未接入精确权威体积'
    for r in exact:
        v = r.get_editor_property('water_physics_volume')
        assert v in volumes and r.is_water_region_valid(), '失效精确水域不能退回宽Box'
    lakes = [a for a in actors if a.get_class().get_path_name() == '/Script/Water.WaterBodyLake']
    assert lakes, '缺少原生WaterBodyLake'
    for a in lakes:
        c = a.get_water_body_component()
        assert not c.get_editor_property('affects_landscape'), '不得雕刻地形'
        query = [p for p in a.get_components_by_class(u.PrimitiveComponent)
                 if 'LakeCollision' in p.get_class().get_name() and p.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION]
        assert query, '原生水下检测缺少湖体查询碰撞'
        assert c.get_editor_property('underwater_post_process_material'), '缺少真实水下后处理材质'
    print('WATER_NATIVE_VERIFY_PASSED', world.get_path_name())


def package_file(package):
    """本次明确所有者的原生包；不在工程外写副本。"""
    mounts = {
        'Game': ROOT/'Game/Content',
        'DBAWorldPack_Village': ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content',
        'GamePlatformSurface': ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content',
        'DBAContentPack_Common': ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content',
    }
    mount, relative = package.strip('/').split('/', 1)
    base = mounts[mount]/relative
    return base.with_suffix('.umap') if base.with_suffix('.umap').exists() else base.with_suffix('.uasset')


def backup(package):
    source = package_file(package)
    if source.exists():
        target = OUT/'Backups'/source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(source, target)
        return hashlib.sha256(source.read_bytes()).hexdigest()


def author_geometry(underwater_material):
    """调用唯一原生作者建立同源体积；调用前材质必须已真实创建、保存。"""
    world, actors = world_actors()
    package = world.get_path_name().split('.')[0]
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '有未保存地图，拒绝覆盖'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '有未保存内容，拒绝覆盖'
    before = inspect_world('GeometryBefore')
    backup(package)
    lakes = [a for a in actors if a.get_class().get_path_name() == '/Script/Water.WaterBodyLake']
    assert len(lakes) == 1
    lake = lakes[0]
    component = lake.get_water_body_component()
    material = u.load_asset(underwater_material)
    assert isinstance(material, u.MaterialInterface)
    region_id = 'Village.Lake' if package == VILLAGE else 'Review.Lake.NativeSwim'
    existing = [a for a in actors if isinstance(a, u.GamePlatformWaterRegion) and str(a.get_editor_property('region').region_id) == region_id]
    assert len(existing) <= 1
    region = existing[0] if existing else u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.GamePlatformWaterRegion, lake.get_actor_location())
    config = region.get_editor_property('region')
    config.set_editor_property('region_id', region_id)
    config.set_editor_property('kind', u.GamePlatformWaterKind.LAKE)
    config.set_editor_property('priority', 30)
    config.set_editor_property('flow_speed_cm_per_second', 0)
    config.set_editor_property('swimmable', True)
    region.set_editor_property('region', config)
    region.set_actor_label(region_id + '_Authority')
    region.set_folder_path('Water/Authority')
    depth = 2500.0 if package == VILLAGE else 1000.0
    assert not region.get_editor_property('water_physics_volume'), '已有体积不得重复烘焙'
    volume, error = u.GamePlatformWaterEditorLibrary.bake_lake_physics_volume(lake, region, depth)
    assert volume and not str(error), str(error)
    volume.set_folder_path('Water/Authority')
    curves = component.get_editor_property('curve_settings')
    curves.set_editor_property('channel_depth', depth)
    component.set_editor_property('curve_settings', curves)
    component.set_editor_property('fixed_water_depth', depth)
    # Python将bool成功返回压缩为OutError（成功为空字符串，失败为None）。
    error = u.GamePlatformWaterEditorLibrary.configure_lake_underwater(lake, material)
    assert error is not None and not str(error), str(error)
    finish_geometry()


def finish_geometry():
    """恢复已完成原生调用但脚本随后中断的本轮事务；不重新生成体积。"""
    world, actors = world_actors()
    package = world.get_path_name().split('.')[0]
    assert {p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()} <= {package}
    before = json.loads((OUT/(world.get_name()+'.GeometryBefore.json')).read_text(encoding='utf-8'))
    after = inspect_world('GeometryAuthored')
    old = {a['actor']: a for a in before['actors']}
    spline_errors = {}
    for a in after['actors']:
        if a['actor'] in old and 'spline' in old[a['actor']]:
            expected=old[a['actor']]['spline']
            assert len(a['spline'])==len(expected)
            error=max(sum((x-y)**2 for x,y in zip(p,q))**.5 for p,q in zip(a['spline'],expected))
            spline_errors[a['label']]=error
            assert error < .0001, '湖岸或河流样条变化：'+str(error)+'cm'
        if a['class'] == '/Script/Engine.PlayerStart':
            assert a['transform'] == old[a['actor']]['transform'], '人工出生点变化'
    verify_world()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    (OUT/(world.get_name()+'.GeometrySaved.json')).write_text(json.dumps({'pid':os.getpid(),'map':package,'spline_errors_cm':spline_errors,'player_starts_unchanged':True,'map_sha256':backup_hash(package)},ensure_ascii=False,indent=2),encoding='utf-8')
    print('WATER_GEOMETRY_SAVED', json.dumps({'map': package, 'map_sha256': backup_hash(package)}, ensure_ascii=False))


def backup_hash(package):
    return hashlib.sha256(package_file(package).read_bytes()).hexdigest()
