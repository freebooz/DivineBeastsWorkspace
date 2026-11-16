"""通过直接Monolith冷重载读取人工夹具；只读UE资产，不代替运行或人工验收。"""
from pathlib import Path
import importlib.util
import json
import os

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginManualUI20261027'

def validate():
    import unreal
    source=ROOT/'Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py'
    spec=importlib.util.spec_from_file_location('manual_fixture_reload',source)
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    authored=json.loads((OUT/'T04.Fixtures.Authored.json').read_text(encoding='utf-8-sig'))
    if not helper.is_cold_reload(authored['editor_pid'],authored['saved_epoch'],os.getpid(),helper.current_process_started_epoch()):
        raise RuntimeError('需要保存后启动的另一个Editor进程冷读夹具')
    recipe=json.loads((ROOT/'Docs/Implementation/PluginDemos/ManualFixtureRecipes.json').read_text(encoding='utf-8-sig'))
    loaded={}
    for entry in authored['assets']:
        asset=unreal.load_asset(entry['asset'])
        if asset is None or asset.get_class().get_path_name()!=entry['class']:
            raise RuntimeError('夹具身份/类型不匹配：'+entry['asset'])
        loaded[entry['asset']]=asset
    profile=loaded[recipe['input']['asset']]
    logical=profile.get_editor_property('logical_id').to_dict()
    assert logical=={'namespace':'divinebeasts.manual.input','name':'profile','logical_version':1}
    definitions=profile.get_editor_property('actions')
    assert len(definitions)==4
    for native,expected in zip(definitions,recipe['input']['actions']):
        text=native.export_text()
        assert expected['asset'] in text and expected['descriptor']['semantic_id_tag'] in text
        action=loaded[expected['asset']]
        assert action.get_editor_property('value_type')==getattr(unreal.InputActionValueType,expected['value_type'].upper())
    context=loaded[recipe['input']['contexts'][0]['context']]
    mappings=context.get_editor_property('default_key_mappings').get_editor_property('mappings')
    keys=[m.get_editor_property('key').export_text() for m in mappings]
    assert keys==[m['key'] for m in recipe['input']['mappings']]
    for mapping,expected in zip(mappings,recipe['input']['mappings']):
        assert mapping.get_editor_property('action').get_name()==expected['action']
        assert [m.get_class().get_path_name() for m in mapping.get_editor_property('modifiers')]==[m['class'] for m in expected['modifiers']]
    confirm=mappings[-1].get_editor_property('player_mappable_key_settings')
    assert str(confirm.get_editor_property('name'))=='ManualConfirm'
    flow=loaded[recipe['flow']['asset']]
    assert str(flow.get_editor_property('entry_node_id'))==recipe['flow']['entry_node_id']
    assert len(flow.get_editor_property('nodes'))==len(recipe['flow']['nodes'])
    equipment=loaded[recipe['equipment']['asset']]
    assert str(equipment.get_editor_property('socket_name'))==recipe['equipment']['socket_name']
    assert equipment.get_editor_property('static_mesh').get_path_name()==recipe['equipment']['static_mesh']
    assets=[unreal.EditorAssetLibrary.find_asset_data(path) for path in loaded]
    settings=unreal.ValidateAssetsSettings(skip_excluded_directories=False,show_if_no_failures=False,
        collect_per_asset_details=True,validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures,result=unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(assets,settings)
    details=result.to_dict()
    assert failures==0 and details['num_invalid']==0 and details['num_valid']==8,details
    report={'reader_pid':os.getpid(),'author_pid':authored['editor_pid'],'independent_reload':True,
        'assets_checked':8,'mapping_keys':keys,'logical_id':logical,
        'native_validation':{key:int(value) for key,value in details.items() if key.startswith('num_') and isinstance(value,(int,bool))},
        'runtime':'not_proven_by_readback','human_review':'pending'}
    (OUT/'T04.Fixtures.Readback.Cold.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'independent_reload':True,'assets_checked':8,'native_valid':8,'keys':keys}))

if __name__=='__main__':validate()
