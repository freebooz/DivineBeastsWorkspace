#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01 20m宏观去重复与亮度均衡UV的真实D3D12 GPU对比采集。

仅在全新/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview关卡
真实UE Editor中运行。SceneCapture2D、RenderTarget和光照调整全部临时，
finally恢复各灯光强度并销毁SceneCapture，不修改源QA地图、原Rock素材、
正式Village或PCG。用户人工最终签收独立于本自动化实际截图。
需要独占渲染Editor，若当前不是目标QA World必须拒绝。
"""
from __future__ import annotations

from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
OUT=(Path("E:/work/2026/DivineBeastsWorkspace/Game/Saved/Validation/"
          "TerrainGaea/CliffRock01Macro20261011"))
LIGHTS={
    "CliffReview_Sun":(2.2,1.0),
    "CliffReview_LeftSoftbox":(13000.,5000.),
    "CliffReview_RightSoftbox":(10000.,4000.),
    "CliffReview_Skylight":(.62,.36),
}
SCENES=(
    ("Balanced","CliffRock01Macro_20m_BalancedLights_ActualGPU.png"),
    ("Soft","CliffRock01Macro_20m_SoftLights_ActualGPU.png"),
)


def main()->None:
    import unreal  # type: ignore
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("此SceneCapture只允许在独立MacroReview地图，不接管用户打开的地图")
    actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    camera=actors.get("CliffReview_Camera_20m")
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("未找到20米Cliff审核摄像机")
    expected={
        "CliffReview_WorldAligned_20m":
            "/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro",
        "CliffReview_UV_20m":
            "/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview",
    }
    for label,pkg in expected.items():
        a=actors.get(label)
        mi=unreal.EditorAssetLibrary.load_asset(pkg)
        if not isinstance(a,unreal.StaticMeshActor) or not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("Cliff宏观板或QA实例资产缺失："+label)
        if a.get_editor_property("static_mesh_component").get_material(0)!=mi:
            raise RuntimeError("独立审核20m板实际未绑定宏观材质："+label)
    for _,filename in SCENES:
        if (OUT/filename).exists():
            raise FileExistsError("已有GPU审核图，禁止覆盖："+filename)
    comps={}
    previous={}
    for label in LIGHTS:
        actor=actors.get(label)
        if not actor:raise RuntimeError("审核灯光缺失："+label)
        comp=actor.get_editor_property("light_component")
        comps[label]=comp
        previous[label]=float(comp.get_editor_property("intensity"))
    cap=None
    try:
        cap=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
        if not isinstance(cap,unreal.SceneCapture2D):
            raise RuntimeError("UE没有真实SceneCapture2D Actor")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(target,unreal.TextureRenderTarget2D):
            raise RuntimeError("真实D3D12 RenderTarget2D不可用")
        component=cap.get_editor_property("capture_component2d")
        component.set_editor_property("texture_target",target)
        component.set_editor_property("fov_angle",69.0)
        component.set_editor_property("capture_every_frame",False)
        component.set_editor_property("capture_on_movement",False)
        component.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",-2.)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        component.set_editor_property("post_process_settings",post)
        component.set_editor_property("post_process_blend_weight",1.)
        OUT.mkdir(parents=True,exist_ok=True)
        for index,(tag,name) in enumerate(SCENES):
            for actorname,levels in LIGHTS.items():
                intensity=float(levels[index])
                light=comps[actorname]
                setter=getattr(light,"set_intensity",None)
                if setter:setter(intensity)
                else:light.set_editor_property("intensity",intensity)
                if abs(float(light.get_editor_property("intensity"))-intensity)>.02:
                    raise RuntimeError("UE材质比较照明不一致："+actorname)
            component.capture_scene()
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),name)
            photo=OUT/name
            if not photo.is_file() or photo.stat().st_size<60000:
                raise RuntimeError("真实D3D12场景截图未产生有效PNG："+name)
            print("CLIFF_MACRO_20M_GPU_CAPTURE_SAVED",tag,photo,"bytes",photo.stat().st_size)
    finally:
        for label,light in comps.items():
            fn=getattr(light,"set_intensity",None)
            if fn:fn(previous[label])
            else:light.set_editor_property("intensity",previous[label])
        if cap is not None:
            unreal.EditorLevelLibrary.destroy_actor(cap)
        for label,light in comps.items():
            if abs(float(light.get_editor_property("intensity"))-previous[label])>.02:
                raise RuntimeError("Cliff QA临时照明没有恢复："+label)
        print("CLIFF_MACRO_GPU_CAPTURE_ORIGINAL_LIGHTS_RESTORED")
    print("CLIFF_MACRO_20M_REAL_GPU_CAPTURE_PASS",
          "images=2","manualUserApprovalPending=True")


if __name__=="__main__":
    main()
