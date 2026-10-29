# -*- coding: utf-8 -*-
"""SSLM UE5.8 原生独立WaterBodyLake审核关卡：不加载/修改Village Landscape。

由于引擎WaterEditor在存在Landscape时会弹出编辑层对话框，命令行只在无Landscape的新建关卡生成官方水体。
正式Village地图迁入需在交互式编辑器与真实Gaea湖盆导入后进行。
"""
import unreal
import os
import json
import traceback

REVIEW="/Game/Development/Water/L_DBA_WaterBodyLake_Isolated_20261029"
VILLAGE_MAT="/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_Lake"
UNDERWATER_MAT="/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume"
DEST=os.path.join(unreal.Paths.project_saved_dir(),"Validation","Water","NativeLakeIsolatedReport.json")


def main():
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    path=os.path.join(unreal.Paths.project_content_dir(),"Development","Water",
                      "L_DBA_WaterBodyLake_Isolated_20261029.umap")
    surface=unreal.load_asset(VILLAGE_MAT)
    underwater=unreal.load_asset(UNDERWATER_MAT)
    if not surface or not underwater:
        raise RuntimeError("必需的Village湖面或原生水下材质不存在")
    if os.path.isfile(path):
        if not editor.load_level(REVIEW):
            raise RuntimeError("旧审核地图无法加载")
    else:
        if not editor.new_level(REVIEW,False):
            raise RuntimeError("无法创建原生独立审核地图")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or str(world.get_outermost().get_name())!=REVIEW:
        raise RuntimeError("当前地图不符，禁止保存")
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    listing=list(actors.get_all_level_actors())
    if any(isinstance(a,unreal.Landscape) for a in listing):
        raise RuntimeError("独立审核地图内有Landscape，禁止Headless水体创建")
    lakes=[a for a in listing if isinstance(a,unreal.WaterBodyLake)]
    if len(lakes)>1:
        raise RuntimeError("重复湖泊，应人工确定")
    if lakes:
        lake=lakes[0]
    else:
        lake=actors.spawn_actor_from_class(unreal.WaterBodyLake,unreal.Vector(0,0,0))
        if not lake:
            raise RuntimeError("WaterBodyLake原生生成失败")
        lake.set_actor_label("Review_DBA_NativeWaterBodyLake")
    water=lake.get_water_body_component()
    if not water:
        raise RuntimeError("没有WaterBodyComponent")
    water.set_editor_property("affects_landscape",False)
    water.set_water_material(surface)
    water.set_underwater_post_process_material(underwater)
    water.set_editor_property("generate_overlap_events",True)
    settings=water.get_editor_property("underwater_post_process_settings")
    settings.set_editor_property("enabled",True)
    settings.set_editor_property("blend_weight",1.0)
    settings.set_editor_property("priority",1.0)
    settings.set_editor_property("blend_radius",100.0)
    water.set_editor_property("underwater_post_process_settings",settings)

    spline=lake.get_water_spline()
    if not spline:
        raise RuntimeError("WaterSpline不存在")
    points=[(3500,0),(2500,1800),(0,2600),(-2500,1800),
            (-3500,0),(-2500,-1800),(0,-2600),(2500,-1800)]
    spline.clear_spline_points(False)
    for x,y in points:
        spline.add_spline_point(unreal.Vector(x,y,0),unreal.SplineCoordinateSpace.LOCAL,False)
    for i in range(len(points)):
        spline.set_spline_point_type(i,unreal.SplinePointType.CURVE,False)
    spline.set_closed_loop(True,True)
    spline.update_spline()

    zones=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=="WaterZone"]
    if len(zones)>1:
        raise RuntimeError("重复WaterZone")
    if not zones:
        zone=actors.spawn_actor_from_class(unreal.WaterZone,unreal.Vector(0,0,0))
        if not zone:
            raise RuntimeError("不能生成WaterZone")
        zone.set_actor_label("Review_DBA_WaterZone")
        zones=[zone]
    if not editor.save_current_level():
        raise RuntimeError("原生水体审核地图保存失败")
    os.makedirs(os.path.dirname(DEST),exist_ok=True)
    report={
      "sourceVillageUnmodified":True,
      "reviewMap":REVIEW,
      "nativeLakeClass":lake.get_class().get_name(),
      "splinePointCount":int(spline.get_number_of_spline_points()),
      "waterZoneCount":len(zones),
      "material":VILLAGE_MAT,
      "underwaterPostProcess":UNDERWATER_MAT,
      "landscapeCarving":False,
      "status":"UE58 saved, cold reload and visual validation required"
    }
    with open(DEST,"w",encoding="utf-8") as file:
        json.dump(report,file,ensure_ascii=False,indent=2)
    unreal.log("SSLM_NATIVE_LAKE_ISOLATED_SAVED "+REVIEW)


try:
    main()
except Exception:
    unreal.log_error("SSLM_NATIVE_LAKE_ISOLATED_FAILED "+traceback.format_exc())
    raise
