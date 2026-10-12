#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8独立十层Landscape六态真GPU审核采样；不修改正式Village，也不保存临时Actor。

模式inspect（默认）只输出计划；apply仅在GPU RHI和真实审查地图可用时执行。
SceneCapture2D导出原始PNG，随后应调用ValidateTerrainProductionGPUFrames.py对像素
亮度、两两差异和分辨率检查。无效黑帧和NullRHI不得记为成功，更不替代真人美术审核。
"""
from __future__ import annotations
import json
import math
import os
from pathlib import Path

MODE=os.environ.get("TERRAIN_PBR_GPU_REVIEW_MODE","inspect").lower().strip()
MAP="/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview"
MPC="/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal"
MI="/DBAPresentationPack_Core/Materials/Terrain/MI_DBA_Terrain_Production10_Review"
# 名称 / 雪量 / 湿润度 / 全局苔藓影响：重用GamePlatformSurface唯一MPC。
STATES=(
    ("Dry",0.0,0.0,0.0),
    ("Moss",0.0,0.0,1.0),
    ("Wet",0.0,0.75,0.5),
    ("LightSnow",0.23,0.1,1.0),
    ("HeavySnow",0.85,0.3,1.0),
    ("AfterSnow",0.12,0.55,0.8),
)
PARAMS={
    "snow":"GP_Surface_GlobalSnowAmount",
    "wet":"GP_Surface_GlobalWetness",
    "moss":"GP_Surface_GlobalMossInfluence"
}
OUTPUT_NAME="TerrainPBR_GPU_20261012"
TERRAIN_LABEL="TerrainCalibration_HeightR16_Weight10"

def execute():
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("TERRAIN_PBR_GPU_REVIEW_MODE只能inspect/apply/verify")
    print("TERRAIN_PBR_GPU_PLAN",MODE,MAP,"weather_states",len(STATES))
    if MODE=="inspect":return
    try:import unreal # type: ignore
    except ImportError as ex:raise RuntimeError("只能在UE5.8 Editor/RHI完成审核") from ex
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE未加载神兽联盟正式工程")
    output=Path(project).resolve().parent/"Saved/Validation"/OUTPUT_NAME
    paths={n:output/(n+"_D3D12_1280x720.png") for n,_,_,_ in STATES}
    if MODE=="verify":
        for name,path in paths.items():
            if not path.is_file() or path.stat().st_size<30_000:
                raise RuntimeError("真正UE RHI图像不存在或异常小："+str(path))
        print("TERRAIN_PBR_GPU_FILES_PRESENT_NOT_PIXEL_VALIDATED",len(paths))
        return
    for f in paths.values():
        if f.exists():raise FileExistsError("已有历史审核证据不允许覆盖："+str(f))
    if "-nullrhi" in unreal.SystemLibrary.get_command_line().lower():
        raise RuntimeError("NullRHI不会提供真实GPU结果，禁止捕获")
    lib=unreal.EditorAssetLibrary
    for path in (MAP,MPC,MI):
        if not lib.does_asset_exist(path):raise FileNotFoundError("真实审查资源不存在："+path)
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not editor.load_level(MAP):
        raise RuntimeError("UE Editor无法真实加载十层Landscape审核关卡")
    world=unreal.EditorLevelLibrary.get_editor_world()
    collection=lib.load_asset(MPC)
    if not isinstance(world,unreal.World) or not isinstance(collection,unreal.MaterialParameterCollection):
        raise RuntimeError("RHI审查世界/MPC真实反射类错误")
    landscape=[x for x in unreal.EditorLevelLibrary.get_all_level_actors()
               if isinstance(x,unreal.Landscape)]
    if len(landscape)!=1:raise RuntimeError("十层Landscape真实Actor数量不等于一")
    live_material=landscape[0].get_editor_property("landscape_material")
    if not live_material or not (MI.rsplit("/",1)[1] in live_material.get_path_name()):
        raise RuntimeError("审核Landscape未绑定正式项目PBR MI")
    from_pt=unreal.Vector(15000,-28000,65000)
    to_pt=unreal.Vector(0,0,5000)
    dx,dy,dz=to_pt.x-from_pt.x,to_pt.y-from_pt.y,to_pt.z-from_pt.z
    rot=unreal.Rotator(
        math.degrees(math.atan2(dz,math.hypot(dx,dy))),
        math.degrees(math.atan2(dy,dx)),0)
    actor=None
    output.mkdir(parents=True,exist_ok=True)
    try:
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D,from_pt,rot)
        if not isinstance(actor,unreal.SceneCapture2D):
            raise RuntimeError("SceneCapture2D真实Actor创建失败")
        actor.set_actor_label("TerrainPBR_TemporaryD3D12Capture_DoNotSave")
        capture=actor.get_editor_property("capture_component2d")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(capture,unreal.SceneCaptureComponent2D) or not isinstance(
                target,unreal.TextureRenderTarget2D):
            raise RuntimeError("真实RenderTarget纹理或捕获组件反射失败")
        capture.set_editor_property("fov_angle",67.0)
        capture.set_editor_property("texture_target",target)
        capture.set_editor_property("capture_every_frame",False)
        capture.set_editor_property("capture_on_movement",False)
        capture.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        for name,snow,wet,moss in STATES:
            for parameter,value in ((PARAMS["snow"],snow),(PARAMS["wet"],wet),
                                    (PARAMS["moss"],moss)):
                unreal.MaterialLibrary.set_scalar_parameter_value(
                    world,collection,unreal.Name(parameter),value)
            capture.capture_scene()
            unreal.RenderingLibrary.export_render_target(
                world,target,str(output),paths[name].name)
            if not paths[name].is_file() or paths[name].stat().st_size<30_000:
                raise RuntimeError("UE实际GPU审核帧缺失/不合格："+str(paths[name]))
            print("TERRAIN_PBR_GPU_RAW_CAPTURE",name,paths[name].stat().st_size)
    finally:
        if world and collection:
            for parameter,reset in ((PARAMS["snow"],0.0),(PARAMS["wet"],0.0),
                                    (PARAMS["moss"],1.0)):
                unreal.MaterialLibrary.set_scalar_parameter_value(
                    world,collection,unreal.Name(parameter),reset)
        if actor:unreal.EditorLevelLibrary.destroy_actor(actor)
    evidence=output/"TerrainPBR_GPU_Evidence.json"
    if evidence.exists():raise FileExistsError("已存在同名场景GPU审核结果，不允许覆盖")
    evidence.write_text(json.dumps({
        "origin":"UE5.8 SceneCapture2D",
        "landscape":MAP,"materialInstance":MI,"mpc":MPC,
        "frames":[{"name":name,"snow":snow,"wet":wet,"moss":moss,
                   "file":str(paths[name]),"bytes":paths[name].stat().st_size}
                  for name,snow,wet,moss in STATES],
        "scopeZh":"仅代表尝试保存真实RHI帧；像素有效性/性能/用户人工审美独立验收"
    },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("TERRAIN_PBR_GPU_CAPTURES_WRITTEN_PIXELS_STILL_NEED_VALIDATION",len(STATES))

if __name__=="__main__":execute()
