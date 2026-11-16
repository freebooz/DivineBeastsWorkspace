"""直接 Monolith 创建七项开发夹具；不启动服务、不授予角色资格、不写生产地图。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
BASE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/'
MAP = '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'
KINDS = {
    'Region': unreal.GamePlatformRegionDefinition,
    'World': unreal.GamePlatformWorldDefinition,
    'Weather': unreal.GamePlatformWeatherPresetDefinition,
    'Water': unreal.GamePlatformWaterPresentationPreset,
    'NavAgent': unreal.GamePlatformNavigationAgentProfile,
    'Review': unreal.GamePlatformReviewCase,
    'Performance': unreal.GamePlatformPerformanceProfileAsset,
}


def asset_id(value):
    result = unreal.PrimaryAssetId()
    expected = 'GamePlatformDefinition:' + value
    if not result.import_text('"' + expected + '"') or result.export_text() != expected:
        raise RuntimeError('真实 PrimaryAssetId 导入失败')
    return result


def identity(asset, namespace, name):
    asset.set_editor_property('logical_id', unreal.GamePlatformId(namespace=namespace, name=name, logical_version=1))
    asset.set_editor_property('data_version', unreal.GamePlatformDataVersion(schema_version=1, content_revision=1))
    asset.set_editor_property('required_definitions', [])


def author():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != ROOT / 'Game/DivineBeastsArena.uproject':
        raise RuntimeError('只允许当前正式工程')
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError('先停止本任务 PIE')
    paths = {name: BASE + 'DA_DBA_Manual' + name for name in KINDS}
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:
        raise RuntimeError('存在其它脏包，保留并停止')
    for name, path in paths.items():
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError('首次创建入口拒绝覆盖已占用包: ' + path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = {}
    for name, klass in KINDS.items():
        path = paths[name]
        asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if asset is None:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', klass)
            folder, _, short = path.rpartition('/')
            asset = tools.create_asset(short, folder, klass, factory)
        if not isinstance(asset, klass):
            raise RuntimeError('原生资产类型失败: ' + name)
        created[name] = asset
    region = created['Region']
    identity(region, 'world.demo', 'region')
    region.set_editor_property('region_type_tag', 'Manual.World.Region')
    world = created['World']
    identity(world, 'world.demo', 'hub')
    world.set_editor_property('map_identity', unreal.load_asset(MAP))
    world.set_editor_property('regions', [asset_id('world.demo.region@1')])
    world.set_editor_property('required_definitions', [asset_id('world.demo.region@1')])
    world.set_editor_property('readiness_timeout_seconds', 3600.0)
    weather = created['Weather']
    identity(weather, 'weather.demo', 'rain')
    weather.set_editor_property('weather', unreal.GamePlatformWeatherScheduleEntry(
        state=unreal.GamePlatformWeatherState(**{'type': unreal.GamePlatformWeatherType.LIGHT_RAIN,
            'rain_intensity': 0.4, 'wetness': 0.4, 'puddle_amount': 0.2, 'fog_intensity': 0.1}),
        min_hold_seconds=3.0, max_hold_seconds=3.0, transition_seconds=1.0, weight=1))
    water = created['Water']
    for field, value in [('ripple_semantic_tag', 'Presentation.Water.Ripple.VFX'),
                         ('splash_semantic_tag', 'Presentation.Water.Splash.VFX'),
                         ('max_ripple_spawns_per_second', 18), ('max_view_distance_cm', 6000.0),
                         ('minimal_time_between_same_point_seconds', 0.13), ('enable_splash', True)]:
        water.set_editor_property(field, value)
    nav = created['NavAgent']
    for field, value in [('profile_id', 'Manual.Humanoid'), ('agent_radius', 35.0), ('agent_height', 144.0),
                         ('step_height', 45.0), ('max_slope', 44.0), ('default_filter_id', 'Navigation.Filter.Default'),
                         ('invoker_policy', unreal.GamePlatformNavigationInvokerPolicy.REGISTER_WHEN_ACTIVE),
                         ('tile_generation_radius', 3000.0), ('tile_removal_radius', 5000.0), ('version', 1)]:
        nav.set_editor_property(field, value)
    review = created['Review']
    for field, value in [('case_id', 'Manual.World.Pending'), ('category', 'Manual.World'), ('case_version', 1),
                         ('feature_id', 'GamePlatformDeveloperTools.human_review'),
                         ('scenario_id', 'GamePlatformDeveloperTools.human_review.Manual'),
                         ('description', '检查原生人工审核合同、待审报告与真实证据校验；不自动签收。'),
                         ('preconditions', ['显式开发模式；已加载此测试用例。']),
                         ('steps', ['创建待审报告并核对 NotRun。', '无真实运行与二进制索引时执行证据校验，应明确拒绝。', '停止案例，重新进入不继承旧报告。']),
                         ('expected_result', '报告保持待审；缺证据明确失败；未执行人工批准。'),
                         ('required_evidence', ['state_dump', 'screenshot', 'log'])]:
        review.set_editor_property(field, value)
    performance = created['Performance']
    profile = unreal.GamePlatformPerformanceProfile()
    for field, value in [('scenario_id', 'Manual.World.Bounded'), ('map', unreal.SoftObjectPath(MAP + '.L_DBA_PluginDemoHub')),
                         ('player_count', 1), ('bot_count', 0), ('duration_seconds', 5.0),
                         ('warmup_seconds', 1.0), ('metrics', ['GameThreadMs'])]:
        profile.set_editor_property(field, value)
    performance.set_editor_property('profile', profile)
    for asset in created.values():
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('真实保存失败: ' + asset.get_path_name())
    settings = unreal.ValidateAssetsSettings(skip_excluded_directories=False, show_if_no_failures=False,
        collect_per_asset_details=True, validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures, results = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(a.get_path_name()) for a in created.values()], settings)
    counts = {k: int(v) for k, v in results.to_dict().items() if k.startswith('num_') and isinstance(v, (int, bool))}
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(),
              'assets': {name: a.get_path_name() for name, a in created.items()}, 'native_validation': counts,
              'validation_failures': failures, 'cold_reload': 'pending', 'runtime': 'pending', 'human_review': 'pending',
              'limitations': ['World 初始化、区域宿主与导航数据仍需真实开发宿主。',
                              '天气与水体预设数值不等于可见输出。', '性能场景 Executor 尚未注册。']}
    (OUT / 'T05.World.Fixtures.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'created_saved': len(created), 'native_validation': counts, 'failures': failures}))
    if failures or counts.get('num_invalid', 0):
        raise RuntimeError('原生验证有失败；保留报告与资产待定位')


if __name__ == '__main__':
    author()
