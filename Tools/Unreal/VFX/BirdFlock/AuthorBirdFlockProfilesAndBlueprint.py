#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 三种鸟类配置DataAsset和统一鸟群蓝图生成器。

项目层内容只决定鸟类模型、材质及飞行预设；平台通用类管理实例化更新。
只能在编译过GamePlatformBirdFlockProfile和GamePlatformBirdFlockActor的正式UE编辑器执行。
apply只创建独立BirdFlock目录的新资产，存在同名资产时回读校验，绝不改写其他项目资源。
"""
from __future__ import annotations
import os

MODE=os.getenv("BIRD_FLOCK_PROFILE_MODE","inspect").lower()
ROOT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock"
PROFILES=ROOT+"/Profiles"
MESH=ROOT+"/Meshes"
MATERIAL=ROOT+"/Materials"
BLUEPRINTS=ROOT+"/Blueprints"
SPECIES={
    "Seagull":{"count":32,"max":512,"radius":1700.0,"height":850.0,
               "speed":250.0,"variation":135.0,"wind":80.0,"rate":20.0,
               "draw":24000,"mode":"orbit"},
    "Swallow":{"count":72,"max":512,"radius":1150.0,"height":570.0,
               "speed":340.0,"variation":190.0,"wind":110.0,"rate":24.0,
               "draw":18000,"mode":"figure_eight"},
    "Eagle":{"count":5,"max":80,"radius":2400.0,"height":1400.0,
             "speed":350.0,"variation":220.0,"wind":70.0,"rate":15.0,
             "draw":35000,"mode":"orbit"},
}


def execute():
    if MODE=="inspect":
        for name,settings in SPECIES.items():
            print("BIRD_PROFILE_PLAN",name,settings,
                  PROFILES+"/DA_DBA_BirdFlock_"+name)
        print("BIRD_BLUEPRINT_PLAN",BLUEPRINTS+"/BP_DBA_BirdFlockAmbient")
        return
    if MODE!="apply":
        raise ValueError("只支持inspect或apply")
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止在非正式UE工程创建鸟群资源")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("仅支持锁定UE5.8")

    cls=getattr(unreal,"GamePlatformBirdFlockProfile",None)
    actor=getattr(unreal,"GamePlatformBirdFlockActor",None)
    if cls is None or actor is None:
        raise RuntimeError("客户端鸟群C++反射类型未编译，停止创建DataAsset/Blueprint")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for directory in (PROFILES,BLUEPRINTS):
        if not assets.does_directory_exist(directory):
            if not assets.make_directory(directory):
                raise RuntimeError("无法创建内容目录："+directory)

    mode_enum=getattr(unreal,"GamePlatformBirdFlockFlightMode",None)
    if mode_enum is None:
        raise RuntimeError("鸟群飞行模式UENUM未反射，不能创建预设")
    factory=unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class",cls)

    loaded={}
    for name,settings in SPECIES.items():
        mesh=assets.load_asset(MESH+"/SM_DBA_Bird_"+name)
        material=assets.load_asset(MATERIAL+"/MI_DBA_Bird_"+name)
        if not isinstance(mesh,unreal.StaticMesh) or not isinstance(material,unreal.MaterialInterface):
            raise RuntimeError("鸟群资源缺失："+name)
        path=PROFILES+"/DA_DBA_BirdFlock_"+name
        if assets.does_asset_exist(path):
            data=assets.load_asset(path)
            if not isinstance(data,cls):
                raise RuntimeError("已有目标DataAsset类型异常："+path)
            print("BIRD_PROFILE_REUSED",path)
        else:
            data=tools.create_asset("DA_DBA_BirdFlock_"+name,PROFILES,cls,factory)
            if not isinstance(data,cls):
                raise RuntimeError("DataAsset生成失败："+path)
            data.set_editor_property("display_species_id",unreal.Name(name))
            data.set_editor_property("bird_mesh",mesh)
            data.set_editor_property("bird_material",material)
            data.set_editor_property("default_bird_count",settings["count"])
            data.set_editor_property("max_bird_count",settings["max"])
            data.set_editor_property("radius_cm",settings["radius"])
            data.set_editor_property("flight_height_cm",settings["height"])
            data.set_editor_property("cruise_speed_cm_per_second",settings["speed"])
            data.set_editor_property("height_variation_cm",settings["variation"])
            data.set_editor_property("wind_influence_cm",settings["wind"])
            data.set_editor_property("update_rate_hz",settings["rate"])
            data.set_editor_property("max_draw_distance_cm",settings["draw"])
            enum_value={
                "orbit":mode_enum.ORBIT,
                "traverse":mode_enum.TRAVERSE,
                "figure_eight":mode_enum.FIGURE_EIGHT}[settings["mode"]]
            data.set_editor_property("flight_mode",enum_value)
            if not assets.save_loaded_asset(data,only_if_is_dirty=False):
                raise RuntimeError("DataAsset写盘失败："+path)
            print("BIRD_PROFILE_SAVED",path)
        loaded[name]=data

    target=BLUEPRINTS+"/BP_DBA_BirdFlockAmbient"
    if assets.does_asset_exist(target):
        bp=assets.load_asset(target)
        if not isinstance(bp,unreal.Blueprint):
            raise TypeError("统一蓝图位置已被其他类型资产占用："+target)
        print("BIRD_BLUEPRINT_REUSED",target)
    else:
        creator=unreal.BlueprintFactory()
        creator.set_editor_property("parent_class",actor)
        bp=tools.create_asset("BP_DBA_BirdFlockAmbient",BLUEPRINTS,unreal.Blueprint,creator)
        if not isinstance(bp,unreal.Blueprint):
            raise RuntimeError("鸟群蓝图生成失败")
        # 默认物种是海鸥，其余类型通过实例中BirdProfile直接切换，不建三套播放器。
        bp_class=assets.load_blueprint_class(target)
        if bp_class is None:
            raise RuntimeError("蓝图生成后无法加载父类")
        cdo=unreal.get_default_object(bp_class)
        cdo.set_editor_property("bird_profile",loaded["Seagull"])
        cdo.set_editor_property("bird_count_override",-1)
        if not assets.save_loaded_asset(bp,only_if_is_dirty=False):
            raise RuntimeError("鸟群蓝图保存失败")
        print("BIRD_BLUEPRINT_SAVED",target)

    for name in SPECIES:
        path=PROFILES+"/DA_DBA_BirdFlock_"+name
        if not isinstance(assets.load_asset(path),cls):
            raise RuntimeError("鸟群DataAsset回读失败："+path)
    if assets.load_blueprint_class(target) is None:
        raise RuntimeError("鸟群蓝图回读失败")
    print("BIRD_PROJECT_BLUEPRINTS_OK",len(loaded),1)


if __name__=="__main__":
    execute()
