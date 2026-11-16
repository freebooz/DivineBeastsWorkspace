"""直接 Monolith 的有限步骤 PIE 页路由验收；外部驱动逐步调用，不向 UI 添加轮询。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027/Pages.Live.json'
MANIFEST = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
PAGES = sorted(MANIFEST['plugins'], key=lambda p: p['plugin_id'])
STATE = {'editor_pid': os.getpid(), 'started_epoch': time.time(), 'host': 'Editor PIE native menu route',
         'results': [], 'pending': None, 'status': 'running', 'human_review': 'pending', 'packaged_client_verified': False}


def persist():
    OUT.write_text(json.dumps(STATE, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def host():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world is None:
        raise RuntimeError('需要本任务真实 PIE World')
    gi = unreal.GameplayStatics.get_game_instance(world)
    catalog = next(x for x in unreal.ObjectIterator(unreal.DivineBeastsDemoCatalogSubsystem) if x.get_outer() == gi)
    screens = [s for s in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.DivineBeastsDemoHubScreen, False) if s.is_activated()]
    return catalog, screens


def request_next():
    if STATE['pending'] is not None:
        raise RuntimeError('上一路由尚未核对')
    index = len(STATE['results'])
    if index == len(PAGES):
        STATE['status'] = 'native_routes_and_desired_size_passed'
        STATE['finished_epoch'] = time.time()
        persist()
        return {'complete': True, 'count': index}
    target = PAGES[index]['plugin_id']
    catalog, _ = host()
    if not catalog.open_plugin_page(target):
        STATE['status'] = 'failed'
        persist()
        raise RuntimeError('真实目录拒绝页面: ' + target)
    STATE['pending'] = {'plugin_id': target, 'requested_epoch': time.time(), 'probe_count': 0}
    persist()
    return {'requested': target, 'index': index}


def inspect():
    pending = STATE['pending']
    if pending is None:
        raise RuntimeError('没有本次待验路由')
    pending['probe_count'] += 1
    target = pending['plugin_id']
    _, screens = host()
    current = [s for s in screens if str(s.get_view_model().get_fixed_plugin_id()) == target]
    if not current:
        if time.time() - pending['requested_epoch'] > 30 or pending['probe_count'] > 30:
            STATE['status'] = 'failed_timeout'
            persist()
            raise RuntimeError('真实页面在有界期限内未激活: ' + target)
        return {'ready': False, 'target': target}
    if len(screens) != 1 or len(current) != 1:
        raise RuntimeError('存在多个活动页')
    screen = current[0]
    vm = screen.get_view_model()
    snapshot = vm.get_menu_snapshot()
    assert str(snapshot.selected_plugin_id) == target
    assert len(snapshot.plugins) == 1 and str(snapshot.plugins[0].plugin_id) == target
    expected = '/DBAUIPack_Core/Development/PluginDemos/UI/PluginTests/WBP_DBA_PluginTest_' + target
    # 类路径由唯一作者源提供；禁止根据展示标题认定身份。
    entry = PAGES[len(STATE['results'])]
    expected = entry['test_page_asset']
    actual = screen.get_class().get_path_name()
    if expected.endswith('_C'):
        assert actual == expected, (actual, expected)
    else:
        short = expected.rsplit('/', 1)[-1].split('.')[0]
        expected_class = expected if '.' in expected else expected + '.' + short
        assert actual == expected_class + '_C', (actual, expected_class)
    screen.force_layout_prepass()
    size = screen.get_desired_size()
    if size.x <= 0 or size.y <= 0:
        return {'ready': False, 'target': target, 'layout_pending': True}
    foreign = PAGES[0]['plugin_id'] if target != PAGES[0]['plugin_id'] else PAGES[1]['plugin_id']
    assert not vm.select_plugin(foreign)
    assert str(vm.get_fixed_plugin_id()) == target
    result = {'plugin_id': target, 'actual_class': actual, 'active_pages': len(screens),
              'selected_plugin': str(snapshot.selected_plugin_id), 'features': len(snapshot.features),
              'desired_size_after_native_prepass': [size.x, size.y], 'cross_plugin_selection_rejected': True,
              'probe_count': pending['probe_count'], 'human_review': 'pending'}
    STATE['results'].append(result)
    STATE['pending'] = None
    persist()
    return {'ready': True, 'verified': target, 'count': len(STATE['results'])}


persist()
