# -*- coding: utf-8 -*-
"""实际UE5.8独立进程回读镜头资源；不冒充人工视觉或多人联机验收。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview"
BP="/Game/Development/PluginDemos/Blueprints/BP_DBA_CinematicCameraRig"
VIEWS={"DBA_Cine_24mm":24.0,"DBA_Cine_35mm":35.0,"DBA_Cine_50mm":50.0,"DBA_Cine_85mm":85.0}
def main():
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP) or not assets.does_asset_exist(BP): raise RuntimeError("缺少真实蓝图或场景地图")
    if not isinstance(assets.load_asset(BP),unreal.Blueprint): raise RuntimeError("蓝图类型不正确")
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP): raise RuntimeError("场景重载失败")
    actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    for label,focal in VIEWS.items():
        a=actors.get(label)
        if not isinstance(a,unreal.CineCameraActor): raise RuntimeError("缺少CineCameraActor:"+label)
        actual=float(a.get_cine_camera_component().get_editor_property("current_focal_length"))
        if abs(actual-focal)>0.05: raise RuntimeError("摄像机焦距不匹配:"+label)
    if "DBA_CameraReview_Manny" not in actors: raise RuntimeError("角色网格缺失")
    record={"engine":str(unreal.SystemLibrary.get_engine_version()),"map":MAP,
            "blueprint":BP,"actors":len(actors),"focals_mm":VIEWS,
            "editor_reloaded":True,"game_running":False,"manual_visual_review":"pending"}
    dst=ROOT/"Saved/Validation/CinematicCamera/IndependentReload.json"
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CINEMATIC_RELOAD_OK",json.dumps(record,ensure_ascii=False))
if __name__=="__main__": main()