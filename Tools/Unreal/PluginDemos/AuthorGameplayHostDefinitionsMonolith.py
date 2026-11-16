"""只创建隔离Gameplay Host所需的中立Pawn/Experience定义，不改Hub或准入状态。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
BASE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/'


def primary(text):
    value = unreal.PrimaryAssetId()
    assert value.import_text('"' + text + '"') and value.export_text() == text
    return value


def author():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    assert project == ROOT / 'Game/DivineBeastsArena.uproject'
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
    assert not (unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() +
                unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()), '其它脏包保留'
    definitions = [('Pawn', unreal.GamePlatformPawnDefinition, 'pawn.demo'),
                   ('Experience', unreal.GamePlatformExperienceDefinition, 'experience.demo')]
    paths = [BASE + 'DA_DBA_Manual' + name for name, _, _ in definitions]
    assert not any(unreal.EditorAssetLibrary.does_asset_exist(path) for path in paths), '首次作者拒绝覆盖'
    created = {}
    for name, klass, namespace in definitions:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', klass)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_DBA_Manual' + name, BASE.rstrip('/'), klass, factory)
        assert asset is not None
        asset.set_editor_property('logical_id', unreal.GamePlatformId(namespace=namespace, name='gameplay', logical_version=1))
        asset.set_editor_property('data_version', unreal.GamePlatformDataVersion(schema_version=1, content_revision=1))
        asset.set_editor_property('required_definitions', [])
        created[name] = asset
    pawn_id = primary('GamePlatformDefinition:pawn.demo.gameplay@1')
    pawn = created['Pawn']
    pawn.set_editor_property('pawn_class', unreal.load_class(None, '/Script/Engine.DefaultPawn'))
    pawn.set_editor_property('spawn_envelope_half_extent_centimeters', unreal.Vector(42, 42, 96))
    pawn.set_editor_property('pawn_purpose', 'OfflineManualSphere')
    pawn.set_editor_property('shared_definitions', [])
    experience = created['Experience']
    experience.set_editor_property('supported_world_ids', [unreal.GamePlatformId(namespace='world.demo', name='hub', logical_version=1)])
    experience.set_editor_property('purpose', 'OfflineManualGameplay')
    experience.set_editor_property('default_pawn_definition_id', pawn_id)
    experience.set_editor_property('required_definitions', [pawn_id])
    experience.set_editor_property('shared_definitions', [pawn_id])
    experience.set_editor_property('server_definitions', [])
    experience.set_editor_property('spawn_policy_id', 'DemoGameplayFixture')
    experience.set_editor_property('assembly_entries', [])
    experience.set_editor_property('respawn_policy', unreal.GamePlatformRespawnPolicy.SERVER_AUTHORIZED)
    experience.set_editor_property('development_only', True)
    experience.set_editor_property('player_experience_timeout_seconds', 30.0)
    experience.set_editor_property('spawn_timeout_seconds', 30.0)
    experience.set_editor_property('client_preparation_timeout_seconds', 60.0)
    experience.set_editor_property('maximum_waiting_players', 1)
    for asset in created.values():
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()
    settings = unreal.ValidateAssetsSettings(skip_excluded_directories=False, show_if_no_failures=False,
        collect_per_asset_details=True, validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures, results = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(path) for path in paths], settings)
    counts = {key: int(value) for key, value in results.to_dict().items()
              if key.startswith('num_') and isinstance(value, (int, bool))}
    assert failures == 0 and counts['num_valid'] == 2 and counts['num_warnings'] == 0, counts
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'assets': paths,
              'pawn_id': pawn_id.export_text(), 'world_id': 'world.demo.hub@1', 'native_validation': counts,
              'hub_and_player_start_modified': False, 'runtime_admission_granted': False,
              'independent_reload': 'pending', 'native_GameplayHost_runtime_and_human': 'pending'}
    (OUT / 'Gameplay.HostDefinitions.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    author()
