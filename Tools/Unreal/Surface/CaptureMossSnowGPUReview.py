#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""独立苔藓/积雪审核关卡三态真实GPU场景捕获（不修改正式Village）。

仅在已启用D3D图形RHI的真正UE5.8编辑器执行：暂时打开Review地图，
给Surface MPC设置晴天/小雪/大雪三种只读视觉状态并用SceneCapture2D
输出原生1280x720 PNG。NULL RHI、黑图、地图路径/资源缺失或写盘失败都拒绝通过。
SceneCapture是临时Actor，finally还原全局MPC并销毁，不保存.umap。
默认inspect仅说明方案，MOSS_SNOW_GPU_MODE=apply才捕获。
截图仅为程序化视觉证据，最终人工审核与真实GPU帧时仍独立进行。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

MODE = os.environ.get("MOSS_SNOW_GPU_MODE", "inspect").strip().lower()
MAP = "/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview"
MPC = "/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal"
MI = "/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff"
STATES = (("Clear", 0.0, 0.0), ("LightSnow", .23, .12),
          ("HeavySnow", .85, .3))


def run() -> None:
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("MOSS_SNOW_GPU_MODE只能inspect/apply/verify")
    print("MOSS_SNOW_GPU_PLAN",MAP,STATES)
    if MODE == "inspect":
        return
    import unreal

    project = unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("真实渲染审核必须在神兽联盟唯一正式工程")
    # 不覆盖上一轮GPU失败截图；目录名受控，防止穿越工作空间。
    import re
    folder_name=os.environ.get("MOSS_SNOW_GPU_OUTPUT_NAME","MossSnowVisual_20261012")
    if not re.fullmatch(r"MossSnowVisual_[A-Za-z0-9_]{5,60}",folder_name):
        raise ValueError("GPU审核输出目录名必须为受控ASCII身份")
    output = Path(project).resolve().parent / "Saved/Validation" / folder_name
    names = {name: output / ("MossSnow_" + name + "_GPU_1280x720.png")
             for name,_,_ in STATES}
    if MODE == "verify":
        return verify_files(names,output,write_report=False)

    # UI编辑器加载正式演示地图但不写入原图；不调用NullRHI或复用旧截图。
    assets=unreal.EditorAssetLibrary
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game","/GamePlatformSurface",
                                     "/DBAPresentationPack_Core"],force_rescan=True)
    for path in (MAP,MPC,MI):
        if not assets.does_asset_exist(path):
            raise FileNotFoundError("GPU审核前置UE资产缺失："+path)
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor:
        raise RuntimeError("当前UE进程没有完整的LevelEditorSubsystem")
    world=unreal.EditorLevelLibrary.get_editor_world()
    # GUI预热进程已加载目标图时不重复卸载/加载，以保留Shader编译和纹理流送进度。
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        if not editor.load_level(MAP):
            raise RuntimeError("UE无法加载雪苔独立审核地图")
        world=unreal.EditorLevelLibrary.get_editor_world()
    collection=assets.load_asset(MPC)
    # 先确认审核场景确有5种不同几何对象，防止空场景照样输出三张图片。
    actor_labels={actor.get_actor_label() for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
    required_labels={"MossSnowReview_Ground","MossSnowReview_RockFlat",
                     "MossSnowReview_Slope25","MossSnowReview_Slope50",
                     "MossSnowReview_CliffWall"}
    if not required_labels.issubset(actor_labels):
        raise RuntimeError("独立场景缺少平台岩石审核网格："+str(required_labels-actor_labels))
    if not isinstance(world,unreal.World) or not isinstance(
        collection,unreal.MaterialParameterCollection):
        raise TypeError("GPU审核World/MPC真实类型不符")

    # GPU结果必须由运行中的编辑器RHI输出；不读取伪造的PNG。
    existing=[str(path) for path in names.values() if path.exists()]
    if existing: raise FileExistsError("已有审核图片禁止覆盖："+str(existing))
    output.mkdir(parents=True,exist_ok=True)

    from_location=unreal.Vector(100.0,-1950.0,730.0)
    to_location=unreal.Vector(-50.0,-20.0,180.0)
    # 部分UE5.8命令行反射环境未暴露KismetMathLibrary；按空间向量用标准数学求旋转。
    import math
    dx=to_location.x-from_location.x
    dy=to_location.y-from_location.y
    dz=to_location.z-from_location.z
    yaw=math.degrees(math.atan2(dy,dx))
    pitch=math.degrees(math.atan2(dz,math.sqrt(dx*dx+dy*dy)))
    rotation=unreal.Rotator(pitch,yaw,0.0)
    component=None
    actor=None
    target=None
    try:
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,from_location,rotation)
        if not isinstance(actor,unreal.SceneCapture2D):
            raise RuntimeError("无法创建真正SceneCapture2D临时Actor")
        actor.set_actor_label("MossSnowReview_TemporaryCapture_NOT_SAVED")
        component=actor.get_editor_property("capture_component2d")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(component,unreal.SceneCaptureComponent2D) or not isinstance(
            target,unreal.TextureRenderTarget2D):
            raise RuntimeError("引擎真实GPU捕获组件/RenderTarget2D初始化失败")
        component.set_editor_property("fov_angle",62.0)
        component.set_editor_property("texture_target",target)
        component.set_editor_property("capture_every_frame",False)
        component.set_editor_property("capture_on_movement",False)
        component.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        # 固定相机曝光，避免SkyAtmosphere/雾的粉橙自动曝光掩盖真实PBR纹理。
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",-2.0)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        component.set_editor_property("post_process_settings",post)
        component.set_editor_property("post_process_blend_weight",1.0)
        # 只影响临时捕获组件，不改真实天气地图Sky、Fog或角色视角。
        flags=[]
        for flag in ("Fog","Atmosphere","VolumetricFog","MotionBlur"):
            entry=unreal.EngineShowFlagsSetting()
            entry.set_editor_property("show_flag_name",flag)
            entry.set_editor_property("enabled",False)
            flags.append(entry)
        component.set_editor_property("show_flag_settings",flags)
        for name,snow,wet in STATES:
            for parameter,value in (
                ("GP_Surface_GlobalSnowAmount",snow),
                ("GP_Surface_GlobalWetness",wet),
                ("GP_Surface_GlobalMossInfluence",1.0)):
                unreal.MaterialLibrary.set_scalar_parameter_value(
                    world,collection,unreal.Name(parameter),value)
            component.capture_scene()
            unreal.RenderingLibrary.export_render_target(
                world,target,str(output),names[name].name)
            path=names[name]
            if not path.is_file() or path.stat().st_size < 30000:
                raise RuntimeError("RHI未写出有效独立审核图："+str(path))
            print("MOSS_SNOW_RENDERED_FRAME",name,path,path.stat().st_size)
    finally:
        if world and collection:
            for param,val in (("GP_Surface_GlobalSnowAmount",0.0),
                              ("GP_Surface_GlobalWetness",0.0),
                              ("GP_Surface_GlobalMossInfluence",1.0)):
                unreal.MaterialLibrary.set_scalar_parameter_value(
                    world,collection,unreal.Name(param),val)
        if actor:
            unreal.EditorLevelLibrary.destroy_actor(actor)
    verify_files(names,output,write_report=True)


def verify_files(names,output,write_report: bool) -> None:
    """只核验引擎真实输出文件和尺寸；像素质量由Host侧独立测试审查。"""
    details={}
    for name,path in names.items():
        if not path.is_file() or path.stat().st_size < 30000:
            raise RuntimeError("GPU文件缺失/异常小："+str(path))
        details[name]={"bytes":path.stat().st_size,"file":str(path)}
        print("MOSS_SNOW_GPU_FILE_PRESENT",name,str(path),path.stat().st_size)
    if not write_report:
        print("MOSS_SNOW_GPU_FILES_READ_ONLY_VERIFIED")
        return
    report=output/"MossSnowGPUFileEvidence.json"
    if report.exists():
        raise FileExistsError("已有不同次GPU证据，禁止覆盖："+str(report))
    report.write_text(json.dumps({"source":"UE5.8 SceneCapture2D",
        "captureFiles":details,
        "resultBoundaryZh":"仅确认RHI文件存在；像素质量与性能必须另外校验"},
        ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("MOSS_SNOW_GPU_FILE_CAPTURED",str(report))

if __name__=="__main__":
    run()
