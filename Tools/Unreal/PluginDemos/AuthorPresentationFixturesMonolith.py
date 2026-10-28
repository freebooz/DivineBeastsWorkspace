"""直接Monolith制作有限镜头震动及UIScreen定义；四个UMG及事件图先由原生UI/Blueprint工具保存。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginManualUI20261027'
BASE='/DBAUIPack_Core/Development/PluginDemos/Fixtures/'

def author():
    if Path(unreal.Paths.get_project_file_path()).resolve()!=ROOT/'Game/DivineBeastsArena.uproject':
        raise RuntimeError('不是当前正式工程')
    dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:raise RuntimeError('存在脏包，保留并停止')
    assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
    widgets={}
    for name,parent in [('TestScreen',unreal.GamePlatformUIScreen),('Notification',unreal.GamePlatformNotificationWidget),
                        ('Feedback',unreal.GamePlatformFeedbackWidget),('Nameplate',unreal.GamePlatformWorldNameplateWidget)]:
        p=BASE+'UI/WBP_DBA_PluginDemo'+name;bp=assets.load_asset(p)
        if not isinstance(bp,unreal.WidgetBlueprint) or bp.status!=unreal.BlueprintStatus.BS_UP_TO_DATE or not isinstance(unreal.get_default_object(bp.generated_class()),parent):
            raise RuntimeError('原生UMG样板未先创建/编译或父类不符：'+p)
        widgets[name]=bp
    shake_path=BASE+'Camera/BP_DBA_PluginDemoHitShake'
    definition_path=BASE+'UI/DA_DBA_PluginDemoTestScreen'
    if assets.does_asset_exist(shake_path) or assets.does_asset_exist(definition_path):
        raise RuntimeError('新作者只创建缺失资产；已有版本须独立备份/核对后再更新')
    parent=unreal.load_class(None,'/Script/EngineCameras.DefaultCameraShakeBase')
    if parent is None:raise RuntimeError('真实EngineCameras类未加载')
    factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',parent)
    folder,_,name=shake_path.rpartition('/')
    shake=tools.create_asset(name,folder,unreal.Blueprint,factory)
    if shake is None:raise RuntimeError('原生镜头震动Blueprint创建失败')
    default=unreal.get_default_object(shake.generated_class());pattern=default.get_editor_property('root_shake_pattern')
    if pattern is None or pattern.get_class().get_path_name()!='/Script/EngineCameras.PerlinNoiseCameraShakePattern' or pattern.get_outer()!=default:
        raise RuntimeError('只允许修改本次Blueprint自有真实Perlin模板，不触碰父CDO')
    properties={'duration':0.35,'blend_in_time':0.05,'blend_out_time':0.1,
                'location_amplitude_multiplier':0.0,'rotation_amplitude_multiplier':0.25}
    for field,value in properties.items():pattern.set_editor_property(field,value)
    unreal.BlueprintEditorLibrary.compile_blueprint(shake)
    if shake.status!=unreal.BlueprintStatus.BS_UP_TO_DATE:raise RuntimeError('镜头震动编译失败')
    pattern=unreal.get_default_object(shake.generated_class()).get_editor_property('root_shake_pattern')
    for field,value in properties.items():
        if abs(pattern.get_editor_property(field)-value)>0.00001:raise RuntimeError('编译丢失震动字段：'+field)
    if not assets.save_loaded_asset(shake,only_if_is_dirty=False):raise RuntimeError('震动保存失败')
    definition_factory=unreal.DataAssetFactory();definition_factory.set_editor_property('data_asset_class',unreal.GamePlatformUIScreenDefinition)
    folder,_,name=definition_path.rpartition('/')
    definition=tools.create_asset(name,folder,unreal.GamePlatformUIScreenDefinition,definition_factory)
    if definition is None:raise RuntimeError('UIScreenDefinition创建失败')
    for field,value in [('screen_id','PluginDemo.ManualScreen'),('widget_class',widgets['TestScreen'].generated_class()),
                        ('layer',unreal.GamePlatformUILayer.MODAL),('input_mode',unreal.GamePlatformUIInputMode.GAME_AND_UI),
                        ('pause_policy',unreal.GamePlatformUIPausePolicy.NEVER),('survives_travel',False),
                        ('default_focus_widget_name','CloseButton')]:definition.set_editor_property(field,value)
    if not assets.save_loaded_asset(definition,only_if_is_dirty=False):raise RuntimeError('UIScreenDefinition保存失败')
    report={'editor_pid':os.getpid(),'saved_epoch':time.time(),'assets':[bp.get_path_name() for bp in widgets.values()]+[shake.get_path_name(),definition.get_path_name()],
            'shake_pattern':properties,'screen_id':'PluginDemo.ManualScreen','native_created':6,
            'cold_reload':'pending','runtime':'pending','visual_audio':'pending','human_review':'pending'}
    (OUT/'T05.Presentation.Fixtures.Authored.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'native_created':6,'shake_duration':0.35,'screen_id':'PluginDemo.ManualScreen','cold_reload':'pending'}))

if __name__=='__main__':author()
