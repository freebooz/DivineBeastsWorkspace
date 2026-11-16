"""直接 Monolith 复用原生四节点开发图，制作运行装饰夹具及待 Realize 的空间源配置。"""
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
        raise RuntimeError('保留已有脏包，拒绝作者修改')
    library = unreal.EditorAssetLibrary
    paths = {'Graph': BASE + 'PCG/PCG_DBA_ManualRuntime', 'Runtime': BASE + 'DA_DBA_ManualPCGRuntime',
             'MeshSet': BASE + 'DA_DBA_ManualMeshSet', 'SpatialSource': BASE + 'DA_DBA_ManualPCG'}
    if any(library.does_asset_exist(p) for p in paths.values()):
        raise RuntimeError('首次作者入口拒绝覆盖已占用夹具')
    source_graph = library.load_asset('/Game/Development/Foundation/PCG/Graphs/PCG_Cosmetic')
    source_profile = library.load_asset('/Game/Development/Foundation/PCG/Profiles/DA_PCG_Cosmetic')
    if not isinstance(source_graph, unreal.PCGGraph) or not isinstance(source_profile, unreal.GamePlatformPCGProfileDefinition):
        raise RuntimeError('需先通过真实 GamePlatformPCGEditorLibrary.CreateDevelopmentAssets 创建原生四节点样板')
    if unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(source_profile) != '':
        raise RuntimeError('既有原生四节点图合同失败')
    graph = library.duplicate_asset(source_graph.get_path_name(), paths['Graph'])
    runtime = library.duplicate_asset(source_profile.get_path_name(), paths['Runtime'])
    if not isinstance(graph, unreal.PCGGraph) or not isinstance(runtime, unreal.GamePlatformPCGProfileDefinition):
        raise RuntimeError('引擎原生复制开发图/配置失败')
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def obtain(name, klass):
        f = unreal.DataAssetFactory()
        f.set_editor_property('data_asset_class', klass)
        folder, _, short = paths[name].rpartition('/')
        result = tools.create_asset(short, folder, klass, f)
        if not isinstance(result, klass):
            raise RuntimeError('原生 DataAsset 创建失败: ' + name)
        return result

    def identity(asset, namespace, name):
        asset.set_editor_property('logical_id', unreal.GamePlatformId(namespace=namespace, name=name, logical_version=1))
        asset.set_editor_property('data_version', unreal.GamePlatformDataVersion(schema_version=1, content_revision=1))
        asset.set_editor_property('required_definitions', [])

    identity(runtime, 'pcg.demo', 'cosmetic')
    runtime.set_editor_property('graph_reference', graph)
    runtime.set_editor_property('execution_policy', unreal.GamePlatformPCGExecutionPolicy.RUNTIME_COSMETIC)
    region = unreal.GamePlatformId(namespace='world.demo', name='region', logical_version=1)
    runtime.set_editor_property('region_id', region)
    runtime.set_editor_property('uniform_scale', 0.2)
    runtime.set_editor_property('minimum_outputs', 1)
    mesh_set = obtain('MeshSet', unreal.GamePlatformPCGMeshSetDefinition)
    identity(mesh_set, 'pcg.demo', 'meshset')
    entry = unreal.GamePlatformPCGMeshSetEntry()
    if not entry.import_text('(Mesh="/Engine/BasicShapes/Cube.Cube",Weight=1.0,bStaticCollision=False,bCastShadow=True,bNanitePreferred=False)'):
        raise RuntimeError('真实 MeshSet 条目导入失败')
    mesh_set.set_editor_property('entries', [entry])
    spatial = obtain('SpatialSource', unreal.GamePlatformPCGProfileDefinition)
    identity(spatial, 'pcg.demo', 'spatial')
    mesh_id = unreal.PrimaryAssetId(primary_asset_type=unreal.PrimaryAssetType('GamePlatformDefinition'), primary_asset_name='pcg.demo.meshset@1')
    assert mesh_id.export_text() == 'GamePlatformDefinition:pcg.demo.meshset@1'
    template = library.load_asset('/Game/Development/Foundation/PCG/Templates/TPL_ScatterSurface')
    assert isinstance(template, unreal.PCGGraph)
    for field, value in [('graph_reference', template), ('template_id', 'TPL_ScatterSurface'), ('template_version', 1),
                         ('execution_policy', unreal.GamePlatformPCGExecutionPolicy.EDITOR_GENERATED_STATIC),
                         ('region_id', region), ('uniform_scale', 0.2), ('minimum_outputs', 1),
                         ('mesh_set_definition_id', mesh_id), ('required_definitions', [mesh_id])]:
        spatial.set_editor_property(field, value)
    for asset in [graph, runtime, mesh_set, spatial]:
        if not library.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('真实原生保存失败: ' + asset.get_path_name())
    error = unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(runtime)
    if error != '':
        raise RuntimeError('运行装饰图真实合同失败: ' + str(error))
    nodes = list(graph.get_editor_property('nodes'))
    classes = [n.get_settings().get_class().get_path_name() for n in nodes]
    assert len(nodes) == 4
    edges = list(graph.get_all_edges())
    assert len(edges) == 4
    edge_rows = []
    for edge in edges:
        start = edge.get_editor_property('input_pin')
        end = edge.get_editor_property('output_pin')
        edge_rows.append({'upstream': start.get_path_name(), 'downstream': end.get_path_name()})
    settings = unreal.ValidateAssetsSettings(skip_excluded_directories=False, show_if_no_failures=False,
        collect_per_asset_details=True, validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures, result = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [library.find_asset_data(p) for p in paths.values()], settings)
    counts = {k: int(v) for k, v in result.to_dict().items() if k.startswith('num_') and isinstance(v, (int, bool))}
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'assets': paths, 'runtime_contract': 'passed',
              'runtime_nodes': classes, 'runtime_real_edges': edge_rows, 'native_validation': counts,
              'validation_failures': failures, 'cold_reload': 'pending', 'runtime_generation': 'pending',
              'spatial_state': 'source_template_staged_requires_Data_lease_and_realized_graph_creation_before_positive_approval',
              'spatial_bake': 'pending', 'human_review': 'pending'}
    (OUT / 'T05.World.PCGFixtures.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'assets_saved': 4, 'runtime_contract': 'passed', 'runtime_nodes': len(nodes),
                      'runtime_edges': len(edges), 'spatial_realize': 'pending', 'validation': counts}))
    assert not failures


if __name__ == '__main__':
    author()
