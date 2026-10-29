# -*- coding: utf-8 -*-
"""SSLM：以正式Village地图只读源制作WaterBodyLake替换审核副本，所有资产由UE5.8生成。

仅在 /Game/Development/Water/L_DBA_VillageWaterBodyReview_20261029 内修改。
不删除旧BP、正式地图Landscape、PlayerStart、PCG、NavMesh，也不改正式Village资产。
"""
import unreal
import math
import os
import json
import traceback

SOURCE = "/DBAWorldPack_Village/Maps/L_Village_Start"
REVIEW = "/Game/Development/Water/L_DBA_VillageWaterBodyReview_20261029"
WATER_MAT = "/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_Lake"
UNDERWATER_MAT = "/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume"
REPORT = os.path.join(unreal.Paths.project_saved_dir(), "Validation", "Water", "VillageWaterBodyReviewCreated.json")


def log(value):
    unreal.log("SSLM_VILLAGE_LAKE_REVIEW " + str(value))


def require_world(level):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or str(world.get_outermost().get_name()) != level:
        raise RuntimeError("关卡不符，拒绝创建或者保存：" +
                           (str(world.get_outermost().get_name()) if world else "<none>"))
    return world


def configure_lake_shape(lake):
    spline = lake.get_water_spline()
    if not spline:
        raise RuntimeError("官方WaterBodyLake无有效WaterSpline")
    # 这里是独立审核地图中的椭圆测试湖岸，不是对正式三岛真实岸线的推断。
    radii = [(3500,0),(2500,1700),(0,2600),(-2300,1750),
             (-3500,0),(-2550,-1700),(0,-2600),(2450,-1650)]
    spline.clear_spline_points(False)
    for x,y in radii:
        spline.add_spline_point(unreal.Vector(x,y,0),unreal.SplineCoordinateSpace.LOCAL,False)
    for index in range(len(radii)):
        spline.set_spline_point_type(index,unreal.SplinePointType.CURVE,False)
    spline.set_closed_loop(True,True)
    spline.update_spline()
    return spline.get_number_of_spline_points()


def configure_water(lake, surface, underwater):
    water = lake.get_water_body_component()
    if not water:
        raise RuntimeError("WaterBodyComponent不存在，拒绝保存")
    # 首先关闭自动雕刻；审核副本也不得修改已有Landscape高度。
    water.set_editor_property("affects_landscape",False)
    water.set_water_material(surface)
    water.set_underwater_post_process_material(underwater)
    water.set_editor_property("generate_overlap_events",True)
    setting = water.get_editor_property("underwater_post_process_settings")
    setting.set_editor_property("enabled",True)
    setting.set_editor_property("blend_weight",1.0)
    setting.set_editor_property("blend_radius",100.0)
    setting.set_editor_property("priority",1.0)
    water.set_editor_property("underwater_post_process_settings",setting)
    return water


def main():
    # UE5.8的EditorAssetLibrary.does_asset_exist明确不支持Level类型。
    # 只能通过正式LevelEditorSubsystem从原图生成测试副本；先读源图以核验其存在。
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor.load_level(SOURCE):
        raise RuntimeError("正式Village地图源无法加载")
    surface=unreal.load_asset(WATER_MAT)
    underwater=unreal.load_asset(UNDERWATER_MAT)
    if not surface or not underwater:
        raise RuntimeError("官方或Village材质缺失，未写入地图")

    # NewLevelFromTemplate是Epic官方API，支持地图模板；EditorAssetLibrary不支持Level类型。
    review_disk=os.path.join(unreal.Paths.project_content_dir(),"Development","Water",
                             "L_DBA_VillageWaterBodyReview_20261029.umap")
    if not os.path.isfile(review_disk):
        if not editor.new_level_from_template(REVIEW,SOURCE):
            raise RuntimeError("UE无法从正式Village地图创建审核副本，正式地图未修改")
        log("已通过UE原生LevelEditorSubsystem创建审核地图：" + REVIEW)
    else:
        log("已有审核副本，复用并检查，禁止覆盖")
        if not editor.load_level(REVIEW):
            raise RuntimeError("无法加载审核副本")
    require_world(REVIEW)
    actor_sys=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors=list(actor_sys.get_all_level_actors())
    lakes=[actor for actor in actors if isinstance(actor,unreal.WaterBodyLake)]
    if len(lakes)>1:
        raise RuntimeError("审核副本内已有多个Lake；拒绝猜测修改哪一个")
    if len(lakes)==1:
        lake=lakes[0]
        log("复用已有WaterBodyLake，不重复创建")
    else:
        # 与正式玩家出生区保持距离；(9000,0,250)仅是隔离审核场景的临时湖泊位置。
        # UE5.8 WaterEditor会自动为Landscape创建Water EditLayer；其默认设置使用Slate模态弹窗。
        # Commandlet无Slate应用，必须仅在本次进程内关闭这项自动弹窗（不SaveConfig）。
        if not hasattr(unreal,"LandscapeSettings"):
            raise RuntimeError("无法读取ULandscapeSettings，拒绝在命令行生成水体")
        landscape_settings=unreal.get_default_object(unreal.LandscapeSettings)
        landscape_settings.set_editor_property("show_dialog_for_automatic_layer_creation",False)
        log("仅当前命令行进程关闭Landscape自动编辑层弹窗")
        lake=actor_sys.spawn_actor_from_class(unreal.WaterBodyLake,unreal.Vector(9000,0,250))
        if not lake:
            raise RuntimeError("原生WaterBodyLake创建失败")
        lake.set_actor_label("Review_DBA_WaterBodyLake")
        log("已创建原生WaterBodyLake")

    water=configure_water(lake,surface,underwater)
    num_points=configure_lake_shape(lake)
    refreshed=list(actor_sys.get_all_level_actors())
    zones=[a for a in refreshed if a.get_class().get_name()=="WaterZone"]
    if len(zones)>1:
        raise RuntimeError("已发现重复WaterZone；拒绝保存需要人工合并的地图")
    if len(zones)==0:
        new_zone=actor_sys.spawn_actor_from_class(unreal.WaterZone,unreal.Vector(9000,0,250))
        if not new_zone:
            raise RuntimeError("无法创建官方WaterZone")
        new_zone.set_actor_label("Review_DBA_WaterZone")
        zones=[new_zone]
    if not editor.save_current_level():
        raise RuntimeError("UE5.8审核地图保存失败")

    report={
        "sourceMapUnmodified":SOURCE,"reviewMap":REVIEW,
        "status":"saved_by_unreal_editor_validation_pending",
        "waterBodyClass":str(lake.get_class().get_name()),
        "waterBodyLabel":str(lake.get_actor_label()),
        "lakePositionCm":[float(lake.get_actor_location().x),
                          float(lake.get_actor_location().y),
                          float(lake.get_actor_location().z)],
        "splinePoints":num_points,
        "waterZoneCount":len(zones),
        "lakeMaterial":WATER_MAT,"underwaterPostProcess":UNDERWATER_MAT,
        "landscapeCarving":False,
        "warning":"审核地图没有正式Gaea三岛高程；Spline为临时测试轮廓，不能直接迁入正式Village"
    }
    os.makedirs(os.path.dirname(REPORT),exist_ok=True)
    with open(REPORT,"w",encoding="utf-8") as output:
        json.dump(report,output,ensure_ascii=False,indent=2)
    log("SSLM_VILLAGE_LAKE_REVIEW_SAVED " + REVIEW + " zone=" + str(len(zones)))


try:
    main()
except Exception:
    unreal.log_error("SSLM_VILLAGE_LAKE_REVIEW_FAILED " + traceback.format_exc())
    raise
