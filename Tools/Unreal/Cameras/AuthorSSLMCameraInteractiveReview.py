# -*- coding: utf-8 -*-
"""SSLM UE5.8开发摄像机可操作演示：真实CameraProfile、控制器蓝图和关卡Actor。
仅修改/Game/Development/PluginDemos开发内容，复用已有平台Camera和数据资产类型。
"""
from pathlib import Path
import json
import math
import unreal

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview"
BLUEPRINT="/Game/Development/PluginDemos/Blueprints/BP_DBA_CinematicCameraReviewController"
PROFILES="/Game/Development/PluginDemos/Profiles"
ACTOR_LABEL="DBA_CinematicCameraReview_Controls"
CAMERAS=[
    ("DBA_Cine_24mm","DA_DBA_ReviewCamera_24mm",24.0,5.6,(-640,-20,260)),
    ("DBA_Cine_BlueprintRig","DA_DBA_ReviewCamera_35mm",35.0,5.6,(-850,-200,230)),
    ("DBA_Cine_50mm","DA_DBA_ReviewCamera_50mm",50.0,2.8,(-1080,0,225)),
    ("DBA_Cine_85mm","DA_DBA_ReviewCamera_85mm",85.0,2.8,(-1650,180,215)),
]

def ensure_project():
    current=str(unreal.Paths.get_project_file_path()).replace(chr(92),"/")
    if not current.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止修改非SSLM正式工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("需要锁定正式UE5.8")
    cfg=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/PluginDemos")' not in cfg:
        raise RuntimeError("开发演示目录未排除Cook")
    if not hasattr(unreal,"DivineBeastsCinematicCameraReviewActor"):
        raise RuntimeError("DevClient的ReviewActor未真实编译进UEEditor反射")
    if not hasattr(unreal,"GamePlatformCinematicCameraProfile"):
        raise RuntimeError("平台CameraProfile反射尚不可用")

def ensure_blueprint(assets,tools):
    native=unreal.DivineBeastsCinematicCameraReviewActor
    if assets.does_asset_exist(BLUEPRINT):
        bp=assets.load_asset(BLUEPRINT)
        if not isinstance(bp,unreal.Blueprint):
            raise RuntimeError("既有控制器蓝图类型错误")
        return bp,"existing"
    directory,name=BLUEPRINT.rsplit("/",1)
    if not assets.does_directory_exist(directory):
        assets.make_directory(directory)
    factory=unreal.BlueprintFactory()
    factory.set_editor_property("parent_class",native)
    bp=tools.create_asset(name,directory,unreal.Blueprint,factory)
    if bp is None:
        raise RuntimeError("UE无法生成真正控制器蓝图")
    if hasattr(unreal,"BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not assets.save_loaded_asset(bp,only_if_is_dirty=False):
        raise RuntimeError("控制器蓝图编译保存失败")
    return bp,"created"

def ensure_profile(assets,tools,name,focal,aperture,origin):
    path=PROFILES+"/"+name
    klass=unreal.GamePlatformCinematicCameraProfile
    if assets.does_asset_exist(path):
        asset=assets.load_asset(path)
        if not isinstance(asset,klass):
            raise RuntimeError("同名镜头Profile类型错误："+path)
    else:
        factory=unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class",klass)
        asset=tools.create_asset(name,PROFILES,klass,factory)
    if asset is None:
        raise RuntimeError("创建Profile失败："+path)
    focus=math.sqrt(origin[0]**2+origin[1]**2+(origin[2]-110)**2)
    asset.set_editor_property("mode",unreal.GamePlatformCameraPresentationMode.CHARACTER_PREVIEW)
    asset.set_editor_property("sensor_width_mm",36.0)
    asset.set_editor_property("focal_length_mm",focal)
    asset.set_editor_property("aperture",aperture)
    asset.set_editor_property("focus_distance_cm",focus)
    asset.set_editor_property("blend_time_seconds",0.5)
    asset.set_editor_property("enable_cinematic_focus",True)
    if not asset.is_valid_profile():
        raise RuntimeError("平台Profile字段校验拒绝："+path)
    if not assets.save_loaded_asset(asset,only_if_is_dirty=False):
        raise RuntimeError("镜头Profile未保存："+path)
    return asset,dict(package=path,focal_mm=focal,aperture=aperture,focus_cm=round(focus,2))

def main():
    ensure_project()
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP):
        raise RuntimeError("缺少现有真实电影镜头审核地图")
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    if not assets.does_directory_exist(PROFILES): assets.make_directory(PROFILES)
    bp,status=ensure_blueprint(assets,tools)
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(MAP):
        raise RuntimeError("真实电影镜头地图不能打开")
    labels={a.get_actor_label():a for a in actors.get_all_level_actors()}
    if any(name not in labels for name,_,_,_,_ in CAMERAS):
        raise RuntimeError("地图缺少4个已经制作的CineCamera，拒绝创建替代对象")

    profiles=[]
    profile_assets=[]
    for label,name,focal,aperture,pos in CAMERAS:
        obj,meta=ensure_profile(assets,tools,name,focal,aperture,pos)
        profile_assets.append(obj)
        profiles.append(meta)
    controller=labels.get(ACTOR_LABEL)
    created=False
    if controller is None:
        klass=assets.load_blueprint_class(BLUEPRINT)
        if klass is None:
            raise RuntimeError("控制器蓝图生成类不存在")
        controller=actors.spawn_actor_from_class(klass,unreal.Vector(0,0,100),unreal.Rotator(0,0,0))
        if controller is None: raise RuntimeError("无法实例化真实交互控制器蓝图")
        controller.set_actor_label(ACTOR_LABEL)
        created=True
    if not isinstance(controller,unreal.DivineBeastsCinematicCameraReviewActor):
        raise RuntimeError("控制器Actor类与现有蓝图不兼容")
    if "BP_DBA_CinematicCameraReviewController" not in controller.get_class().get_name():
        raise RuntimeError("该地图没有真正的控制器蓝图实例")

    for (label,_,_,_,_),suffix,profile in zip(CAMERAS,("24mm","35mm","50mm","85mm"),profile_assets):
        controller.set_editor_property("camera"+suffix,labels[label])
        controller.set_editor_property("profile"+suffix,profile)
        if controller.get_editor_property("camera"+suffix)!=labels[label]:
            raise RuntimeError("控制器摄像机硬引用未保存："+suffix)
        if controller.get_editor_property("profile"+suffix)!=profile:
            raise RuntimeError("控制器镜头预设硬引用未保存："+suffix)
    if not levels.save_current_level(): raise RuntimeError("控制器蓝图地图保存失败")
    evidence={
        "engine":str(unreal.SystemLibrary.get_engine_version()),
        "map":MAP,"controller_blueprint":BLUEPRINT,"controller_actor":ACTOR_LABEL,
        "controller_created":created,"blueprint_status":status,"profiles":profiles,
        "actor_count":len(actors.get_all_level_actors()),
        "controls":"1/2/3/4 focal, Z/X aperture, C/V focus, R reset",
        "runtime_auto_cycle":"not_run","manual_review":"pending"}
    output=ROOT/"Saved/Validation/CinematicCamera/AuthorInteractiveReview.json"
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_INTERACTIVE_CAMERA_AUTHOR_OK "+json.dumps(evidence,ensure_ascii=False))
if __name__=="__main__":main()