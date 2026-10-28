"""直接Monolith冷读6个通用表现样板，不把原生资产校验提升为运行/视听或人工通过。"""
from pathlib import Path
import importlib.util
import json
import os

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginManualUI20261027'

def validate():
    import unreal
    spec=importlib.util.spec_from_file_location('presentation_cold_helper',ROOT/'Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py')
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    authored=json.loads((OUT/'T05.Presentation.Fixtures.Authored.json').read_text(encoding='utf-8-sig'))
    if not helper.is_cold_reload(authored['editor_pid'],authored['saved_epoch'],os.getpid(),helper.current_process_started_epoch()):
        raise RuntimeError('需要保存后新启动的另一个Editor冷读')
    assets={path:unreal.load_asset(path) for path in authored['assets']}
    if any(value is None for value in assets.values()):raise RuntimeError('原生样板缺失')
    parents={'TestScreen':unreal.GamePlatformUIScreen,'Notification':unreal.GamePlatformNotificationWidget,
             'Feedback':unreal.GamePlatformFeedbackWidget,'Nameplate':unreal.GamePlatformWorldNameplateWidget}
    widgets={}
    for suffix,parent in parents.items():
        bp=next(v for p,v in assets.items() if p.split('.')[-1]=='WBP_DBA_PluginDemo'+suffix)
        assert isinstance(bp,unreal.WidgetBlueprint) and bp.status==unreal.BlueprintStatus.BS_UP_TO_DATE
        assert isinstance(unreal.get_default_object(bp.generated_class()),parent)
        widgets[suffix]=bp
    shake=next(v for p,v in assets.items() if p.split('.')[-1]=='BP_DBA_PluginDemoHitShake')
    assert shake.status==unreal.BlueprintStatus.BS_UP_TO_DATE
    pattern=unreal.get_default_object(shake.generated_class()).get_editor_property('root_shake_pattern')
    assert pattern.get_class().get_path_name()=='/Script/EngineCameras.PerlinNoiseCameraShakePattern'
    for field,value in authored['shake_pattern'].items():assert abs(pattern.get_editor_property(field)-value)<0.00001
    definition=next(v for p,v in assets.items() if p.split('.')[-1]=='DA_DBA_PluginDemoTestScreen')
    assert isinstance(definition,unreal.GamePlatformUIScreenDefinition)
    assert str(definition.get_editor_property('screen_id'))=='PluginDemo.ManualScreen'
    assert definition.get_editor_property('layer')==unreal.GamePlatformUILayer.MODAL
    assert definition.get_editor_property('input_mode')==unreal.GamePlatformUIInputMode.GAME_AND_UI
    assert definition.get_editor_property('pause_policy')==unreal.GamePlatformUIPausePolicy.NEVER
    assert not definition.get_editor_property('survives_travel')
    assert str(definition.get_editor_property('default_focus_widget_name'))=='CloseButton'
    widget_class=definition.get_editor_property('widget_class')
    path=widget_class.get_path_name() if hasattr(widget_class,'get_path_name') else widget_class.to_dict()['path_string']
    assert path==widgets['TestScreen'].generated_class().get_path_name()
    settings=unreal.ValidateAssetsSettings(skip_excluded_directories=False,show_if_no_failures=False,
        collect_per_asset_details=True,validation_usecase=unreal.DataValidationUsecase.MANUAL)
    failures,result=unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(path) for path in assets],settings)
    counts={k:int(v) for k,v in result.to_dict().items() if k.startswith('num_') and isinstance(v,(int,bool))}
    assert failures==0 and counts['num_valid']==6 and counts['num_invalid']==0,counts
    report={'reader_pid':os.getpid(),'author_pid':authored['editor_pid'],'independent_reload':True,
        'assets_checked':6,'native_validation':counts,'shake_duration':pattern.get_editor_property('duration'),
        'screen_widget_class':path,'runtime':'separate_native_live_evidence_required','visual_audio':'pending','human_review':'pending'}
    (OUT/'T05.Presentation.Fixtures.Readback.Cold.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'independent_reload':True,'native_valid':6,'shake_duration':report['shake_duration']}))

if __name__=='__main__':validate()
