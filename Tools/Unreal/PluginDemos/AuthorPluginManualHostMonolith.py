"""仅通过直接Monolith制作Hub开发宿主；不装配生产准入/体验，不修改PlayerStart。"""
from pathlib import Path
import json
import os
import shutil
import time
import unreal

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginManualUI20261027'
PACKAGE='/Game/Development/PluginDemos/Blueprints/BP_DBA_PluginManualHost'
MAP='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'

def author():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split('.')[0]!=MAP:raise RuntimeError('只允许当前真实开发Hub地图')
    dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:raise RuntimeError('已有脏包，保留并拒绝作者更新')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    starts={a.get_path_name():a.get_actor_transform().export_text() for a in actors if isinstance(a,unreal.PlayerStart)}
    assets=unreal.EditorAssetLibrary
    bp=assets.load_asset(PACKAGE) if assets.does_asset_exist(PACKAGE) else None
    if bp is not None:
        disk=ROOT/'Game/Content/Development/PluginDemos/Blueprints/BP_DBA_PluginManualHost.uasset'
        backup=OUT/'Backups'/('ManualHost-'+str(time.time_ns()))/'BP_DBA_PluginManualHost.uasset'
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(disk,backup)
    else:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.GameModeBase)
        folder,_,name=PACKAGE.rpartition('/')
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Blueprint,factory)
    if not isinstance(bp,unreal.Blueprint):raise RuntimeError('原生开发GameMode Blueprint创建/类型失败')
    controller=unreal.load_class(None,'/Script/DBAWorldsRuntime.DivineBeastsWorldPlayerController')
    if controller is None:raise RuntimeError('实际项目最终镜头通知控制器未加载')
    default=unreal.get_default_object(bp.generated_class())
    if not isinstance(default,unreal.GameModeBase):raise RuntimeError('既有开发宿主父类不匹配')
    for name,value in [('player_controller_class',controller),('game_state_class',unreal.GameStateBase),
                       ('player_state_class',unreal.PlayerState),('default_pawn_class',unreal.DefaultPawn)]:
        default.set_editor_property(name,value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if bp.status!=unreal.BlueprintStatus.BS_UP_TO_DATE:raise RuntimeError('原生宿主编译失败')
    default=unreal.get_default_object(bp.generated_class())
    assert default.get_editor_property('player_controller_class')==controller
    if not assets.save_loaded_asset(bp,only_if_is_dirty=False):raise RuntimeError('宿主保存失败')
    world.get_world_settings().set_editor_property('default_game_mode',bp.generated_class())
    assert starts=={a.get_path_name():a.get_actor_transform().export_text() for a in actors if isinstance(a,unreal.PlayerStart)}
    report={'package':PACKAGE,'map':MAP,'controller_class':controller.get_path_name(),'parent':'/Script/Engine.GameModeBase',
            'player_starts_preserved':starts,'editor_pid':os.getpid(),'saved_epoch':time.time(),
            'map_save':'requires_exact_monolith_save_packages','cold_reload':'pending','runtime':'pending','visual':'pending',
            'limitations':'Engine DefaultPawn无骨骼；本宿主不证明雪足接触或生产准入/权威GameplayActive'}
    (OUT/'WorldObservation.ManualHost.Authored.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'saved_blueprint':PACKAGE,'map_requires_save':MAP,'controller':controller.get_path_name(),'player_starts_preserved':len(starts)}))

if __name__=='__main__':author()
