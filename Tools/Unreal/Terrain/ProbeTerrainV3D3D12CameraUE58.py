#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 十层 V3 地形独立审核地图D3D12相机/RenderTarget（渲染目标）诊断。

只读原生地图与材质绑定，临时SceneCapture2D按实际地形包围盒取景。
比较最终LDR、场景HDR、基础色三种实际UE5.8 RHI渲染模式；落盘输出必须另经像素
有效性审计。无效黑帧仍按失败，不得转称已通过，也不修改正式Village地图。
"""
from __future__ import annotations
import json
import math
from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
EXPECTED_MAT="/DBAWorldPack_Village/Development/Terrain/Production10/MI_DBA_Terrain_Production10_V3_Review"
ROOT=Path(__file__).resolve().parents[3]
DEST=ROOT/"Game/Saved/Validation/TerrainV3D3D12CameraFixed_20261018"

def main()->None:
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("探测必须使用真正SSLM UE5.8正式工程")
    cmd=unreal.SystemLibrary.get_command_line().lower()
    if "-nullrhi" in cmd or "-vulkan" in cmd or "-d3d11" in cmd:
        raise RuntimeError("探测必须使用Win64真正D3D12 RHI，不接受NullRHI/Vulkan/D3D11")
    lib=unreal.EditorAssetLibrary
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not lib.does_asset_exist(MAP) or not editor:
        raise RuntimeError("V3独立审核地图真实资产或编辑器子系统不存在")
    world=unreal.EditorLevelLibrary.get_editor_world()
    # 第一轮GPU图虽然写出，但刚LoadLevel后的同帧抓取全部黑帧；
    # 本轮用UE启动参数先打开地图，使渲染代理和灯光注册在Python执行之前完成。
    if not world or MAP not in world.get_path_name():
        print("SSLM_TERRAIN_GPU_STARTUP_LEVEL_NOT_PRELOADED",world.get_path_name() if world else "none",flush=True)
        raise RuntimeError("D3D12验证必须先由UE编辑器启动阶段打开审核地图，不能同帧LoadLevel和Capture")
    print("SSLM_TERRAIN_GPU_STARTUP_LEVEL_ALREADY_LOADED",world.get_path_name(),flush=True)
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    landscapes=[actor for actor in actors if isinstance(actor,unreal.LandscapeProxy)]
    if len(landscapes)!=1:
        raise RuntimeError("独立地形审核地图必须只有一个Landscape")
    terrain=landscapes[0]
    mat=terrain.get_editor_property("landscape_material")
    if not mat or EXPECTED_MAT not in mat.get_path_name():
        raise RuntimeError("V3审核地形未真正绑定新色材质")
    origin,extent=terrain.get_actor_bounds(only_colliding_components=False)
    print("SSLM_TERRAIN_GPU_PROBE_BOUNDS",json.dumps({
        "origin":[origin.x,origin.y,origin.z],"extent":[extent.x,extent.y,extent.z],
        "actorCount":len(actors),
        "lights":[a.get_actor_label() for a in actors
                  if "Light" in a.get_class().get_name()][:12],
        "material":mat.get_path_name(),
        "rhiCommandLine":cmd[:250],
    },ensure_ascii=False),flush=True)
    if min(extent.x,extent.y)<=100 or extent.x>10000000 or extent.y>10000000:
        raise RuntimeError("真实Landscape包围盒尺寸不合理：不允许编造相机位置")
    distance=max(extent.x,extent.y)
    camera_at=unreal.Vector(origin.x+0.34*distance,origin.y-0.50*distance,
                            origin.z+0.70*distance+max(6000.0,extent.z))
    look=unreal.Vector(origin.x,origin.y,origin.z)
    # UE Python Rotator的三个位置参数实际为Roll/Pitch/Yaw，历史脚本将其误认为
    # Pitch/Yaw/Roll，导致相机背向地形：旧ForwardDot=-0.518，正确应近似1。
    rot=unreal.MathLibrary.find_look_at_rotation(camera_at,look)
    direction=unreal.MathLibrary.get_forward_vector(rot)
    delta=unreal.Vector(look.x-camera_at.x,look.y-camera_at.y,look.z-camera_at.z)
    length=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
    alignment=(direction.x*delta.x+direction.y*delta.y+direction.z*delta.z)/length
    print("SSLM_TERRAIN_V3_CAMERA_ALIGNMENT",round(alignment,6),flush=True)
    if alignment < 0.999:
        raise RuntimeError("实际UE朝向目标地形的Forward向量校验失败")
    # 两次覆盖审核都不允许覆写上一轮PNG或遗失相机/地图运行结果。
    for spec in ("FinalColorLDR","SceneColorHDR","BaseColor"):
        if (DEST/(spec+"_D3D12_1280x720.png")).exists():
            raise FileExistsError("D3D12地形取景探测已存在，拒绝覆盖历史证据："+spec)
    DEST.mkdir(parents=True,exist_ok=True)
    actor=None
    photos=[]
    try:
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,camera_at,rot)
        if not isinstance(actor,unreal.SceneCapture2D):
            raise RuntimeError("引擎不能创建真实SceneCapture2D临时Actor")
        capture=actor.get_editor_property("capture_component2d")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(target,unreal.TextureRenderTarget2D):
            raise RuntimeError("UE没有创建实际RenderTarget2D对象")
        capture.set_editor_property("texture_target",target)
        capture.set_editor_property("fov_angle",63.0)
        capture.set_editor_property("capture_every_frame",False)
        capture.set_editor_property("capture_on_movement",False)
        settings=unreal.PostProcessSettings()
        settings.set_editor_property("override_auto_exposure_method",True)
        settings.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        settings.set_editor_property("override_auto_exposure_bias",True)
        settings.set_editor_property("auto_exposure_bias",0.0)
        settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        settings.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        capture.set_editor_property("post_process_settings",settings)
        capture.set_editor_property("post_process_blend_weight",1.0)
        for label,attr in (("FinalColorLDR","SCS_FINAL_COLOR_LDR"),
                           ("SceneColorHDR","SCS_SCENE_COLOR_HDR"),
                           ("BaseColor","SCS_BASE_COLOR")):
            if not hasattr(unreal.SceneCaptureSource,attr):
                print("SSLM_TERRAIN_GPU_PROBE_SOURCE_NOT_EXPOSED",attr,flush=True)
                continue
            capture.set_editor_property("capture_source",getattr(unreal.SceneCaptureSource,attr))
            capture.capture_scene()
            file=DEST/(label+"_D3D12_1280x720.png")
            unreal.RenderingLibrary.export_render_target(world,target,str(DEST),file.name)
            if not file.is_file():raise RuntimeError("D3D12场景捕获文件没有保存："+label)
            print("SSLM_TERRAIN_GPU_PROBE_FRAME_WRITTEN",label,file.stat().st_size,flush=True)
            photos.append({"source":label,"path":str(file),"bytes":file.stat().st_size})
    finally:
        if actor is not None:unreal.EditorLevelLibrary.destroy_actor(actor)
    (DEST/"RenderProbeManifest.json").write_text(json.dumps({
        "origin":"UE5.8 D3D12 SceneCapture2D",
        "map":MAP,
        "camera": [camera_at.x,camera_at.y,camera_at.z],
        "lookAt":[look.x,look.y,look.z],
        "photos":photos,
        "pixelValidated":False,
        "humanReviewPassed":False,
        "noteZh":"只代表UE实际RHI写出PNG，必须运行真实像素门禁"
    },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_TERRAIN_GPU_PROBE_COMPLETE frames="+str(len(photos)),
          "pixel_gate_still_required=true",flush=True)

if __name__=="__main__":main()
