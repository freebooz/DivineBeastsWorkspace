# -*- coding: utf-8 -*-
"""《神兽联盟》自然飘落/漂浮区域项目数据资产（UE5.8官方资产编辑器生成）。

区域只持有AreaKey、样式权重、风强与高度，不嵌入Niagara或全局播放器；
地图/PCG如何绑定AreaKey由项目客户端表现层负责，不得用区域候选配置冒充已运行。
"""
from __future__ import annotations
import json
import unreal
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
SRC=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
AREA=json.loads((SRC/"AmbientNatureAreaProfiles.json").read_text(encoding="utf8"))
STYLES=json.loads((SRC/"AmbientNatureStyleCatalog.json").read_text(encoding="utf8"))
DEST="/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Regions"
OUT=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/UERegionProfileAuthoring.json"
ASSETS=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
state={"engine":str(unreal.SystemLibrary.get_engine_version()),
       "project":str(unreal.Paths.get_project_file_path()),
       "created":[],"existing":[],"errors":[],"plannedAreas":len(AREA["areas"])}

def build_record(area):
    key=area["areaKey"]
    area_profile=AREA["profileDefinitions"][area["spawnProfile"]]
    name="DA_DBA_AmbientRegion_"+key.replace(".","_")
    path=DEST+"/"+name
    if ASSETS.does_asset_exist(path):
        obj=ASSETS.load_asset(path)
        if not isinstance(obj,unreal.DivineBeastsAmbientRegionProfile):
            raise TypeError("已有同名资产类型不符："+path)
        if str(obj.get_editor_property("area_key"))!=key:
            raise RuntimeError("已有同名区域定义属于其他区域："+path)
        state["existing"].append(path)
        return
    factory=unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class",unreal.DivineBeastsAmbientRegionProfile)
    profile=TOOLS.create_asset(name,DEST,unreal.DivineBeastsAmbientRegionProfile,factory)
    if not isinstance(profile,unreal.DivineBeastsAmbientRegionProfile):
        raise RuntimeError("无法创建真实区域DataAsset："+path)
    profile.set_editor_property("area_key",unreal.Name(key))
    profile.set_editor_property("label_zh",str(area["labelZh"]))
    heights=area_profile["spawnHeightCm"]
    profile.set_editor_property("spawn_height_min_cm",float(heights[0]))
    profile.set_editor_property("spawn_height_max_cm",float(heights[1]))
    profile.set_editor_property("wind_strength",float(area_profile["wind"]))
    style_weights=[]
    total=0.0
    for entry in area["styles"]:
        if entry["styleId"] not in {x["styleId"] for x in STYLES["styles"]}:
            raise RuntimeError("区域引用未登记的风格："+entry["styleId"])
        weight=float(entry["weight"])
        if weight<=0.0:
            raise RuntimeError("区域风格权重必须大于0："+key)
        data=unreal.DivineBeastsAmbientStyleWeight()
        data.set_editor_property("style_id",unreal.Name(entry["styleId"]))
        source_style=next(x for x in STYLES["styles"] if x["styleId"]==entry["styleId"])
        data.set_editor_property("definition_id",unreal.Name(source_style["definitionId"]))
        data.set_editor_property("weight",weight)
        style_weights.append(data)
        total+=weight
    if abs(total-1.0)>0.0001:
        raise RuntimeError("区域权重总和必须为1："+key)
    profile.set_editor_property("styles",style_weights)
    if not ASSETS.save_loaded_asset(profile,only_if_is_dirty=False):
        raise RuntimeError("区域DataAsset保存失败："+path)
    verify=ASSETS.load_asset(path)
    if not isinstance(verify,unreal.DivineBeastsAmbientRegionProfile):
        raise RuntimeError("区域DataAsset回读失败："+path)
    state["created"].append({"path":path,"areaKey":key,"styles":len(style_weights)})
    print("AMBIENT_REGION_DATA_ASSET_SAVED",path,flush=True)

def main():
    if not state["project"].replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("必须在锁定的神兽联盟工程创建DataAsset")
    if not state["engine"].startswith("5.8."):
        raise RuntimeError("要求UE5.8")
    if not hasattr(unreal,"DivineBeastsAmbientRegionProfile"):
        raise RuntimeError("当前Editor DLL尚未包含新增项目区域Profile类型；需在安全重启后完成Editor目标编译/加载")
    if not ASSETS.does_directory_exist(DEST):ASSETS.make_directory(DEST)
    try:
        for region in AREA["areas"]:
            build_record(region)
        state["success"]=True
    except Exception as exc:
        state["success"]=False
        state["errors"].append(str(exc))
        raise
    finally:
        OUT.parent.mkdir(parents=True,exist_ok=True)
        OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
        print("AMBIENT_REGION_AUTHORING_REPORT",OUT,
              "created",len(state["created"]),"existing",len(state["existing"]),
              "success",state.get("success"),flush=True)

if __name__=="__main__":
    main()
