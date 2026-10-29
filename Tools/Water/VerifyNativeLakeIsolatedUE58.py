# -*- coding: utf-8 -*-
"""SSLM：独立水体审核关卡冷启动回读校验，不写入任何UE资产。"""
import unreal
import os
import json
import traceback

REVIEW = "/Game/Development/Water/L_DBA_WaterBodyLake_Isolated_20261029"
EXPECTED_MAT = "/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_Lake"
EXPECTED_PP = "/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume"
OUT=os.path.join(unreal.Paths.project_saved_dir(),"Validation","Water","NativeLakeIsolatedColdReload.json")


def main():
    ed=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not ed.load_level(REVIEW):
        raise RuntimeError("审核地图冷加载失败")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if str(world.get_outermost().get_name())!=REVIEW:
        raise RuntimeError("地图身份不符")
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    lakes=[a for a in actors if isinstance(a,unreal.WaterBodyLake)]
    zones=[a for a in actors if a.get_class().get_name()=="WaterZone"]
    if len(lakes)!=1 or len(zones)!=1:
        raise RuntimeError("预期唯一Lake和Zone，实际 Lake={} Zone={}".format(len(lakes),len(zones)))
    comp=lakes[0].get_water_body_component()
    watermat=comp.get_editor_property("water_material")
    pp=comp.get_editor_property("underwater_post_process_material")
    if not watermat or not pp:
        raise RuntimeError("水面或水下后处理材质在保存后丢失")
    if str(watermat.get_path_name())!=EXPECTED_MAT or str(pp.get_path_name())!=EXPECTED_PP:
        raise RuntimeError("回读材质路径不符")
    settings=comp.get_editor_property("underwater_post_process_settings")
    if not settings.get_editor_property("enabled") or settings.get_editor_property("blend_weight") < 0.9:
        raise RuntimeError("保存后水下PostProcess未启用")
    if comp.get_editor_property("affects_landscape"):
        raise RuntimeError("不允许测试湖泊修改Landscape")
    spline=lakes[0].get_water_spline()
    points=spline.get_number_of_spline_points()
    if points!=8:
        raise RuntimeError("水面Spline编辑未持久化，实际数量="+str(points))
    state={"result":"PASS","map":REVIEW,"nativeLakeCount":1,"zoneCount":1,
           "splinePointCount":int(points),"material":str(watermat.get_path_name()),
           "underwaterMaterial":str(pp.get_path_name()),
           "postProcessEnabled":True,"landscapeCarving":False}
    os.makedirs(os.path.dirname(OUT),exist_ok=True)
    with open(OUT,"w",encoding="utf-8") as f: json.dump(state,f,ensure_ascii=False,indent=2)
    unreal.log("SSLM_NATIVE_LAKE_COLD_RELOAD_PASS "+REVIEW)


try:
    main()
except Exception:
    unreal.log_error("SSLM_NATIVE_LAKE_COLD_RELOAD_FAIL "+traceback.format_exc())
    raise
