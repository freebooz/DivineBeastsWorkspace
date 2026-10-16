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
        # 真正校验世界空间光轴与角色目标中心的余弦，而不是仅检查DataAsset数值。
        import math
        origin=a.get_actor_location()
        direction=a.get_actor_forward_vector()
        x,y,z=-origin.x,-origin.y,110.0-origin.z
        length=math.sqrt(x*x+y*y+z*z)
        cosine=(direction.x*x+direction.y*y+direction.z*z)/length
        if cosine < .999: raise RuntimeError("电影摄像机朝向不正确:"+label)
        focus=a.get_cine_camera_component().get_editor_property("focus_settings").manual_focus_distance
        if abs(float(focus)-length)>1.0: raise RuntimeError("电影机位对焦距离错误:"+label)
    rig=actors.get("DBA_Cine_BlueprintRig")
    if not isinstance(rig,unreal.CineCameraActor): raise RuntimeError("电影机位蓝图实例缺失")
    if "BP_DBA_CinematicCameraRig" not in rig.get_class().get_name():
        raise RuntimeError("未实例化真实摄像机蓝图")
    if rig.get_editor_property("auto_activate_for_player") != unreal.AutoReceiveInput.PLAYER0:
        raise RuntimeError("摄像机蓝图未接管本地默认镜头")
    # 蓝图实例不仅要属于正确的类，还要保存真正的物理镜头参数和朝向。
    bp_comp=rig.get_cine_camera_component()
    film=bp_comp.get_editor_property("filmback")
    if abs(float(film.sensor_width)-36.0)>.05 or abs(float(film.sensor_height)-20.25)>.05:
        raise RuntimeError("电影摄像机蓝图传感器未匹配16:9预设")
    if abs(float(bp_comp.get_editor_property("current_focal_length"))-35.0)>.05:
        raise RuntimeError("35mm电影摄像机蓝图参数错误")
    pos=rig.get_actor_location()
    facing=rig.get_actor_forward_vector()
    import math
    dx,dy,dz=-pos.x,-pos.y,110.0-pos.z
    distance=math.sqrt(dx*dx+dy*dy+dz*dz)
    if (facing.x*dx+facing.y*dy+facing.z*dz)/distance < .999:
        raise RuntimeError("电影摄像机蓝图没有朝向Manny")
    focus=bp_comp.get_editor_property("focus_settings").manual_focus_distance
    if abs(float(focus)-distance)>1.0:
        raise RuntimeError("电影摄像机蓝图焦点距离不匹配")
    native=actors["DBA_Cine_35mm"]
    if native.get_editor_property("auto_activate_for_player") != unreal.AutoReceiveInput.DISABLED:
        raise RuntimeError("两个35mm机位同时接管本地玩家镜头")
    if "DBA_CameraReview_Manny" not in actors: raise RuntimeError("角色网格缺失")
    record={"engine":str(unreal.SystemLibrary.get_engine_version()),"map":MAP,
            "blueprint":BP,"blueprint_instance":"DBA_Cine_BlueprintRig","actors":len(actors),"focals_mm":VIEWS,
            "editor_reloaded":True,"game_running":False,"manual_visual_review":"pending"}
    dst=ROOT/"Saved/Validation/CinematicCamera/IndependentReload.json"
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CINEMATIC_RELOAD_OK",json.dumps(record,ensure_ascii=False))
if __name__=="__main__": main()