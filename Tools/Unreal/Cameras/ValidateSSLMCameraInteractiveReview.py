# -*- coding: utf-8 -*-
"""独立UE5.8加载交互蓝图、真实镜头Profile及四个绑定；仅资产级验证，不声称真实键盘已测试。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview"
BP="/Game/Development/PluginDemos/Blueprints/BP_DBA_CinematicCameraReviewController"
expected=[("24mm",24.0),("35mm",35.0),("50mm",50.0),("85mm",85.0)]
def main():
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP) or not assets.does_asset_exist(BP):
        raise RuntimeError("交互蓝图或地图未真实保存")
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP):
        raise RuntimeError("无法在独立编辑器加载演示地图")
    actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    target=actors.get("DBA_CinematicCameraReview_Controls")
    if not isinstance(target,unreal.DivineBeastsCinematicCameraReviewActor):
        raise RuntimeError("演示Actor并未实例化正确的C++父类")
    if "BP_DBA_CinematicCameraReviewController" not in target.get_class().get_name():
        raise RuntimeError("控制器不是蓝图生成类")
    verified=[]
    for suffix,mm in expected:
        camera=target.get_editor_property("camera"+suffix)
        profile=target.get_editor_property("profile"+suffix)
        if not isinstance(camera,unreal.CineCameraActor):
            raise RuntimeError("缺真实CineCamera引用："+suffix)
        if not isinstance(profile,unreal.GamePlatformCinematicCameraProfile):
            raise RuntimeError("镜头Profile引用类型不正确："+suffix)
        if abs(float(profile.get_editor_property("focal_length_mm"))-mm)>.05 or not profile.is_valid_profile():
            raise RuntimeError("镜头参数不匹配："+suffix)
        verified.append({"lens":suffix,"profile":profile.get_path_name(),"camera":camera.get_actor_label()})
    payload={"engine":str(unreal.SystemLibrary.get_engine_version()),"map":MAP,
             "blueprint":BP,"controllers":1,"verified":verified,"actor_count":len(actors),
             "independent_editor_reload":True,"gameplay_input_test":"pending"}
    dest=ROOT/"Saved/Validation/CinematicCamera/InteractiveReviewReload.json"
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_INTERACTIVE_CAMERA_RELOAD_OK "+json.dumps(payload,ensure_ascii=False))
if __name__=="__main__":main()