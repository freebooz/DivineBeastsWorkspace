#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》P1湖泊真实感审核：在真实UE5.8编辑器中创建/复核独立材质和测试地图。

默认inspect只读；materials新建两份材质实例；material_verify检查原生保存；
map_copy独立复制审核地图；map_bind绑定复制地图；map_verify在新进程回读；
capture在真实RHI获取原版/清澈/深湖多景别，不保存地图。
只创建/修改专属开发审核资源，不修改GamePlatform基础母材质、正式Village或服务器水域。
光学参数是未经人工审批的视觉候选，不能称为实测物理系数或性能验收通过。
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODE = os.environ.get("SSLM_WATER_P1_MODE", "inspect").lower().strip()
ORIGINAL = "/Game/Development/Water/L_GP_WaterReview"
REVIEW = "/Game/Development/Water/Maps/L_SSLM_LakePhotorealP1"
NATIVE = "/GamePlatformSurface/Materials/Water/MI_GP_Lake_Native"
FOLDER = "/Game/Development/Water/Materials"
CLEAR = FOLDER + "/MI_SSLM_LakeClear_P1"
DEEP = FOLDER + "/MI_SSLM_LakeDeep_P1"
SOURCE_FILE = ROOT / "Game/Content/Development/Water/L_GP_WaterReview.umap"
REVIEW_FILE = ROOT / "Game/Content/Development/Water/Maps/L_SSLM_LakePhotorealP1.umap"
OUTPUT = ROOT / "Saved/Validation/WaterLakePhotorealP1_20261024"
MODES = {"inspect","materials","material_verify","map_copy","map_bind","map_verify","capture"}
# 这些系数属于现有Water原生材质的专有标尺，不能直接转译成米制水光系数。
DEFINITIONS = {
  CLEAR:{
    "scalars":{"River Normal Flatness":0.70,"Foam Opacity":0.45,"Caustics Brightness Adjust":0.88},
    "vectors":{"Water Albedo":(0.85,0.91,0.86,0.50),"Scattering":(0.85,0.96,0.90,0.50),"Absorption":(13.0,145.0,315.0,8.0)}},
  DEEP:{
    "scalars":{"River Normal Flatness":0.62,"Foam Opacity":0.34,"Caustics Brightness Adjust":0.70},
    "vectors":{"Water Albedo":(0.77,0.87,0.88,0.50),"Scattering":(0.79,0.90,0.96,0.50),"Absorption":(16.0,135.0,282.0,8.0)}},
}
def digest(file:Path)->str:
    h=hashlib.sha256()
    with file.open("rb") as fp:
        for b in iter(lambda:fp.read(1<<20),b""): h.update(b)
    return h.hexdigest()
def project_guard(unreal):
    name=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not name.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("非当前神兽联盟正式UE工程，拒绝修改")
def preflight(unreal):
    assets=unreal.EditorAssetLibrary
    parent=assets.load_asset(NATIVE)
    if not isinstance(parent,unreal.MaterialInstanceConstant):raise RuntimeError("原生湖面MI缺失")
    original=parent.get_editor_property("parent")
    if "/Water/Materials/WaterSurface/Water_Material_Lake" not in str(original.get_path_name()):
        raise RuntimeError("Water母材质继承关系改变，不安全")
    edit=unreal.MaterialEditingLibrary
    for rule in DEFINITIONS.values():
        if not set(rule["scalars"]) <= set(map(str,edit.get_scalar_parameter_names(parent))):
            raise RuntimeError("湖面标量合同不匹配")
        if not set(rule["vectors"]) <= set(map(str,edit.get_vector_parameter_names(parent))):
            raise RuntimeError("湖面向量合同不匹配")
    return parent
def verify_materials(unreal):
    parent=preflight(unreal)
    assets=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    for path,rules in DEFINITIONS.items():
        obj=assets.load_asset(path)
        if not isinstance(obj,unreal.MaterialInstanceConstant) or obj.get_editor_property("parent")!=parent:
            raise RuntimeError("P1审核材质实例不存在或继承关系错误："+path)
        for name,want in rules["scalars"].items():
            got=float(edit.get_material_instance_scalar_parameter_value(obj,name))
            if abs(got-want)>1e-3: raise RuntimeError("标量保存回读错误："+name)
        for name,want in rules["vectors"].items():
            val=edit.get_material_instance_vector_parameter_value(obj,name)
            if max(abs(a-b) for a,b in zip((val.r,val.g,val.b,val.a),want))>1e-3:
                raise RuntimeError("向量保存回读错误："+name)
        print("SSLM_WATER_P1_MATERIAL_VERIFIED",path,flush=True)
