# -*- coding: utf-8 -*-
"""独立UE5.8实例回读四份真实Camera Profile资产和C++配置验证。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
PREFIX="/DBAPresentationPack_Core/Cameras/Profiles"
ASSETS={"DA_DBA_Camera_Exploration":28.0,"DA_DBA_Camera_CharacterPreview":65.0,
        "DA_DBA_Camera_SkillShowcase":45.0,"DA_DBA_Camera_Arena":35.0}
def main():
    cls=getattr(unreal,"GamePlatformCinematicCameraProfile",None)
    if cls is None: raise RuntimeError("CameraProfile UCLASS未在编辑器加载")
    results=[]
    for name,mm in ASSETS.items():
        path=PREFIX+"/"+name
        asset=unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(asset,cls): raise RuntimeError("DataAsset加载类型失败: "+path)
        focal=float(asset.get_editor_property("focal_length_mm"))
        if abs(focal-mm)>0.05 or not asset.is_valid_profile():
            raise RuntimeError("镜头资产字段回读失败: "+path)
        results.append({"asset":path,"focal_mm":focal})
    out=ROOT/"Saved/Validation/CinematicCamera/ProfilesReload.json"
    record={"engine":str(unreal.SystemLibrary.get_engine_version()),"profiles":results,
            "all_valid":True,"asset_reload_passed":True,"runtime_integration":"pending"}
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CAMERA_PROFILES_RELOAD_OK",json.dumps(record,ensure_ascii=False))
if __name__=="__main__": main()