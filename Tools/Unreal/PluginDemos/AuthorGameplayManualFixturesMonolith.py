"""直接Monolith创建隔离玩法夹具；不改生产资产/地图或真实角色资格。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/PluginManualUI20261027'
BASE = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/'


def author():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    assert project == ROOT / 'Game/DivineBeastsArena.uproject', project
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, '先停止本任务PIE'
    assert not (unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() +
                unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()), '保留其它脏包'
    paths = [BASE + 'DA_DBA_ManualAI', BASE + 'M_DBA_ManualHitFlash']
    assert not any(unreal.EditorAssetLibrary.does_asset_exist(path) for path in paths), '首次作者拒绝覆盖'
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.GamePlatformAIDefinition)
    definition = tools.create_asset('DA_DBA_ManualAI', BASE.rstrip('/'), unreal.GamePlatformAIDefinition, factory)
    assert definition is not None
    definition.set_editor_property('ai_definition_id', 'Demo.Manual.AI')
    definition.set_editor_property('brain_type', unreal.GamePlatformAIBrainType.NONE)
    home = definition.get_editor_property('home_policy').copy()
    home.set_editor_property('patrol_radius', 0.0)
    home.set_editor_property('leash_radius', 1000.0)
    home.set_editor_property('return_home_acceptance_radius', 100.0)
    definition.set_editor_property('home_policy', home)
    definition.set_editor_property('attack_range', 180.0)
    definition.set_editor_property('preferred_range', 140.0)
    definition.set_editor_property('required_definitions', [])
    material = tools.create_asset('M_DBA_ManualHitFlash', BASE.rstrip('/'), unreal.Material, unreal.MaterialFactoryNew())
    assert material is not None
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -320, 0)
    color.set_editor_property('constant', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    opacity = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -320, 180)
    opacity.set_editor_property('r', 0.6)
    assert unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert unreal.MaterialEditingLibrary.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    for asset in (definition, material):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()
    settings = unreal.ValidateAssetsSettings()
    settings.set_editor_property('skip_excluded_directories', False)
    settings.set_editor_property('show_if_no_failures', False)
    failures, results = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(path) for path in paths], settings)
    counts = {key: int(value) for key, value in results.to_dict().items()
              if key.startswith('num_') and isinstance(value, (int, bool))}
    assert failures == 0 and counts['num_valid'] == 2 and counts['num_warnings'] == 0, counts
    report = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'assets': paths,
              'ai_definition_id': str(definition.get_editor_property('ai_definition_id')),
              'home_policy': definition.get_editor_property('home_policy').export_text(),
              'native_asset_validation': counts, 'custom_AI_ValidateDefinition': 'pending_native_adapter',
              'material_expressions': 2, 'material_compile_requested': True,
              'independent_reload': 'pending', 'runtime_and_human_review': 'pending'}
    (OUT / 'Gameplay.Fixtures.Authored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    author()
