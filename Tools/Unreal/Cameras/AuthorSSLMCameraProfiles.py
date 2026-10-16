# -*- coding: utf-8 -*-
"""UE5.8平台电影相机参数数据资产制作。只处理本次项目内容包的独立相机预设。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
PREFIX="/DBAPresentationPack_Core/Cameras/Profiles"
PROFILES=[
 ("DA_DBA_Camera_Exploration","EXPLORATION",28.0,5.6,1250.0,.6,False),
 ("DA_DBA_Camera_CharacterPreview","CHARACTER_PREVIEW",65.0,2.8,330.0,.65,True),
 ("DA_DBA_Camera_SkillShowcase","CINEMATIC",45.0,4.0,650.0,.35,True),
 ("DA_DBA_Camera_Arena","ARENA",35.0,8.0,1500.0,.2,False)
]
def main():
    project=str(unreal.Paths.get_project_file_path()).replace(chr(92),"/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不得修改其它工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("仅允许正式UE5.8")
    cls=getattr(unreal,"GamePlatformCinematicCameraProfile",None)
    enums=getattr(unreal,"GamePlatformCameraPresentationMode",None)
    if cls is None or enums is None:
        raise RuntimeError("当前Editor Camera插件反射类尚未成功链接：禁止创建替代资产")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    if not assets.does_directory_exist(PREFIX): assets.make_directory(PREFIX)
    entries=[]
    for name,mode,focal,aperture,focus,blend,enable in PROFILES:
        path=PREFIX+"/"+name
        existing=assets.does_asset_exist(path)
        if existing:
            obj=assets.load_asset(path)
            if not isinstance(obj,cls): raise RuntimeError("同名资产类型不符: "+path)
        else:
            factory=unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class",cls)
            obj=tools.create_asset(name,PREFIX,cls,factory)
        if not obj: raise RuntimeError("创建预设失败: "+path)
        obj.set_editor_property("mode",getattr(enums,mode))
        obj.set_editor_property("sensor_width_mm",36.0)
        obj.set_editor_property("focal_length_mm",focal)
        obj.set_editor_property("aperture",aperture)
        obj.set_editor_property("focus_distance_cm",focus)
        obj.set_editor_property("blend_time_seconds",blend)
        obj.set_editor_property("enable_cinematic_focus",enable)
        if not obj.is_valid_profile(): raise RuntimeError("C++镜头校验失败: "+path)
        if not assets.save_loaded_asset(obj,only_if_is_dirty=False): raise RuntimeError("无法保存镜头DataAsset: "+path)
        entries.append(dict(asset=path,mode=mode,focal_mm=focal,aperture=aperture,
                            focus_cm=focus,blend_seconds=blend,enabled_focus=enable,
                            existed_before=existing,validated=True))
    result=dict(engine=str(unreal.SystemLibrary.get_engine_version()),
                verified_ue_classes=True,assets=entries,independent_reload=False)
    out=ROOT/"Saved/Validation/CinematicCamera/AuthorProfiles.json"
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CAMERA_PROFILES_CREATED",json.dumps(result,ensure_ascii=False))
if __name__=="__main__":main()