"""直接 Monolith 制作明确标记的 PCG 负向夹具；不生成、不 Cook、不计正向资产通过。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
BASE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/'


def author():
    if unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('保留其它脏包，停止')
    library = unreal.EditorAssetLibrary
    graph_path = BASE + 'PCG_DBA_ManualInvalidGraph'
    targets = [BASE + 'DA_DBA_ManualPCGMissingDependency', BASE + 'DA_DBA_ManualPCGInvalidGraph']
    if any(library.does_asset_exist(p) for p in [graph_path] + targets):
        raise RuntimeError('不覆盖已有负向夹具')
    source = library.load_asset(BASE + 'DA_DBA_ManualPCGRuntime')
    if not isinstance(source, unreal.GamePlatformPCGProfileDefinition):
        raise RuntimeError('需要真实正向运行图配置')
    assert unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(source) == ''
    graph = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'PCG_DBA_ManualInvalidGraph', BASE.rstrip('/'), unreal.PCGGraph, unreal.PCGGraphFactory())
    assert isinstance(graph, unreal.PCGGraph)
    graph.set_editor_property('is_template', False)
    assert len(graph.get_editor_property('nodes')) == 0 and len(graph.get_all_edges()) == 0
    assert graph.get_input_node() and graph.get_output_node()
    assert library.save_loaded_asset(graph, only_if_is_dirty=False)
    created = []
    for path, name in zip(targets, ['missing_dependency_fixture', 'invalid_graph_fixture']):
        asset = library.duplicate_asset(source.get_path_name(), path)
        assert isinstance(asset, unreal.GamePlatformPCGProfileDefinition)
        asset.set_editor_property('logical_id', unreal.GamePlatformId(namespace='pcg.demo', name=name, logical_version=1))
        if name == 'missing_dependency_fixture':
            missing = unreal.PrimaryAssetId(primary_asset_type=unreal.PrimaryAssetType('GamePlatformDefinition'),
                                            primary_asset_name='pcg.demo.required_missing@1')
            assert missing.export_text() == 'GamePlatformDefinition:pcg.demo.required_missing@1'
            asset.set_editor_property('required_definitions', [missing])
        else:
            asset.set_editor_property('required_definitions', [])
            asset.set_editor_property('graph_reference', graph)
        assert library.save_loaded_asset(asset, only_if_is_dirty=False)
        created.append(asset)
    invalid_graph_error = unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(created[1])
    assert invalid_graph_error is None or 'UnsupportedGraphShape' in str(invalid_graph_error), invalid_graph_error
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'intentional_negative_fixtures': True,
              'assets': [a.get_path_name() for a in created] + [graph.get_path_name()],
              'missing_definition': 'GamePlatformDefinition:pcg.demo.required_missing@1',
              'missing_dependency_expected': 'recursive_definition_lease_failure_despite_foreign_resident_graph_and_mesh',
              'invalid_graph_expected': 'UnsupportedGraphShape', 'actual_graph_nodes': 0, 'actual_graph_edges': 0,
              'graph_validation_python_result': str(invalid_graph_error), 'generated': False,
              'cold_reload': 'pending', 'native_regressions': 'pending', 'human_review': 'pending', 'cook_requested': False}
    (OUT / 'T05.World.PCGNegativeFixtures.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'negative_assets_saved': 3, 'invalid_graph_nodes': 0, 'invalid_graph_edges': 0,
                      'graph_contract_rejected': invalid_graph_error is None, 'positive_asset_acceptance': False}))


if __name__ == '__main__':
    author()