def create_materials(unreal):
    assets=unreal.EditorAssetLibrary; edit=unreal.MaterialEditingLibrary
    parent=preflight(unreal)
    if any(assets.does_asset_exist(x) for x in DEFINITIONS):
        # 只能按原身份回读已有资产，不覆盖曾经审核的材质。
        verify_materials(unreal); return
    if not assets.does_directory_exist(FOLDER):assets.make_directory(FOLDER)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for path,rules in DEFINITIONS.items():
        name=path.rsplit("/",1)[-1]
        mi=tools.create_asset(name,FOLDER,unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):raise RuntimeError("无法创建原生材质："+path)
        edit.set_material_instance_parent(mi,parent)
        for parameter,value in rules["scalars"].items():
            edit.set_material_instance_scalar_parameter_value(mi,parameter,value)
        for parameter,rgba in rules["vectors"].items():
            edit.set_material_instance_vector_parameter_value(mi,parameter,unreal.LinearColor(*rgba))
        edit.update_material_instance(mi)
        if not assets.save_loaded_asset(mi,only_if_is_dirty=False):raise RuntimeError("原生MI保存失败："+path)
        print("SSLM_WATER_P1_MATERIAL_SAVED",path,flush=True)
    verify_materials(unreal)
def lake(unreal):
    ls=[a for a in unreal.EditorLevelLibrary.get_all_level_actors()
        if a.get_class().get_name()=="WaterBodyLake"]
    if len(ls)!=1:raise RuntimeError("审核图必须存在唯一原生湖泊："+str(len(ls)))
    cs=list(ls[0].get_components_by_class(unreal.WaterBodyLakeComponent))
    if len(cs)!=1:raise RuntimeError("湖泊WaterBodyLakeComponent数量错误")
    return cs[0]
def map_copy(unreal):
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(ORIGINAL) or not SOURCE_FILE.is_file():raise FileNotFoundError(ORIGINAL)
    if assets.does_asset_exist(REVIEW) or REVIEW_FILE.is_file():raise FileExistsError(REVIEW)
    old=digest(SOURCE_FILE)
    dest_folder=REVIEW.rsplit("/",1)[0]
    if not assets.does_directory_exist(dest_folder):assets.make_directory(dest_folder)
    new=assets.duplicate_asset(ORIGINAL,REVIEW)
    if not isinstance(new,unreal.World) or not assets.save_loaded_asset(new,only_if_is_dirty=False):
        raise RuntimeError("独立审核地图UE原生复制/保存失败")
    if digest(SOURCE_FILE)!=old:raise RuntimeError("复制时更改了原审核地图")
    print("SSLM_WATER_P1_MAP_COPIED",REVIEW,flush=True)
def map_bind(unreal):
    verify_materials(unreal)
    if not REVIEW_FILE.is_file():raise FileNotFoundError(REVIEW)
    original_hash=digest(SOURCE_FILE)
    world=unreal.EditorLoadingAndSavingUtils.load_map(REVIEW)
    if not world:raise RuntimeError("P1复制地图UE加载失败")
    component=lake(unreal)
    mi=unreal.EditorAssetLibrary.load_asset(CLEAR)
    if component.get_water_material()!=mi:
        component.set_water_material(mi)
        if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError("UE原生地图保存失败")
    if digest(SOURCE_FILE)!=original_hash:raise RuntimeError("原审核地图被意外改写")
    print("SSLM_WATER_P1_MAP_BOUND",REVIEW,CLEAR,flush=True)
def map_verify(unreal):
    if not REVIEW_FILE.is_file():raise FileNotFoundError(REVIEW)
    world=unreal.EditorLoadingAndSavingUtils.load_map(REVIEW)
    if not world:raise RuntimeError("P1审核地图未能冷重载")
    component=lake(unreal)
    if component.get_water_material()!=unreal.EditorAssetLibrary.load_asset(CLEAR):
        raise RuntimeError("复制地图湖面材质未成功保存")
    actors=list(unreal.EditorLevelLibrary.get_all_level_actors())
    if len(actors)<20:raise RuntimeError("P1地图缺少原水体与审核对象")
    print("SSLM_WATER_P1_MAP_VERIFIED",REVIEW,len(actors),flush=True)
