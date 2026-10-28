"""经直接Monolith run_python调用的原生页作者。

复用已审核Hub完整UMG树、共享子页及主题，只写固定插件默认值。
页面定义复用已有Hub合同，由WorldUI在本次请求内产生瞬态副本，不另造70份无人消费的定义。
离线计划阶段不导入UE；原生阶段检查工程、反射合同和脏包，保存前备份既有文件。
不触发C++构建、切图、PIE、网络、人工签收或烘焙。
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import time
from datetime import datetime

WORKSPACE = Path(__file__).resolve().parents[3]
AUTHOR = WORKSPACE / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json'
ASSET_ROOT = '/DBAUIPack_Core/Development/PluginDemos'
HUB = ASSET_ROOT + '/UI/WBP_DBA_PluginDemoHub'
HUB_DEFINITION = ASSET_ROOT + '/Definitions/DA_DBA_PluginDemoHubScreen'
EVIDENCE = WORKSPACE / 'Saved/Validation/PluginManualUI20261027'


def is_cold_reload(author_pid: int, saved_epoch: float, reader_pid: int, reader_started_epoch: float) -> bool:
    """另一个先前已启动的Editor也可能缓存旧资产，不能只比较PID。"""
    return author_pid != reader_pid and reader_started_epoch > saved_epoch > 0


def current_process_started_epoch() -> float:
    """读取Windows当前进程创建时间，既不启动进程也不访问其他进程。"""
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.GetProcessTimes.restype = wintypes.BOOL
    created, exited, system, user = (wintypes.FILETIME() for _ in range(4))
    if not kernel.GetProcessTimes(kernel.GetCurrentProcess(), ctypes.byref(created), ctypes.byref(exited), ctypes.byref(system), ctypes.byref(user)):
        raise ctypes.WinError(ctypes.get_last_error())
    return ((created.dwHighDateTime << 32) | created.dwLowDateTime) / 10000000.0 - 11644473600.0


def make_page_plan(document: dict, selected: list[str] | None = None) -> list[dict]:
    """封闭的PluginId到原生包/类映射；失败发生在引擎导入和任何资产写入之前。"""
    if document.get('manual_page_contract_version') != 1:
        raise ValueError('manual_page_contract_version must be 1')
    plugins = document.get('plugins', [])
    identities = [p['plugin_id'] for p in plugins]
    if len(identities) != len(set(identities)):
        raise ValueError('duplicate plugin identity')
    if selected is not None and (len(selected) != len(set(selected)) or set(selected) - set(identities)):
        raise ValueError('duplicate or unknown selected plugin identity')
    result = []
    for plugin in plugins:
        identity = plugin['plugin_id']
        expected = ASSET_ROOT + '/UI/Plugins/WBP_DBA_PluginTest_' + identity
        if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', identity) or plugin.get('test_page_asset') != expected:
            raise ValueError('page identity must be bound to its plugin: ' + identity)
        if selected is not None and identity not in selected:
            continue
        name = expected.rsplit('/', 1)[-1]
        result.append({'plugin_id': identity, 'fixed_plugin_id': identity, 'widget': expected,
                       'widget_class': expected + '.' + name + '_C',
                       'definition_template': HUB_DEFINITION})
    return sorted(result, key=lambda p: p['plugin_id'])


def authoring_result(pages: list[dict], fingerprint: str) -> dict:
    """作者结果不能提升成冷重载、实际运行或人工签收。"""
    return {'source_fingerprint': fingerprint, 'pages': pages, 'saved_page_count': len(pages),
            'cold_reload': 'not_run', 'runtime': 'not_run', 'human_review': 'pending'}


def checked_source(manifest_path: str = str(AUTHOR)) -> tuple[dict, str]:
    """复用唯一作者源验证器，不接受工程外输入或重复人工清单。"""
    source = Path(manifest_path).resolve()
    if not source.is_relative_to(WORKSPACE):
        raise ValueError('manifest must stay in current workspace')
    raw = source.read_bytes()
    document = json.loads(raw.decode('utf-8-sig'))
    spec = importlib.util.spec_from_file_location('plugin_page_manifest_validator', WORKSPACE / 'Tests/Architecture/ValidatePluginDemoManifest.py')
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)
    issues = validator.validate_manifest(document, WORKSPACE)
    if issues:
        raise ValueError(json.dumps(issues, ensure_ascii=False))
    return document, hashlib.sha256(raw).hexdigest()


def check_editor(unreal):
    """不以类名存在代替新增反射合同已加载。"""
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != (WORKSPACE / 'Game/DivineBeastsArena.uproject').resolve():
        raise RuntimeError('wrong editor project')
    if not str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'):
        raise RuntimeError('requires locked UE5.8')
    native = getattr(unreal, 'DivineBeastsDemoHubScreen', None)
    if native is None or 'fixed_plugin_id' not in dir(unreal.get_default_object(native)):
        raise RuntimeError('BlockedByEditorBinary: fixed_plugin_id reflection is not loaded')


def read_page(unreal, page: dict) -> dict:
    """只读取当前Editor对象；独立进程冷重载由另一个只读脚本确认。"""
    blueprint = unreal.EditorAssetLibrary.load_asset(page['widget'])
    if not isinstance(blueprint, unreal.WidgetBlueprint):
        raise RuntimeError('missing real WidgetBlueprint: ' + page['widget'])
    generated = blueprint.generated_class()
    default = unreal.get_default_object(generated) if generated else None
    if not isinstance(default, unreal.DivineBeastsDemoHubScreen):
        raise RuntimeError('wrong native page parent: ' + page['widget'])
    fixed = str(default.get_editor_property('fixed_plugin_id'))
    if fixed != page['plugin_id']:
        raise RuntimeError('fixed plugin mismatch: ' + page['widget'])
    status = str(blueprint.get_editor_property('status'))
    if blueprint.get_editor_property('status') != unreal.BlueprintStatus.BS_UP_TO_DATE:
        raise RuntimeError('page not compiled up to date: ' + page['widget'] + ': ' + status)
    definition = unreal.EditorAssetLibrary.load_asset(page['definition_template'])
    if not isinstance(definition, unreal.GamePlatformUIScreenDefinition):
        raise RuntimeError('missing canonical screen definition template')
    if str(definition.get_editor_property('screen_id')) != 'DivineBeasts.Demo.Hub':
        raise RuntimeError('canonical screen template identity mismatch')
    widget_class = definition.get_editor_property('widget_class')
    expected_hub_class = HUB + '.' + HUB.rsplit('/', 1)[-1] + '_C'
    if widget_class is None or widget_class.get_path_name() != expected_hub_class:
        raise RuntimeError('canonical screen template class mismatch')
    return {**page, 'compiled': True, 'saved': True, 'fixed_plugin_id': fixed,
            'tree_audit': 'requires_direct_monolith_ui_get_widget_tree'}


def author_pages(manifest_path: str = str(AUTHOR), selected: list[str] | None = None) -> dict:
    """由直接Monolith执行真实引擎复制/编译/保存；不在源码中创建视觉树。"""
    document, fingerprint = checked_source(manifest_path)
    plan = make_page_plan(document, selected)
    import unreal
    check_editor(unreal)
    assets = unreal.EditorAssetLibrary
    targets = {page['widget'] for page in plan}
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if dirty.intersection(targets | {HUB, HUB_DEFINITION}):
        raise RuntimeError('target or template has unsaved edits; preserve it before authoring')
    template = assets.load_asset(HUB)
    definition_template = assets.load_asset(HUB_DEFINITION)
    if not isinstance(template, unreal.WidgetBlueprint) or not isinstance(definition_template, unreal.GamePlatformUIScreenDefinition):
        raise RuntimeError('canonical native Hub templates are missing')
    if str(unreal.get_default_object(template.generated_class()).get_editor_property('fixed_plugin_id')) not in ('None', ''):
        raise RuntimeError('Hub template must not carry a fixed plugin')
    # 包路径由唯一注册表内的DBAUIPack_Core描述定位，所有备份/报告留在当前工程。
    ui_pack = next(p for p in document['plugins'] if p['plugin_id'] == 'DBAUIPack_Core')
    content = (WORKSPACE / ui_pack['descriptor_path']).parent / 'Content'
    backup = EVIDENCE / 'Backups' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    for page in plan:
        for key in ('widget',):
            package = page[key]
            existing = assets.load_asset(package) if assets.does_asset_exist(package) else None
            if existing is not None:
                correct = isinstance(existing, unreal.WidgetBlueprint if key == 'widget' else unreal.GamePlatformUIScreenDefinition)
                if not correct:
                    raise RuntimeError('refusing to overwrite wrong native asset type: ' + package)
                relative = Path(package.removeprefix('/DBAUIPack_Core/')).with_suffix('.uasset')
                original = content / relative
                if not original.is_file():
                    raise RuntimeError('saved native target file not found for backup: ' + package)
                destination = backup / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, destination)
    result = authoring_result([], fingerprint)
    result.update(editor_pid=os.getpid(), engine=str(unreal.SystemLibrary.get_engine_version()), backup=str(backup), status='in_progress')
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    report = EVIDENCE / 'T07.Pages.Authored.json'
    def write_report():
        report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    write_report()
    try:
        for page in plan:
            blueprint = assets.load_asset(page['widget']) if assets.does_asset_exist(page['widget']) else assets.duplicate_asset(HUB, page['widget'])
            if not isinstance(blueprint, unreal.WidgetBlueprint):
                raise RuntimeError('native page duplication failed: ' + page['widget'])
            default = unreal.get_default_object(blueprint.generated_class())
            if not isinstance(default, unreal.DivineBeastsDemoHubScreen):
                raise RuntimeError('wrong native page parent: ' + page['widget'])
            default.set_editor_property('fixed_plugin_id', page['plugin_id'])
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            if str(unreal.get_default_object(blueprint.generated_class()).get_editor_property('fixed_plugin_id')) != page['plugin_id']:
                raise RuntimeError('compile lost fixed plugin context: ' + page['widget'])
            if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
                raise RuntimeError('native asset save failed: ' + blueprint.get_path_name())
            result['pages'].append(read_page(unreal, page))
            result['saved_page_count'] = len(result['pages'])
            write_report()
    except Exception as failure:
        result.update(status='failed', error=str(failure))
        write_report()
        raise
    result['status'] = 'saved_requires_independent_reload'
    result['saved_epoch'] = time.time()
    write_report()
    return result


if __name__ == '__main__':
    # 仅真实Monolith的run_python执行此入口；普通Python可导入make_page_plan做离线验证。
    print(json.dumps(author_pages(), ensure_ascii=False))
