"""消费真实原生作者输出，显式绑定开发Profile；不生成、不Bake、不改共享模板。"""
raise RuntimeError('SCRIPT_RETIRED: 已退休归档，见Tools/Archive/Project/20261031/README.md。')
from pathlib import Path
import hashlib
import importlib.util
import json
import os
import re
import shutil
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
PROFILE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCG'
REPORT = OUT / 'World.PCG.RealizedBinding.Authored.json'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bind():
    assert not REPORT.exists(), '一次性绑定报告已存在，拒绝重复增修订'
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    assert project == ROOT / 'Game/DivineBeastsArena.uproject'
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, 'PIE仍活动'
    assert not (unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() +
                unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()), '保留其它脏包'
    launch = json.loads((OUT / 'SHA256.WorldAuthoringLive.Launch.json').read_text(encoding='utf-8-sig'))
    base = ROOT / f"Saved/Validation/FoundationM0/{launch['RunId']}/PluginDemoTests"
    result = json.loads((base / 'result.json').read_text(encoding='utf-8-sig'))
    assert result['Status'] == 'Passed' and result['Suite'] == 'WorldAuthoringLive'
    assert result['Suites'][0]['Result']['Cleaned'] and result['Suites'][0]['Result']['ExitCode'] == 0
    raw = json.loads((base / 'WorldAuthoringLive/Automation/index.json').read_text(encoding='utf-8-sig'))
    assert len(raw['tests']) == 3 and all(x['state'] == 'Success' for x in raw['tests'])
    log = (base / 'WorldAuthoringLive/Unreal.log').read_text(encoding='utf-8-sig')
    names = set(re.findall(r'package: (/Game/Development/Foundation/PCG/Realized/PCG_DBA_ManualNative[A-F0-9]{32})', log))
    assert len(names) == 1, names
    graph_path = names.pop()
    graph_file = ROOT / ('Game/Content/' + graph_path.removeprefix('/Game/') + '.uasset')
    assert graph_file.is_file()
    helper_spec = importlib.util.spec_from_file_location('cold_reader', ROOT / 'Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py')
    helper = importlib.util.module_from_spec(helper_spec)
    helper_spec.loader.exec_module(helper)
    reader_start = helper.current_process_started_epoch()
    assert reader_start > graph_file.stat().st_mtime, '新图保存之后启动的独立Editor才可绑定'
    assert result['Suites'][0]['Result']['ProcessId'] != os.getpid()
    profile = unreal.EditorAssetLibrary.load_asset(PROFILE)
    graph = unreal.EditorAssetLibrary.load_asset(graph_path)
    assert isinstance(graph, unreal.PCGGraph)
    old_graph = profile.get_editor_property('graph_reference')
    assert isinstance(old_graph, unreal.PCGGraph) and old_graph.get_path_name().startswith('/Game/Development/Foundation/PCG/Templates/')
    old_version = profile.get_editor_property('data_version').copy()
    profile_file = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/Development/PluginDemos/Fixtures/DA_DBA_ManualPCG.uasset'
    template_package = old_graph.get_path_name().split('.')[0]
    template_file = ROOT / ('Game/Content/' + template_package.removeprefix('/Game/') + '.uasset')
    before_hash, template_hash = digest(profile_file), digest(template_file)
    backup = OUT / 'Backups' / ('PCG-BeforeRealizedBind-' + str(time.time_ns())) / profile_file.name
    backup.parent.mkdir(parents=True)
    shutil.copy2(profile_file, backup)
    version = unreal.GamePlatformDataVersion(schema_version=old_version.get_editor_property('schema_version'),
        content_revision=old_version.get_editor_property('content_revision') + 1)
    try:
        profile.set_editor_property('graph_reference', graph)
        profile.set_editor_property('data_version', version)
        assert unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(profile) == ''
        assert unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False), '真实保存失败'
    except Exception:
        profile.set_editor_property('graph_reference', old_graph)
        profile.set_editor_property('data_version', old_version)
        raise
    assert digest(template_file) == template_hash, '共享模板必须字节不变'
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'author_native_run': launch['RunId'],
              'author_pid': result['Suites'][0]['Result']['ProcessId'], 'independent_graph_reload': True,
              'reader_started_epoch': reader_start, 'profile': PROFILE, 'graph': graph_path,
              'profile_before_sha256': before_hash, 'profile_after_sha256': digest(profile_file),
              'profile_backup': str(backup), 'template': template_package, 'template_sha256_unchanged': template_hash,
              'content_version': version.export_text(), 'nodes': len(graph.get_editor_property('nodes')),
              'edges': len(graph.get_all_edges()), 'native_profile_contract': 'passed',
              'bound_profile_cold_reload': 'pending_next_Editor', 'spatial_Bake_and_human_review': 'pending'}
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    bind()
