"""直接 Monolith 冷读真实开发资源；正向与故意负向严格分开。"""
from pathlib import Path
import importlib.util
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
BASE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/'


def validate():
    spec = importlib.util.spec_from_file_location('manual_cold_time', ROOT / 'Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    core = json.loads((OUT / 'T05.World.Fixtures.Authored.json').read_text(encoding='utf-8-sig'))
    pcg = json.loads((OUT / 'T05.World.PCGFixtures.Authored.json').read_text(encoding='utf-8-sig'))
    negative = json.loads((OUT / 'T05.World.PCGNegativeFixtures.Authored.json').read_text(encoding='utf-8-sig'))
    wrap = json.loads((OUT / 'TextWrap.Authored.json').read_text(encoding='utf-8-sig'))
    sfx = json.loads((OUT / 'Presentation.SFXFixtures.Authored.json').read_text(encoding='utf-8-sig'))
    nav_fix = json.loads((OUT / 'World.NavFixture.AgentFix.Authored.json').read_text(encoding='utf-8-sig'))
    for author in [core, pcg, negative, wrap, sfx, nav_fix]:
        assert author['editor_pid'] != os.getpid(), '需要保存后独立启动的另一个 Editor'
    maps = ['/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub', '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoStream']
    foundations = ['/Game/Development/Foundation/PCG/Graphs/PCG_Cosmetic',
                   '/Game/Development/Foundation/PCG/Graphs/PCG_StaticCollision',
                   '/Game/Development/Foundation/PCG/Profiles/DA_PCG_Cosmetic',
                   '/Game/Development/Foundation/PCG/Profiles/DA_PCG_StaticCollision']
    paths = list(core['assets'].values()) + list(pcg['assets'].values()) + wrap['packages'] + foundations + maps
    paths += [sfx['cue']] + list(sfx['definitions'].values())
    manifest = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
    pack = (ROOT / next(p['descriptor_path'] for p in manifest['plugins'] if p['plugin_id'] == 'DBAUIPack_Core')).parent / 'Content'
    files = []
    for path in paths + negative['assets']:
        package = path.split('.')[0]
        suffix = '.umap' if package in maps else '.uasset'
        if package.startswith('/Game/'):
            file = ROOT / 'Game/Content' / (package.removeprefix('/Game/') + suffix)
        else:
            assert package.startswith('/DBAUIPack_Core/')
            file = pack / (package.removeprefix('/DBAUIPack_Core/') + suffix)
        assert file.is_file(), file
        files.append(file)
    reader_start = helper.current_process_started_epoch()
    assert reader_start > max(f.stat().st_mtime for f in files), '当前 Editor 早于最终保存，不构成冷读'
    assets = {path: unreal.load_asset(path) for path in paths}
    assert all(assets.values())
    world = unreal.load_asset(core['assets']['World'])
    region = unreal.load_asset(core['assets']['Region'])
    expected = 'GamePlatformDefinition:world.demo.region@1'
    assert [v.export_text() for v in world.get_editor_property('regions')] == [expected]
    assert [v.export_text() for v in world.get_editor_property('required_definitions')] == [expected]
    assert region.get_editor_property('logical_id').get_editor_property('name') == 'region'
    runtime = unreal.load_asset(pcg['assets']['Runtime'])
    assert unreal.GamePlatformPCGEditorLibrary.validate_profile_contract(runtime) == ''
    graph = runtime.get_editor_property('graph_reference')
    if not isinstance(graph, unreal.PCGGraph):
        graph = unreal.load_asset(pcg['assets']['Graph'])
    assert len(graph.get_editor_property('nodes')) == 4 and len(graph.get_all_edges()) == 4
    text_count = 0
    for path in wrap['text_nodes']:
        widget = unreal.find_object(None, path)
        assert isinstance(widget, unreal.TextBlock)
        assert widget.get_editor_property('auto_wrap_text')
        assert widget.get_editor_property('wrapping_policy') == unreal.TextWrappingPolicy.ALLOW_PER_CHARACTER_WRAPPING
        text_count += 1
    for path in wrap['packages']:
        assert assets[path].status == unreal.BlueprintStatus.BS_UP_TO_DATE
    editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert editor_world.get_path_name().split('.')[0] == maps[0]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    original = json.loads((OUT / 'HubNavigation.Before.json').read_text(encoding='utf-8-sig'))['player_starts']
    starts = {a.get_path_name(): a.get_actor_transform().export_text() for a in actors if isinstance(a, unreal.PlayerStart)}
    assert starts == original
    nav = next(a for a in actors if isinstance(a, unreal.RecastNavMesh))
    assert nav.get_editor_property('agent_radius') == nav_fix['agent_radius']
    assert nav.get_editor_property('agent_height') == nav_fix['agent_height']
    profile = unreal.load_asset(core['assets']['NavAgent'])
    assert profile.get_editor_property('agent_radius') == nav_fix['agent_radius']
    assert profile.get_editor_property('agent_height') == nav_fix['agent_height']
    assert nav.get_editor_property('runtime_generation') == unreal.RuntimeGenerationType.DYNAMIC
    streaming = unreal.GameplayStatics.get_streaming_level(editor_world, 'L_DBA_PluginDemoStream')
    assert isinstance(streaming, unreal.LevelStreamingDynamic)
    assert not streaming.get_editor_property('initially_loaded') and not streaming.get_editor_property('initially_visible')
    # 故意负向仅核合同输入；实际Data/PCG拒绝码由Standalone回归独立核验。
    missing = unreal.load_asset(BASE + 'DA_DBA_ManualPCGMissingDependency')
    assert [x.export_text() for x in missing.get_editor_property('required_definitions')] == ['GamePlatformDefinition:pcg.demo.required_missing@1']
    bad = unreal.load_asset(BASE + 'PCG_DBA_ManualInvalidGraph')
    assert isinstance(bad, unreal.PCGGraph) and not bad.get_editor_property('is_template')
    assert len(bad.get_editor_property('nodes')) == 0 and len(bad.get_all_edges()) == 0
    cue = assets[sfx['cue']]
    assert isinstance(cue, unreal.SoundCue)
    first = cue.get_editor_property('first_node')
    assert isinstance(first, unreal.SoundNodeModulatorContinuous)
    pitch = first.get_editor_property('pitch_modulation_params')
    assert str(pitch.get_editor_property('parameter_name')) == 'ManualPitch'
    assert abs(pitch.get_editor_property('min_output') - 0.8) < 0.00001
    assert abs(pitch.get_editor_property('max_output') - 1.2) < 0.00001
    sequence = first.get_editor_property('child_nodes')[0]
    assert isinstance(sequence, unreal.SoundNodeConcatenator) and len(sequence.get_editor_property('child_nodes')) == 16
    spaces = set()
    for path in sfx['definitions'].values():
        definition = assets[path]
        assert isinstance(definition, unreal.GamePlatformSFXDefinition)
        assert {str(n) for n in definition.get_editor_property('allowed_float_parameters')} == {'ManualPitch'}
        assert {str(k): v for k, v in definition.get_editor_property('default_float_parameters').items()} == {'ManualPitch': 0.5}
        spaces.add(str(definition.get_editor_property('playback_space')))
    assert len(spaces) == 3
    settings = unreal.ValidateAssetsSettings(skip_excluded_directories=False, show_if_no_failures=False,
        collect_per_asset_details=True, validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures, result = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(p) for p in assets], settings)
    counts = {k: int(v) for k, v in result.to_dict().items() if k.startswith('num_') and isinstance(v, (int, bool))}
    assert not failures and counts['num_valid'] == len(assets), counts
    report = {'reader_pid': os.getpid(), 'reader_started_epoch': reader_start, 'checked_epoch': time.time(),
              'independent_reload': True, 'positive_assets': len(assets), 'native_validation': counts,
              'wrap_text_nodes': text_count, 'runtime_graph_nodes': 4, 'runtime_graph_edges': 4,
              'negative_inputs_read': 3, 'player_starts_preserved': len(starts), 'stream_initially_unloaded': True,
              'sfx_parameter_consumer': first.get_path_name(), 'sfx_playback_spaces': sorted(spaces), 'sfx_actual_listening': 'pending',
              'spatial_realized_graph': 'separate_pending_authoring_and_Bake', 'runtime_generation_and_cleanup': 'pending',
              'human_review': 'pending'}
    (OUT / 'T05.World.Fixtures.Readback.Cold.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'independent_reload': True, 'positive_assets': len(assets), 'native_validation': counts,
                      'negative_inputs': 3, 'wrap_text_nodes': text_count}))


if __name__ == '__main__':
    validate()