def capture(unreal):
    """渲染原始/清澈/深湖九张画面；只临时改变复制地图内存对象，绝不保存关卡。"""
    if "-nullrhi" in str(unreal.SystemLibrary.get_command_line()).lower():
        raise RuntimeError("禁止把NullRHI作为真实截图")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not world or REVIEW not in str(world.get_path_name()):
        raise RuntimeError("必须在UE进程启动参数中预加载已保存P1地图")
    source_hash=digest(SOURCE_FILE); review_hash=digest(REVIEW_FILE)
    if OUTPUT.exists() and (list(OUTPUT.glob("*.png")) or (OUTPUT/"CaptureEvidence.json").exists()):
        raise FileExistsError("已有审核图及证据，禁止覆盖")
    OUTPUT.mkdir(parents=True,exist_ok=True)
    component=lake(unreal)
    assets=unreal.EditorAssetLibrary
    variants={"Baseline":assets.load_asset(NATIVE),"Clear":assets.load_asset(CLEAR),
              "Deep":assets.load_asset(DEEP)}
    cams=[a for a in unreal.EditorLevelLibrary.get_all_level_actors()
          if a.get_class().get_name()=="CameraActor" and any(
          v in a.get_actor_label() for v in ("Overview","Shore","Waterfall"))]
    if len(cams)!=3:raise RuntimeError("原生审核镜头不全")
    records=[]
    for variant,material in variants.items():
        component.set_water_material(material)
        for camera in cams:
            role=next(key for key in ("Overview","Shore","Waterfall") if key in camera.get_actor_label())
            spawned=None
            try:
                spawned=unreal.EditorLevelLibrary.spawn_actor_from_class(
                    unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
                comp=spawned.get_editor_property("capture_component2d")
                rt=unreal.RenderingLibrary.create_render_target2d(
                    world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
                    unreal.LinearColor(0,0,0,1),False,False)
                comp.set_editor_property("texture_target",rt)
                comp.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
                comp.set_editor_property("capture_every_frame",False)
                comp.set_editor_property("capture_on_movement",False)
                comp.set_editor_property("fov_angle",55.0)
                comp.capture_scene()
                filename="SSLM_Lake_"+variant+"_"+role+"_D3D12.png"
                unreal.RenderingLibrary.export_render_target(world,rt,str(OUTPUT),filename)
                png=OUTPUT/filename
                if not png.is_file() or png.stat().st_size<12000:
                    raise RuntimeError("GPU真实截图文件为空："+filename)
                records.append({"variant":variant,"view":role,"name":filename,
                                "bytes":png.stat().st_size,"sha256":digest(png)})
                print("SSLM_WATER_P1_GPU_FRAME",filename,png.stat().st_size,flush=True)
            finally:
                if spawned is not None:unreal.EditorLevelLibrary.destroy_actor(spawned)
    component.set_water_material(variants["Clear"])
    if digest(SOURCE_FILE)!=source_hash or digest(REVIEW_FILE)!=review_hash:
        raise RuntimeError("取图时原始地图或P1地图被意外保存")
    data={"sourceMap":ORIGINAL,"reviewMap":REVIEW,"originalSHA256":source_hash,
          "reviewSHA256":review_hash,"frames":records,"realRHI":True,
          "pixelValidated":False,"gpuBudgetValidated":False,"humanApproved":False,
          "noteZh":"所有数值仅为P1艺术候选；未经像素检测、性能实测或人工批准"}
    (OUTPUT/"CaptureEvidence.json").write_text(
        json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_WATER_P1_CAPTURE_COMPLETE",len(records),flush=True)
def main():
    if MODE not in MODES:raise ValueError("不支持的模式："+MODE)
    if MODE=="inspect":
        print("SSLM_WATER_P1_PLAN",ORIGINAL,REVIEW,CLEAR,DEEP,"NoFormalVillageChange")
        return
    import unreal
    project_guard(unreal)
    if MODE=="materials":create_materials(unreal)
    elif MODE=="material_verify":verify_materials(unreal)
    elif MODE=="map_copy":map_copy(unreal)
    elif MODE=="map_bind":map_bind(unreal)
    elif MODE=="map_verify":map_verify(unreal)
    elif MODE=="capture":capture(unreal)
if __name__=="__main__":main()
